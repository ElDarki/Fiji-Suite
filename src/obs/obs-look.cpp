#include "obs-look.hpp"

#include "../model/theme-files.hpp"

#include <obs-module.h>
#include <util/bmem.h>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QSaveFile>

namespace fiji::obs {

namespace {

// Nunito Sans for everything people operate; Zen Maru Gothic (Sun) and Shippori Mincho B1 (Moon) for headings.
// The Mincho is cut down to Latin, punctuation and kana to keep the plugin small.
const char *const kFonts[] = {"fonts/NunitoSans-Regular.ttf", "fonts/NunitoSans-Bold.ttf", "fonts/ZenMaruGothic-Bold.ttf",
			      "fonts/ShipporiMinchoB1-Bold-subset.ttf"};

QString moduleFile(const char *name)
{
	char *path = obs_module_file(name);
	const QString result = path ? QString::fromUtf8(path) : QString();
	bfree(path);
	return result;
}

QByteArray readAll(const QString &file)
{
	QFile in(file);
	return in.open(QIODevice::ReadOnly) ? in.readAll() : QByteArray();
}

// relative path -> content of the theme files under a folder
QMap<QString, QByteArray> themeFiles(const QString &folder)
{
	QMap<QString, QByteArray> files;
	QDir root(folder);
	QDirIterator it(folder, QDir::Files, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		const QString path = it.next();
		const QString relative = root.relativeFilePath(path);
		if (model::isThemeFile(relative)) {
			files.insert(relative, readAll(path));
		}
	}
	return files;
}

void registerFonts()
{
	for (const char *name : kFonts) {
		const QString file = moduleFile(name);
		if (file.isEmpty() || QFontDatabase::addApplicationFont(file) < 0) {
			blog(LOG_WARNING, "[Fiji Suite] could not load the font %s", name);
		}
	}
}

} // namespace

bool installLook()
{
	registerFonts();

	// obs_module_file finds files, so ask for one theme and take its folder
	const QString shipped = QFileInfo(moduleFile("themes/Fiji_Sun.obt")).absolutePath();
	char *config = obs_module_get_config_path(obs_current_module(), "");
	const QString configDir = config ? QString::fromUtf8(config) : QString();
	bfree(config);
	if (moduleFile("themes/Fiji_Sun.obt").isEmpty() || configDir.isEmpty()) {
		return false;
	}
	const QString target = model::userThemesDir(configDir);
	const auto shippedFiles = themeFiles(shipped);
	const auto plan = model::planThemes(shippedFiles, themeFiles(target));
	int written = 0;
	for (const auto &relative : plan.write) {
		const QString path = QDir(target).filePath(relative);
		QDir().mkpath(QFileInfo(path).absolutePath());
		QSaveFile out(path);
		if (out.open(QIODevice::WriteOnly) && out.write(shippedFiles.value(relative)) >= 0 && out.commit()) {
			written++;
		}
	}
	for (const auto &relative : plan.remove) {
		QFile::remove(QDir(target).filePath(relative));
	}
	if (written) {
		blog(LOG_INFO, "[Fiji Suite] installed %d theme file(s) into %s", written, target.toUtf8().constData());
	}
	return plan.added > 0;
}

} // namespace fiji::obs
