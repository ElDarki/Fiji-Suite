#pragma once

// MODEL layer: release versions. "X.Y.Z" with an optional leading "v" and an optional "-suffix" (a pre-release, older
// than the same version without one) or "+metadata" (ignored). Qt Core only.

#include <QString>

namespace fiji::model {

struct ReleaseVersion {
	int part[3] = {0, 0, 0};
	bool prerelease = false;
	bool valid = false;
};

ReleaseVersion parseVersion(const QString &text);
// True when `latest` is newer than `current`; false when either is not a version.
bool isNewer(const QString &latest, const QString &current);
// "v1.2.3" -> "1.2.3"
QString bareVersion(const QString &tag);

} // namespace fiji::model
