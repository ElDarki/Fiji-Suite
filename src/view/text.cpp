#include "text.hpp"

namespace fiji::view {

namespace {
Translate &pluginTranslate()
{
	static Translate translate;
	return translate;
}
Translate &obsTranslate()
{
	static Translate translate;
	return translate;
}
} // namespace

void setTranslators(Translate plugin, Translate obs)
{
	pluginTranslate() = std::move(plugin);
	obsTranslate() = std::move(obs);
}

QString pluginText(const char *key)
{
	return pluginTranslate() ? pluginTranslate()(key) : QString::fromUtf8(key);
}

QString obsText(const char *key)
{
	return obsTranslate() ? obsTranslate()(key) : QString::fromUtf8(key);
}

} // namespace fiji::view
