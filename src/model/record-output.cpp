#include "record-output.hpp"

#include <QRegularExpression>

namespace fiji::model {

QStringList recordFormats()
{
	return {"mp4", "hybrid_mp4", "fragmented_mp4", "mov", "hybrid_mov", "fragmented_mov", "mkv", "ts", "mp3u8"};
}

RecordOutput newRecordOutput(bool backtrack, const QString &defaultName, const QString &filenamePattern)
{
	RecordOutput output;
	output.backtrack = backtrack;
	output.name = defaultName;
	output.filename = filenamePattern + QStringLiteral(" ") + defaultName;
	output.maxTimeSec = backtrack ? 30 : 0;
	return output;
}

QString filenameFollowingName(const QString &filename, const QString &pattern, const QString &oldName,
			      const QString &newName)
{
	if (filename.isEmpty() || filename == pattern || filename == pattern + QStringLiteral(" ") + oldName) {
		return pattern + QStringLiteral(" ") + newName;
	}
	return filename;
}

QString safeFilename(const QString &filename, const QString &platform)
{
	QString safe = filename;
	if (platform == QLatin1String("mac")) {
		safe.remove(QRegularExpression(QStringLiteral("[:]")));
	} else if (platform == QLatin1String("windows")) {
		safe.remove(QRegularExpression(QStringLiteral("[<>:\"\\|\\?\\*]")));
	}
	return safe;
}

bool recordOutputValid(const RecordOutput &output, const QStringList &otherNames)
{
	if (output.name.isEmpty() || otherNames.contains(output.name)) {
		return false;
	}
	if (output.path.isEmpty() || output.filename.isEmpty() || output.format.isEmpty()) {
		return false;
	}
	return !output.backtrack || output.maxSizeMb > 0 || output.maxTimeSec > 0;
}

} // namespace fiji::model
