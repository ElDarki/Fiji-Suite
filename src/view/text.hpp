#pragma once

// VIEW: translated text. Views do not include obs-module.h; the composition installs OBS's lookups once
// (setTranslators), and views ask by key. Without them (tests) a key is its own text.

#include <QString>
#include <functional>

namespace fiji::view {

using Translate = std::function<QString(const char *key)>;

// plugin: Fiji Suite's own strings (data/locale); obs: OBS's own strings (obs_frontend_get_locale_string).
void setTranslators(Translate plugin, Translate obs);
QString pluginText(const char *key);
QString obsText(const char *key);

} // namespace fiji::view
