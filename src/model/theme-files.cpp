#include "theme-files.hpp"

#include <QDir>
#include <QFileInfo>

namespace fiji::model {

QStringList retiredThemes()
{
	// 0.4.0 installed the themes as Yami styles; they are base themes now (same ids), so the old files go
	return {"Fiji_Sun.ovt", "Fiji_Moon.ovt"};
}

bool isThemeFile(const QString &path)
{
	return path.endsWith(QStringLiteral(".obt")) || path.endsWith(QStringLiteral(".ovt")) || path.endsWith(QStringLiteral(".svg"));
}

ThemePlan planThemes(const QMap<QString, QByteArray> &shipped, const QMap<QString, QByteArray> &installed)
{
	ThemePlan plan;
	for (auto it = shipped.constBegin(); it != shipped.constEnd(); ++it) {
		if (!isThemeFile(it.key())) {
			continue;
		}
		const auto there = installed.constFind(it.key());
		if (there != installed.constEnd() && *there == it.value()) {
			continue;
		}
		plan.write.append(it.key());
		if (there == installed.constEnd() && !it.key().endsWith(QStringLiteral(".svg"))) {
			plan.added++;
		}
	}
	for (const auto &old : retiredThemes()) {
		if (installed.contains(old) && !shipped.contains(old)) {
			plan.remove.append(old);
		}
	}
	return plan;
}

QString userThemesDir(const QString &pluginConfigDir)
{
	// <config>/obs-studio/plugin_config/fiji-suite[/] -> <config>/obs-studio/themes
	const QString pluginDir = QDir::cleanPath(pluginConfigDir);  // .../plugin_config/fiji-suite
	const QString obsDir = QFileInfo(QFileInfo(pluginDir).path()).path(); // .../obs-studio
	return QDir::cleanPath(obsDir + QStringLiteral("/themes"));
}

} // namespace fiji::model
