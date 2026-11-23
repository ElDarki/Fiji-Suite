#pragma once

#include <filesystem>
#include <string>

class QWidget;

// Fiji-chan's look for OBS: her fonts and the Fiji Sun / Fiji Moon themes (base themes, listed under Settings → Appearance → Theme).
//
// The fonts (data/fonts) are registered with Qt when the plugin loads, so the themes and the plugin's own pages
// can use them. The themes (data/themes/*.ovt) are copied into the user's obs-studio/themes folder, where OBS looks
// for custom themes. OBS reads that folder only when it starts, so a theme copied for the first time shows up in
// Settings → Appearance after the next restart. Only Fiji Suite's own theme files are written; nothing is switched
// for the user and no other file in that folder is touched.
namespace FijiLook {

void Install();

// After OBS has loaded: if this start added themes OBS has not listed yet, says so (once, non-modal).
void ShowRestartNotice(QWidget *parent);

// Copies every theme file (.obt, .ovt) from `from` into `to` (created if missing) when the copy there is missing or
// differs, and removes the theme files earlier versions installed that `from` no longer has. Returns how many files
// were written; `added` gets how many of those were new. Split out so it can be tested without OBS.
int CopyThemes(const std::filesystem::path &from, const std::filesystem::path &to, int *added = nullptr);

// The user's obs-studio/themes folder, worked out from this plugin's config folder
// (<config>/obs-studio/plugin_config/fiji-suite), so portable mode and Flatpak are handled the same way OBS does.
std::filesystem::path UserThemesDir(const std::filesystem::path &pluginConfigDir);

} // namespace FijiLook
