#include "obs-capture-sources.hpp"

#include <obs.hpp>

#include <cstring>

namespace fiji::obs {

namespace {
bool isCapture(obs_source_t *source)
{
	const char *id = source ? obs_source_get_unversioned_id(source) : nullptr;
	return id && (strcmp(id, "game_capture") == 0 || strcmp(id, "window_capture") == 0);
}

QString uuidOf(obs_source_t *source)
{
	return QString::fromUtf8(obs_source_get_uuid(source));
}

obs_source_t *sourceOf(obs_weak_source_t *weak)
{
	return weak ? obs_weak_source_get_source(weak) : nullptr;
}
} // namespace

ObsCaptureSources::~ObsCaptureSources()
{
	if (listening_) {
		if (auto sh = obs_get_signal_handler()) {
			signal_handler_disconnect(sh, "source_create", sourceCreated, this);
			signal_handler_disconnect(sh, "source_remove", sourceRemoved, this);
		}
	}
	std::lock_guard lock(mutex_);
	for (auto &tracked : tracked_) {
		untrack(*tracked);
	}
	tracked_.clear();
}

void ObsCaptureSources::listen(Listener listener)
{
	if (listening_) {
		return;
	}
	listener_ = std::move(listener);
	listening_ = true;
	auto sh = obs_get_signal_handler();
	signal_handler_connect(sh, "source_create", sourceCreated, this);
	signal_handler_connect(sh, "source_remove", sourceRemoved, this);
}

void ObsCaptureSources::sourceCreated(void *param, calldata_t *cd)
{
	auto self = static_cast<ObsCaptureSources *>(param);
	auto source = static_cast<obs_source_t *>(calldata_ptr(cd, "source"));
	if (!isCapture(source)) {
		return;
	}
	auto tracked = std::make_shared<Tracked>(Tracked{self, uuidOf(source), obs_source_get_weak_source(source)});
	{
		std::lock_guard lock(self->mutex_);
		self->tracked_.insert(tracked->uuid, tracked);
	}
	auto sh = obs_source_get_signal_handler(source);
	signal_handler_connect(sh, "rename", renamed, tracked.get());
	signal_handler_connect(sh, "hooked", hooked, tracked.get());
	signal_handler_connect(sh, "unhooked", unhooked, tracked.get());
	if (self->listener_.added) {
		self->listener_.added(tracked->uuid, QString::fromUtf8(obs_source_get_name(source)));
	}
}

void ObsCaptureSources::sourceRemoved(void *param, calldata_t *cd)
{
	auto self = static_cast<ObsCaptureSources *>(param);
	auto source = static_cast<obs_source_t *>(calldata_ptr(cd, "source"));
	if (!isCapture(source)) {
		return;
	}
	const QString uuid = uuidOf(source);
	{
		std::lock_guard lock(self->mutex_);
		auto found = self->tracked_.find(uuid);
		if (found == self->tracked_.end()) {
			return;
		}
		self->untrack(**found);
		self->tracked_.erase(found);
	}
	if (self->listener_.removed) {
		self->listener_.removed(uuid);
	}
}

void ObsCaptureSources::untrack(Tracked &tracked)
{
	if (auto source = sourceOf(tracked.weak)) {
		auto sh = obs_source_get_signal_handler(source);
		signal_handler_disconnect(sh, "rename", renamed, &tracked);
		signal_handler_disconnect(sh, "hooked", hooked, &tracked);
		signal_handler_disconnect(sh, "unhooked", unhooked, &tracked);
		obs_source_release(source);
	}
	obs_weak_source_release(tracked.weak);
	tracked.weak = nullptr;
}

void ObsCaptureSources::renamed(void *param, calldata_t *cd)
{
	auto tracked = static_cast<Tracked *>(param);
	if (tracked->self->listener_.renamed) {
		tracked->self->listener_.renamed(tracked->uuid, QString::fromUtf8(calldata_string(cd, "new_name")));
	}
}

void ObsCaptureSources::hooked(void *param, calldata_t *cd)
{
	auto tracked = static_cast<Tracked *>(param);
	if (tracked->self->listener_.hooked) {
		tracked->self->listener_.hooked(tracked->uuid, QString::fromUtf8(calldata_string(cd, "executable")),
						QString::fromUtf8(calldata_string(cd, "title")));
	}
}

void ObsCaptureSources::unhooked(void *param, calldata_t *)
{
	auto tracked = static_cast<Tracked *>(param);
	if (tracked->self->listener_.hooked) {
		tracked->self->listener_.hooked(tracked->uuid, QString(), QString());
	}
}

QList<CaptureWindow> ObsCaptureSources::windows(const QString &uuid)
{
	QList<CaptureWindow> out;
	OBSSourceAutoRelease source = obs_get_source_by_uuid(uuid.toUtf8().constData());
	if (!source) {
		return out;
	}
	obs_properties_t *props = obs_source_properties(source);
	if (!props) {
		return out;
	}
	obs_property_t *window = obs_properties_get(props, "window");
	const auto format = obs_property_list_format(window);
	const size_t count = obs_property_list_item_count(window);
	for (size_t i = 0; i < count; i++) {
		const char *title = obs_property_list_item_name(window, i);
		if (!title || title[0] == '\0') {
			continue;
		}
		if (format == OBS_COMBO_FORMAT_STRING) {
			out.append({QString::fromUtf8(title), QString::fromUtf8(obs_property_list_item_string(window, i))});
		} else if (format == OBS_COMBO_FORMAT_INT) {
			out.append({QString::fromUtf8(title), QVariant::fromValue<qlonglong>(obs_property_list_item_int(window, i))});
		}
	}
	obs_properties_destroy(props);
	return out;
}

void ObsCaptureSources::pickWindow(const QString &uuid, const QVariant &value)
{
	OBSSourceAutoRelease source = obs_get_source_by_uuid(uuid.toUtf8().constData());
	if (!source) {
		return;
	}
	OBSDataAutoRelease data = obs_data_create();
	if (value.typeId() == QMetaType::QString) {
		obs_data_set_string(data, "window", value.toString().toUtf8().constData());
	} else {
		obs_data_set_int(data, "window", value.toLongLong());
	}
	obs_source_update(source, data);
}

} // namespace fiji::obs
