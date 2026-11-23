#pragma once

#include <filesystem>
#include <string>

// Fiji-chan's look for OBS: her fonts and the Fiji Sun / Fiji Moon themes.
//
// The fonts (data/fonts) are registered with Qt when the plugin loads, so the themes and the plugin's own pages
// can use them. The themes (data/themes/*.ovt) are copied into the user's obs-studio/themes folder, where OBS looks
// for custom themes. OBS reads that folder only when it starts, so a theme copied for the first time shows up in
// Settings → Appearance after the next restart. Only Fiji Suite's own theme files are written; nothing is switched
// for the user and no other file in that folder is touched.
namespace FijiLook {

void Install();

// Copies every .ovt file from `from` into `to` (created if missing) when the copy there is missing or differs.
// Returns how many files were written. Split out so it can be tested without OBS.
int CopyThemes(const std::filesystem::path &from, const std::filesystem::path &to);

// The user's obs-studio/themes folder, worked out from this plugin's config folder
// (<config>/obs-studio/plugin_config/fiji-suite), so portable mode and Flatpak are handled the same way OBS does.
std::filesystem::path UserThemesDir(const std::filesystem::path &pluginConfigDir);

} // namespace FijiLook
