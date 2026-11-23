#include "fiji-look.hpp"

#include <obs-module.h>
#include <util/bmem.h>

#include <QFontDatabase>
#include <QMessageBox>
#include <QString>

#include <fstream>
#include <iterator>
#include <system_error>

namespace fs = std::filesystem;

namespace FijiLook {

namespace {

// Theme files earlier versions installed and this one no longer ships. Only these names are ever removed.
const char *const RETIRED[] = {"Fiji_Sun.ovt", "Fiji_Moon.ovt"};

bool restartNeeded = false;

// Nunito Sans for everything people operate; Zen Maru Gothic (Sun) and Shippori Mincho B1 (Moon) for headings.
// The Mincho is cut down to Latin, punctuation and kana to keep the plugin small.
const char *const FONTS[] = {"fonts/NunitoSans-Regular.ttf", "fonts/NunitoSans-Bold.ttf", "fonts/ZenMaruGothic-Bold.ttf",
			     "fonts/ShipporiMinchoB1-Bold-subset.ttf"};

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

int CopyThemes(const fs::path &from, const fs::path &to, int *added)
{
	if (added)
		*added = 0;
	std::error_code ec;
	if (!fs::is_directory(from, ec))
		return 0;
	fs::create_directories(to, ec);
	if (ec)
		return 0;

	int written = 0;
	// the theme files, and the folders of icons they use (FijiSun/, FijiMoon/)
	for (auto it = fs::recursive_directory_iterator(from, ec); !ec && it != fs::recursive_directory_iterator();
	     it.increment(ec)) {
		const auto &entry = *it;
		const auto ext = entry.path().extension();
		if (!entry.is_regular_file() || (ext != ".obt" && ext != ".ovt" && ext != ".svg"))
			continue;
		const fs::path target = to / fs::relative(entry.path(), from, ec);
		const std::string content = ReadAll(entry.path());
		const bool existed = fs::exists(target, ec);
		if (existed && ReadAll(target) == content)
			continue;
		fs::create_directories(target.parent_path(), ec);
		std::ofstream out(target, std::ios::binary | std::ios::trunc);
		out << content;
		if (out.good()) {
			written++;
			if (!existed && added && ext != ".svg")
				(*added)++;
		}
	}

	// 0.4.0 installed the themes as Yami styles; they are base themes now (same ids), so the old files go
	for (const char *old : RETIRED) {
		const fs::path target = to / old;
		if (fs::exists(target, ec) && !fs::exists(from / old, ec))
			fs::remove(target, ec);
	}
	return written;
}

void Install()
{
	RegisterFonts();

	// obs_module_file finds files, so ask for one theme and take its folder
	const fs::path themes = ModuleFile("themes/Fiji_Sun.obt").parent_path();
	char *config = obs_module_get_config_path(obs_current_module(), "");
	if (themes.empty() || !config) {
		bfree(config);
		return;
	}
	const fs::path target = UserThemesDir(fs::u8path(config));
	bfree(config);

	int added = 0;
	if (int n = CopyThemes(themes, target, &added))
		blog(LOG_INFO, "[Fiji Suite] installed %d theme file(s) into %s", n, target.u8string().c_str());
	// OBS lists themes only when it starts, before plugins load: new ones show up after a restart
	restartNeeded = added > 0;
}

void ShowRestartNotice(QWidget *parent)
{
	if (!restartNeeded)
		return;
	restartNeeded = false;
	auto box = new QMessageBox(QMessageBox::Information, QString::fromUtf8(obs_module_text("ThemesAddedTitle")),
				   QString::fromUtf8(obs_module_text("ThemesAddedText")), QMessageBox::Ok, parent);
	box->setAttribute(Qt::WA_DeleteOnClose);
	box->setModal(false);
	box->show();
}

} // namespace FijiLook
