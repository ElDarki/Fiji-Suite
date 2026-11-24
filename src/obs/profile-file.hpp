#pragma once

// Fiji Suite's settings file in the current OBS profile's folder (fiji-suite.json), read and written the way OBS does
// (obs_data JSON, a temporary file and a .bak copy). Free of obs.h.

#include <QJsonObject>
#include <QString>
#include <optional>

namespace fiji::obs {

// <current profile folder>/fiji-suite.json, or empty when OBS has no current profile.
QString CurrentProfileFile();
// The file's content, or nothing when there is no file (or it and its .bak cannot be read).
std::optional<QJsonObject> LoadProfileFile(const QString &path);
bool SaveProfileFile(const QString &path, const QJsonObject &document);
// Where OBS records for the current profile (Advanced or Simple output mode), or empty.
QString ProfileRecordPath();

} // namespace fiji::obs
