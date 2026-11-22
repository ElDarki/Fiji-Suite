#pragma once

#include <string>

// Asks GitHub once, when OBS has finished loading, whether a newer Fiji Suite release exists, and if so
// offers it in a small window. One request per OBS start, on a background thread; nothing more runs
// afterwards. Off when "Check for updates when OBS starts" is unticked in the settings.
namespace UpdateCheck {

void Start();
void Stop();

// True when version `latest` is newer than `current`. Both are "X.Y.Z" with an optional leading "v" and
// an optional "-suffix"; a version with a suffix (a pre-release) is older than the same one without.
bool IsNewer(const std::string &latest, const std::string &current);

} // namespace UpdateCheck
