#include "fiji-look.hpp"

#include <obs-module.h>
#include <util/bmem.h>

#include <QFontDatabase>
#include <QString>

#include <fstream>
#include <iterator>
#include <system_error>

namespace fs = std::filesystem;

namespace FijiLook {

namespace {

const char *const FONTS[] = {"fonts/NunitoSans-Regular.ttf", "fonts/NunitoSans-Bold.ttf",
			     "fonts/ZenMaruGothic-Bold.ttf"};

std::string ReadAll(const fs::path &file)
{
	std::ifstream in(file, std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

fs::path ModuleFile(const char *name)
{
	char *path = obs_module_file(name);
	if (!path)
		return {};
	fs::path result = fs::u8path(path);
	bfree(path);
	return result;
}

void RegisterFonts()
{
	for (const char *name : FONTS) {
		const fs::path file = ModuleFile(name);
		if (file.empty() || QFontDatabase::addApplicationFont(QString::fromStdString(file.u8string())) < 0)
			blog(LOG_WARNING, "[Fiji Suite] could not load the font %s", name);
	}
}

} // namespace

fs::path UserThemesDir(const fs::path &pluginConfigDir)
{
	// <config>/obs-studio/plugin_config/fiji-suite[/] → <config>/obs-studio/themes
	fs::path dir = pluginConfigDir;
	if (!dir.has_filename())
		dir = dir.parent_path();
	return dir.parent_path().parent_path() / "themes";
}

int CopyThemes(const fs::path &from, const fs::path &to)
{
	std::error_code ec;
	if (!fs::is_directory(from, ec))
		return 0;
	fs::create_directories(to, ec);
	if (ec)
		return 0;

	int written = 0;
	for (const auto &entry : fs::directory_iterator(from, ec)) {
		if (!entry.is_regular_file() || entry.path().extension() != ".ovt")
			continue;
		const fs::path target = to / entry.path().filename();
		const std::string content = ReadAll(entry.path());
		if (fs::exists(target, ec) && ReadAll(target) == content)
			continue;
		std::ofstream out(target, std::ios::binary | std::ios::trunc);
		out << content;
		if (out.good())
			written++;
	}
	return written;
}

void Install()
{
	RegisterFonts();

	// obs_module_file finds files, so ask for one theme and take its folder
	const fs::path themes = ModuleFile("themes/Fiji_Sun.ovt").parent_path();
	char *config = obs_module_get_config_path(obs_current_module(), "");
	if (themes.empty() || !config) {
		bfree(config);
		return;
	}
	const fs::path target = UserThemesDir(fs::u8path(config));
	bfree(config);

	if (int n = CopyThemes(themes, target))
		blog(LOG_INFO, "[Fiji Suite] installed %d theme file(s) into %s", n, target.u8string().c_str());
}

} // namespace FijiLook
