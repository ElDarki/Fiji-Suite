#pragma once

// The outside world, as the update check needs it. Free of obs.h and curl.

#include <QString>
#include <functional>

namespace fiji::obs {

// The newest published release (not a draft, not a pre-release).
class ReleaseFeed {
public:
	virtual ~ReleaseFeed() = default;
	// Asks once, in the background. `done` runs on the UI thread with the release's tag ("v1.2.3"), or an empty
	// string when the answer could not be had (offline, GitHub refused, an odd answer); it never runs after stop().
	virtual void fetchLatest(std::function<void(const QString &tag)> done) = 0;
	virtual void stop() = 0;
};

// The user's choices about update checks (OBS's user config).
class UpdateSettings {
public:
	virtual ~UpdateSettings() = default;
	virtual bool checkOnStart() const = 0;
	virtual QString skippedVersion() const = 0;
	virtual void skipVersion(const QString &version) = 0;
};

} // namespace fiji::obs
