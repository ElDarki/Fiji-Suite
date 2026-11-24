#pragma once

// MODEL: a Fiji Suite recording or backtrack (replay buffer) output as its dialog edits it, and the dialog's rules.
// Stored in the profile's output list as {"type": "record"|"backtrack", "name", "path", "filename", "format",
// "max_size_mb", "max_time_sec"}. Qt Core only.

#include <QString>
#include <QStringList>

namespace fiji::model {

struct RecordOutput {
	bool backtrack = false;
	QString name;
	QString path;
	QString filename; // OBS's filename formatting ("%CCYY-%MM-%DD %hh-%mm-%ss Name")
	QString format = QStringLiteral("hybrid_mp4");
	qint64 maxSizeMb = 0; // splits a recording / limits a backtrack; 0 = off
	qint64 maxTimeSec = 0;
};

// The container formats a recording can use, in the order the dialog lists them.
QStringList recordFormats();

// A new output's values: its default name, the filename pattern followed by that name, and 30 s for a backtrack.
RecordOutput newRecordOutput(bool backtrack, const QString &defaultName, const QString &filenamePattern);

// The filename after the name changed from oldName to newName: it follows the name while it is still the pattern,
// the pattern plus the old name, or empty; a filename the user wrote stays.
QString filenameFollowingName(const QString &filename, const QString &pattern, const QString &oldName,
			      const QString &newName);

// The filename without the characters the platform does not allow ("mac", "windows", anything else: unchanged).
QString safeFilename(const QString &filename, const QString &platform);

// Whether the dialog may save: a name no other output has, a folder, a filename and a format, and for a backtrack a
// length or size limit.
bool recordOutputValid(const RecordOutput &output, const QStringList &otherNames);

} // namespace fiji::model
