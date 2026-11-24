#include "theme-files.hpp"

#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

namespace fiji::model::themes {

namespace {

// Theme files earlier versions installed and this one no longer ships. Only these names are ever removed.
const char *const RETIRED[] = {"Fiji_Sun.ovt", "Fiji_Moon.ovt"};

std::string ReadAll(const fs::path &file)
{
	std::ifstream in(file, std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
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

} // namespace fiji::model::themes
