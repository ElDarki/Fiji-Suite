#include "update-check.hpp"

#include "../version.h"
#include <atomic>
#include <cctype>
#include <curl/curl.h>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.hpp>
#include <QAbstractButton>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>
#include <thread>
#include <util/config-file.h>

namespace {

// GitHub answers with the newest release that is neither a draft nor a pre-release.
const char *latestReleaseApi = "https://api.github.com/repos/ElDarki/Fiji-Suite/releases/latest";
// Where the "Download" button goes. Fixed here, so nothing from the response is ever opened.
const char *latestReleasePage = "https://github.com/ElDarki/Fiji-Suite/releases/latest";
// The release JSON is a few kilobytes; anything far bigger is not what we asked for.
const size_t maxBody = 512 * 1024;

std::thread worker;
std::atomic<bool> stopping{false};
bool started = false; // UI thread only

struct Version {
	int part[3] = {0, 0, 0};
	bool prerelease = false;
	bool valid = false;
};

Version parse(const std::string &text)
{
	Version v;
	size_t i = 0;
	if (i < text.size() && (text[i] == 'v' || text[i] == 'V')) {
		i++;
	}
	for (int k = 0; k < 3; k++) {
		if (i >= text.size() || !std::isdigit(static_cast<unsigned char>(text[i]))) {
			return v;
		}
		int n = 0;
		while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
			n = n * 10 + (text[i] - '0');
			if (n > 1000000) {
				return v;
			}
			i++;
		}
		v.part[k] = n;
		if (k < 2) {
			if (i >= text.size() || text[i] != '.') {
				return v;
			}
			i++;
		}
	}
	if (i < text.size()) {
		if (text[i] != '-' && text[i] != '+') {
			return v;
		}
		v.prerelease = text[i] == '-'; // "+..." is build metadata and does not change the version
	}
	v.valid = true;
	return v;
}

size_t write_body(char *data, size_t size, size_t count, void *user)
{
	auto body = static_cast<std::string *>(user);
	const size_t n = size * count;
	if (body->size() + n > maxBody) {
		return 0; // makes curl stop with an error
	}
	body->append(data, n);
	return n;
}

int keep_going(void *, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
	return stopping ? 1 : 0; // non-zero aborts the transfer, so OBS can close without waiting
}

QString text(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

// UI thread.
void offer(const QString &latest)
{
	if (stopping) {
		return;
	}
	auto user_config = obs_frontend_get_user_config();
	if (user_config) {
		const char *skipped = config_get_string(user_config, "FijiSuite", "SkippedVersion");
		if (skipped && latest == QString::fromUtf8(skipped)) {
			blog(LOG_INFO, "[Fiji Suite] Version %s was skipped, not offering it", skipped);
			return;
		}
	}

	auto window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	auto box = new QMessageBox(window);
	box->setAttribute(Qt::WA_DeleteOnClose);
	box->setWindowModality(Qt::NonModal);
	box->setIcon(QMessageBox::Information);
	box->setWindowTitle(text("UpdateTitle"));
	box->setText(text("UpdateText").arg(latest, QStringLiteral(PROJECT_VERSION)));
	box->setInformativeText(text("UpdateInfo"));
	auto download = box->addButton(text("UpdateDownload"), QMessageBox::AcceptRole);
	auto skip = box->addButton(text("UpdateSkip"), QMessageBox::DestructiveRole);
	auto later = box->addButton(text("UpdateLater"), QMessageBox::RejectRole);
	box->setDefaultButton(download);
	box->setEscapeButton(later);
	QObject::connect(box, &QMessageBox::buttonClicked, box, [download, skip, latest](QAbstractButton *button) {
		if (button == download) {
			QDesktopServices::openUrl(QUrl(QString::fromUtf8(latestReleasePage)));
		} else if (button == skip) {
			if (auto config = obs_frontend_get_user_config()) {
				config_set_string(config, "FijiSuite", "SkippedVersion", latest.toUtf8().constData());
				config_save_safe(config, "tmp", "bak");
			}
		}
	});
	box->show();
}

void run()
{
	std::string body;
	CURL *curl = curl_easy_init();
	if (!curl) {
		return;
	}
	curl_slist *headers = curl_slist_append(nullptr, "Accept: application/vnd.github+json");
	curl_easy_setopt(curl, CURLOPT_URL, latestReleaseApi);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "fiji-suite/" PROJECT_VERSION);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, keep_going);
	const CURLcode res = curl_easy_perform(curl);
	long status = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
	curl_easy_cleanup(curl);
	curl_slist_free_all(headers);

	if (stopping) {
		return;
	}
	if (res != CURLE_OK) {
		blog(LOG_INFO, "[Fiji Suite] Update check failed: %s", curl_easy_strerror(res));
		return;
	}
	if (status != 200) {
		blog(LOG_INFO, "[Fiji Suite] Update check failed: HTTP %ld", status);
		return;
	}
	OBSDataAutoRelease data = obs_data_create_from_json(body.c_str());
	if (!data) {
		blog(LOG_INFO, "[Fiji Suite] Update check failed: unreadable answer");
		return;
	}
	const std::string tag = obs_data_get_string(data, "tag_name");
	if (!parse(tag).valid) {
		blog(LOG_INFO, "[Fiji Suite] Update check failed: unexpected tag '%s'", tag.c_str());
		return;
	}
	if (!UpdateCheck::IsNewer(tag, PROJECT_VERSION)) {
		blog(LOG_INFO, "[Fiji Suite] Up to date (latest release %s)", tag.c_str());
		return;
	}
	QString latest = QString::fromStdString(tag);
	if (latest.startsWith(QLatin1Char('v')) || latest.startsWith(QLatin1Char('V'))) {
		latest.remove(0, 1);
	}
	blog(LOG_INFO, "[Fiji Suite] Version %s is available (this is %s)", latest.toUtf8().constData(),
	     PROJECT_VERSION);
	QMetaObject::invokeMethod(QCoreApplication::instance(), [latest] { offer(latest); }, Qt::QueuedConnection);
}

} // namespace

bool UpdateCheck::IsNewer(const std::string &latest, const std::string &current)
{
	const Version a = parse(latest);
	const Version b = parse(current);
	if (!a.valid || !b.valid) {
		return false;
	}
	for (int k = 0; k < 3; k++) {
		if (a.part[k] != b.part[k]) {
			return a.part[k] > b.part[k];
		}
	}
	return b.prerelease && !a.prerelease;
}

void UpdateCheck::Start()
{
	// One check per OBS start, however often this is called.
	if (started) {
		return;
	}
	started = true;
	auto user_config = obs_frontend_get_user_config();
	if (user_config && !config_get_bool(user_config, "FijiSuite", "CheckForUpdates")) {
		blog(LOG_INFO, "[Fiji Suite] Update check is off");
		return;
	}
	stopping = false;
	worker = std::thread(run);
}

void UpdateCheck::Stop()
{
	stopping = true;
	if (worker.joinable()) {
		worker.join();
	}
}
