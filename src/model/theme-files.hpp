#pragma once

// MODEL layer: keeping the user's themes folder in step with the themes the plugin ships (Fiji Sun / Fiji Moon, the
// .obt/.ovt files and the .svg icons they use). Pure rules over file names and contents; obs::installLook reads and
// writes the files. Qt Core only.

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QStringList>

namespace fiji::model {

struct ThemePlan {
	QStringList write;  // relative paths to (over)write from the shipped copy
	QStringList remove; // relative paths of retired theme files to delete
	int added = 0;      // themes (not icons) the user did not have yet: OBS lists them only after a restart
	QStringList newThemes; // which those are (relative paths), so a failed write does not count as added
};

// Theme files the plugin ever shipped and no longer does. Only these are ever removed from the user's folder.
QStringList retiredThemes();

// shipped / installed: relative path -> content, for the theme files (.obt, .ovt, .svg) on each side.
ThemePlan planThemes(const QMap<QString, QByteArray> &shipped, const QMap<QString, QByteArray> &installed);

// The user's obs-studio/themes folder, from this plugin's config folder (<config>/obs-studio/plugin_config/fiji-suite),
// so portable mode and Flatpak are handled the way OBS handles them.
QString userThemesDir(const QString &pluginConfigDir);

bool isThemeFile(const QString &path);

} // namespace fiji::model
