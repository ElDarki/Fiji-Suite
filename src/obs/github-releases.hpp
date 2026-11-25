#pragma once

// ReleaseFeed over GitHub's API (libcurl, which ships with OBS), and UpdateSettings in OBS's user config.

#include "update-port.hpp"

#include <QObject>
#include <atomic>
#include <thread>

namespace fiji::obs {

class GitHubReleases final : public QObject, public ReleaseFeed {
	Q_OBJECT

public:
	explicit GitHubReleases(QString userAgent, QObject *parent = nullptr);
	~GitHubReleases() override;

	void fetchLatest(std::function<void(const QString &tag)> done) override;
	void stop() override;

private:
	QString userAgent_;
	std::thread worker_;
	std::atomic<bool> stopping_{false};
};

class FrontendUpdateSettings final : public UpdateSettings {
public:
	bool checkOnStart() const override;
	QString skippedVersion() const override;
	void skipVersion(const QString &version) override;
};

} // namespace fiji::obs
