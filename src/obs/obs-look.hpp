#pragma once

// Fiji-chan's look for OBS, put in place when the plugin loads: her fonts (data/fonts) are registered with Qt so the
// themes and the plugin's pages can use them, and the Fiji Sun / Fiji Moon themes (data/themes) are copied into the
// user's obs-studio/themes folder (model::planThemes decides what). Only Fiji Suite's own theme files are written;
// nothing is switched for the user and no other file there is touched.

namespace fiji::obs {

// Returns true when this start added themes OBS has not listed yet: OBS reads its themes folder only when it starts,
// so they show up in Settings -> Appearance after the next restart.
bool installLook();

} // namespace fiji::obs
