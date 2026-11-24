#pragma once

// Fiji-chan's look for OBS, put in place when the plugin loads: her fonts (data/fonts) are registered with Qt so the
// themes and the plugin's own pages can use them, and the Fiji Sun / Fiji Moon themes are copied into the user's
// themes folder (model::themes). OBS lists themes only when it starts, before plugins load, so new ones show up after
// a restart: the result says how many are new.

namespace fiji::obs {

// Returns how many themes were added for the first time (0 when they were all there already).
int InstallLook();

} // namespace fiji::obs
