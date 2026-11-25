#include "github-releases.hpp"

#include <curl/curl.h>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/config-file.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QPointer>

namespace fiji::obs {

namespace {

// GitHub answers with the newest release that is neither a draft nor a pre-release.
const char *kLatestReleaseApi = "https://api.github.com/repos/ElDarki/Fiji-Suite/releases/latest";
// The release JSON is a few kilobytes; anything far bigger is not what we asked for.
const size_t kMaxBody = 512 * 1024;

size_t write_body(char *data, size_t size, size_t count, void *user)
{
	auto body = static_cast<std::string *>(user);
	const size_t n = size * count;
	if (body->size() + n > kMaxBody) {
		return 0; // makes curl stop with an error
	}
	body->append(data, n);
	return n;
}

int keep_going(void *stopping, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
	return static_cast<std::atomic<bool> *>(stopping)->load() ? 1 : 0; // aborts, so OBS can close without waiting
}

// The release's tag, or empty with the reason logged.
QString fetch(const std::string &userAgent, std::atomic<bool> &stopping)
{
	std::string body;
	CURL *curl = curl_easy_init();
	if (!curl) {
		return {};
	}
	curl_slist *headers = curl_slist_append(nullptr, "Accept: application/vnd.github+json");
	curl_easy_setopt(curl, CURLOPT_URL, kLatestReleaseApi);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, keep_going);
	curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &stopping);
	const CURLcode res = curl_easy_perform(curl);
	long status = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
	curl_easy_cleanup(curl);
	curl_slist_free_all(headers);
	if (stopping) {
		return {};
	}
	if (res != CURLE_OK || status != 200) {
		blog(LOG_INFO, "[Fiji Suite] Update check failed: %s (HTTP %ld)", curl_easy_strerror(res), status);
		return {};
	}
	const auto tag = QJsonDocument::fromJson(QByteArray::fromStdString(body)).object().value("tag_name").toString();
	if (tag.isEmpty()) {
		blog(LOG_INFO, "[Fiji Suite] Update check failed: unreadable answer");
	}
	return tag;
}

} // namespace

GitHubReleases::GitHubReleases(QString userAgent, QObject *parent) : QObject(parent), userAgent_(std::move(userAgent)) {}

GitHubReleases::~GitHubReleases()
{
	stop();
}

void GitHubReleases::fetchLatest(std::function<void(const QString &tag)> done)
{
	stop();
	stopping_ = false;
	QPointer<GitHubReleases> self(this);
	worker_ = std::thread([this, self, done = std::move(done), agent = userAgent_.toStdString()] {
		const QString tag = fetch(agent, stopping_);
		if (stopping_) {
			return;
		}
		QMetaObject::invokeMethod(
			this,
			[self, done, tag] {
				if (self && !self->stopping_) {
					done(tag);
				}
			},
			Qt::QueuedConnection);
	});
}

void GitHubReleases::stop()
{
	stopping_ = true;
	if (worker_.joinable()) {
		worker_.join();
	}
}

bool FrontendUpdateSettings::checkOnStart() const
{
	auto config = obs_frontend_get_user_config();
	return !config || config_get_bool(config, "FijiSuite", "CheckForUpdates");
}

QString FrontendUpdateSettings::skippedVersion() const
{
	auto config = obs_frontend_get_user_config();
	const char *skipped = config ? config_get_string(config, "FijiSuite", "SkippedVersion") : nullptr;
	return QString::fromUtf8(skipped ? skipped : "");
}

void FrontendUpdateSettings::skipVersion(const QString &version)
{
	if (auto config = obs_frontend_get_user_config()) {
		config_set_string(config, "FijiSuite", "SkippedVersion", version.toUtf8().constData());
		config_save_safe(config, "tmp", "bak");
	}
}

} // namespace fiji::obs
