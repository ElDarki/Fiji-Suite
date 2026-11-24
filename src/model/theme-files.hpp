#pragma once

// MODEL layer: Fiji Sun and Fiji Moon as files. The themes (data/themes: .obt, .ovt and their .svg icons) are copied
// into the user's obs-studio/themes folder, where OBS looks for custom themes. Only Fiji Suite's own theme files are
// written or removed; nothing else in that folder is touched. Standard library only.

#include <filesystem>

namespace fiji::model::themes {

// Copies every theme file from `from` into `to` (created if missing) when the copy there is missing or differs, and
// removes the theme files earlier versions installed that `from` no longer has. Returns how many files were written;
// `added` gets how many of those were new themes (icons not counted).
int CopyThemes(const std::filesystem::path &from, const std::filesystem::path &to, int *added = nullptr);

// The user's obs-studio/themes folder, worked out from this plugin's config folder
// (<config>/obs-studio/plugin_config/fiji-suite), so portable mode and Flatpak are handled the same way OBS does.
std::filesystem::path UserThemesDir(const std::filesystem::path &pluginConfigDir);

} // namespace fiji::model::themes
