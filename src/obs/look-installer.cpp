#include "look-installer.hpp"

#include "../model/theme-files.hpp"

#include <obs-module.h>
#include <util/bmem.h>

#include <QFontDatabase>
#include <QString>

namespace fs = std::filesystem;

namespace fiji::obs {

namespace {

// Nunito Sans for everything people operate; Zen Maru Gothic (Sun) and Shippori Mincho B1 (Moon) for headings.
// The Mincho is cut down to Latin, punctuation and kana to keep the plugin small.
const char *const FONTS[] = {"fonts/NunitoSans-Regular.ttf", "fonts/NunitoSans-Bold.ttf", "fonts/ZenMaruGothic-Bold.ttf",
			     "fonts/ShipporiMinchoB1-Bold-subset.ttf"};

fs::path ModuleFile(const char *name)
{
	char *path = obs_module_file(name);
	if (!path)
		return {};
	fs::path result = fs::u8path(path);
	bfree(path);
	return result;
}

} // namespace

int InstallLook()
{
	for (const char *name : FONTS) {
		const fs::path file = ModuleFile(name);
		if (file.empty() || QFontDatabase::addApplicationFont(QString::fromStdString(file.u8string())) < 0)
			blog(LOG_WARNING, "[Fiji Suite] could not load the font %s", name);
	}

	// obs_module_file finds files, so ask for one theme and take its folder
	const fs::path themes = ModuleFile("themes/Fiji_Sun.obt").parent_path();
	char *config = obs_module_get_config_path(obs_current_module(), "");
	if (themes.empty() || !config) {
		bfree(config);
		return 0;
	}
	const fs::path target = model::themes::UserThemesDir(fs::u8path(config));
	bfree(config);

	int added = 0;
	if (int n = model::themes::CopyThemes(themes, target, &added))
		blog(LOG_INFO, "[Fiji Suite] installed %d theme file(s) into %s", n, target.u8string().c_str());
	return added;
}

} // namespace fiji::obs
