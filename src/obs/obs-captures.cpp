#include "obs-captures.hpp"

#include "../model/captures.hpp"

#include <obs.hpp>

#include <QMetaObject>
#include <QPointer>

namespace fiji::obs {

// One watched source: its weak reference and the handle its signals call back with.
struct ObsCaptures::Watched {
	ObsCaptures *owner;
	QString key;
	obs_weak_source_t *source;
};

namespace {

bool isCapture(obs_source_t *source)
{
	return source && model::isCaptureKind(QString::fromUtf8(obs_source_get_unversioned_id(source)));
}

} // namespace

ObsCaptures::ObsCaptures(model::Captures &captures, QObject *parent) : QObject(parent), captures_(captures)
{
	auto sh = obs_get_signal_handler();
	signal_handler_connect(sh, "source_create", source_create, this);
	signal_handler_connect(sh, "source_remove", source_remove, this);
}

ObsCaptures::~ObsCaptures()
{
	if (auto sh = obs_get_signal_handler()) {
		signal_handler_disconnect(sh, "source_create", source_create, this);
		signal_handler_disconnect(sh, "source_remove", source_remove, this);
	}
	for (const auto &key : watched_.keys()) {
		unwatch(key);
	}
}

void ObsCaptures::watch(obs_source_t *source)
{
	const QString key = QString::fromUtf8(obs_source_get_uuid(source));
	if (watched_.contains(key)) {
		return;
	}
	auto w = new Watched{this, key, obs_source_get_weak_source(source)};
	watched_.insert(key, w);
	auto sh = obs_source_get_signal_handler(source);
	signal_handler_connect(sh, "rename", rename, w);
	signal_handler_connect(sh, "hooked", hooked, w);
	signal_handler_connect(sh, "unhooked", unhooked, w);
	captures_.add(key, QString::fromUtf8(obs_source_get_name(source)));
}

void ObsCaptures::unwatch(const QString &key)
{
	Watched *w = watched_.take(key);
	if (!w) {
		return;
	}
	if (OBSSourceAutoRelease source = obs_weak_source_get_source(w->source)) {
		auto sh = obs_source_get_signal_handler(source);
		signal_handler_disconnect(sh, "rename", rename, w);
		signal_handler_disconnect(sh, "hooked", hooked, w);
		signal_handler_disconnect(sh, "unhooked", unhooked, w);
	}
	obs_weak_source_release(w->source);
	delete w;
	captures_.remove(key);
}

obs_source_t *ObsCaptures::sourceOf(const QString &key) const
{
	Watched *w = watched_.value(key);
	return w ? obs_weak_source_get_source(w->source) : nullptr;
}

QList<WindowChoice> ObsCaptures::windows(const QString &captureKey)
{
	QList<WindowChoice> choices;
	OBSSourceAutoRelease source = sourceOf(captureKey);
	if (!source) {
		return choices;
	}
	obs_properties_t *props = obs_source_properties(source);
	if (!props) {
		return choices;
	}
	obs_property_t *window = obs_properties_get(props, "window");
	const auto format = obs_property_list_format(window);
	const size_t count = obs_property_list_item_count(window);
	for (size_t i = 0; i < count; i++) {
		const QString label = QString::fromUtf8(obs_property_list_item_name(window, i));
		if (format == OBS_COMBO_FORMAT_STRING) {
			choices.append({label, QString::fromUtf8(obs_property_list_item_string(window, i))});
		} else if (format == OBS_COMBO_FORMAT_INT) {
			choices.append({label, QVariant::fromValue<qlonglong>(obs_property_list_item_int(window, i))});
		}
	}
	obs_properties_destroy(props);
	return choices;
}

void ObsCaptures::chooseWindow(const QString &captureKey, const QVariant &value)
{
	OBSSourceAutoRelease source = sourceOf(captureKey);
	if (!source) {
		return;
	}
	OBSDataAutoRelease settings = obs_data_create();
	if (value.typeId() == QMetaType::QString) {
		obs_data_set_string(settings, "window", value.toString().toUtf8().constData());
	} else {
		obs_data_set_int(settings, "window", value.toLongLong());
	}
	obs_source_update(source, settings);
}

// ---- OBS's signals (on OBS's threads); the model changes on the UI thread --------------------------------------
void ObsCaptures::source_create(void *data, calldata_t *cd)
{
	auto self = static_cast<ObsCaptures *>(data);
	auto source = static_cast<obs_source_t *>(calldata_ptr(cd, "source"));
	if (!isCapture(source)) {
		return;
	}
	obs_weak_source_t *handle = obs_source_get_weak_source(source); // released in the queued call
	QMetaObject::invokeMethod(
		self,
		[self, handle] {
			if (OBSSourceAutoRelease s = obs_weak_source_get_source(handle)) {
				self->watch(s);
			}
			obs_weak_source_release(handle);
		},
		Qt::QueuedConnection);
}

void ObsCaptures::source_remove(void *data, calldata_t *cd)
{
	auto self = static_cast<ObsCaptures *>(data);
	auto source = static_cast<obs_source_t *>(calldata_ptr(cd, "source"));
	if (!isCapture(source)) {
		return;
	}
	const QString key = QString::fromUtf8(obs_source_get_uuid(source));
	QMetaObject::invokeMethod(self, [self, key] { self->unwatch(key); }, Qt::QueuedConnection);
}

void ObsCaptures::hooked(void *data, calldata_t *cd)
{
	auto w = static_cast<Watched *>(data);
	const QString executable = QString::fromUtf8(calldata_string(cd, "executable"));
	const QString title = QString::fromUtf8(calldata_string(cd, "title"));
	auto self = w->owner;
	const QString key = w->key;
	QMetaObject::invokeMethod(self, [self, key, executable, title] { self->captures_.hooked(key, executable, title); },
				  Qt::QueuedConnection);
}

void ObsCaptures::unhooked(void *data, calldata_t *)
{
	auto w = static_cast<Watched *>(data);
	auto self = w->owner;
	const QString key = w->key;
	QMetaObject::invokeMethod(self, [self, key] { self->captures_.unhooked(key); }, Qt::QueuedConnection);
}

void ObsCaptures::rename(void *data, calldata_t *cd)
{
	auto w = static_cast<Watched *>(data);
	auto self = w->owner;
	const QString key = w->key;
	const QString name = QString::fromUtf8(calldata_string(cd, "new_name"));
	QMetaObject::invokeMethod(self, [self, key, name] { self->captures_.renamed(key, name); }, Qt::QueuedConnection);
}

} // namespace fiji::obs
