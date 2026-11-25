#include "update-checker.hpp"

#include "../model/release-version.hpp"
#include "../obs/update-port.hpp"

namespace fiji::controller {

UpdateChecker::UpdateChecker(obs::ReleaseFeed &feed, obs::UpdateSettings &settings, QString current, QObject *parent)
	: QObject(parent),
	  feed_(feed),
	  settings_(settings),
	  current_(std::move(current))
{
}

void UpdateChecker::start()
{
	if (started_) {
		return;
	}
	started_ = true;
	if (!settings_.checkOnStart()) {
		outcome_ = QStringLiteral("off");
		return;
	}
	feed_.fetchLatest([this](const QString &tag) { found(tag); });
}

void UpdateChecker::stop()
{
	feed_.stop();
}

void UpdateChecker::found(const QString &tag)
{
	if (!model::parseVersion(tag).valid) {
		outcome_ = QStringLiteral("failed");
		return;
	}
	if (!model::isNewer(tag, current_)) {
		outcome_ = QStringLiteral("up to date");
		return;
	}
	const QString latest = model::bareVersion(tag);
	if (latest == settings_.skippedVersion()) {
		outcome_ = QStringLiteral("skipped");
		return;
	}
	outcome_ = QStringLiteral("offered");
	emit offerUpdate(latest, current_);
}

void UpdateChecker::skip(const QString &version)
{
	settings_.skipVersion(version);
}

} // namespace fiji::controller
