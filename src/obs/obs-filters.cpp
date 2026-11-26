#include "obs-filters.hpp"

#include <obs-frontend-api.h>
#include <obs.hpp>

#include <QMetaObject>

#include <string>

namespace fiji::obs {

struct ObsFilters::Watched {
	ObsFilters *owner;
	QString key;
	obs_weak_source_t *filter;
};

namespace {

QString keyOf(obs_source_t *filter)
{
	return QString::fromUtf8(obs_source_get_uuid(filter));
}

model::MediaFlags flagsOf(uint32_t flags)
{
	return {(flags & OBS_SOURCE_VIDEO) != 0, (flags & OBS_SOURCE_AUDIO) != 0, (flags & OBS_SOURCE_ASYNC) != 0};
}

const char *const kSourceSignals[] = {"remove", "destroy"};

} // namespace

ObsFilters::ObsFilters(model::FilterList &filters, QObject *parent) : QObject(parent), filters_(filters) {}

ObsFilters::~ObsFilters()
{
	show(nullptr);
}

obs_source_t *ObsFilters::shown() const
{
	return source_ ? obs_weak_source_get_source(source_) : nullptr;
}

void ObsFilters::show(obs_source_t *source)
{
	if (source_ && source && obs_weak_source_references_source(source_, source)) {
		return;
	}
	if (source_) {
		if (OBSSourceAutoRelease previous = obs_weak_source_get_source(source_)) {
			auto sh = obs_source_get_signal_handler(previous);
			for (auto name : kSourceSignals) {
				signal_handler_disconnect(sh, name, source_remove, this);
			}
			signal_handler_disconnect(sh, "filter_add", filter_add, this);
			signal_handler_disconnect(sh, "filter_remove", filter_remove, this);
			signal_handler_disconnect(sh, "reorder_filters", filter_reorder, this);
		}
		obs_weak_source_release(source_);
		source_ = nullptr;
	}
	unwatchFilters();
	if (!source) {
		filters_.clear();
		return;
	}
	source_ = obs_source_get_weak_source(source);
	auto sh = obs_source_get_signal_handler(source);
	for (auto name : kSourceSignals) {
		signal_handler_connect(sh, name, source_remove, this);
	}
	signal_handler_connect(sh, "filter_add", filter_add, this);
	signal_handler_connect(sh, "filter_remove", filter_remove, this);
	signal_handler_connect(sh, "reorder_filters", filter_reorder, this);
	filters_.show(readFilters());
	watchFilters();
}

void ObsFilters::deselected(obs_source_t *source)
{
	if (source_ && obs_weak_source_references_source(source_, source)) {
		show(nullptr);
	}
}

QList<model::Filter> ObsFilters::readFilters()
{
	QList<model::Filter> out;
	OBSSourceAutoRelease source = shown();
	if (!source) {
		return out;
	}
	obs_source_enum_filters(
		source,
		[](obs_source_t *, obs_source_t *filter, void *param) {
			static_cast<QList<model::Filter> *>(param)->append(
				{keyOf(filter), QString::fromUtf8(obs_source_get_name(filter)), obs_source_enabled(filter)});
		},
		&out);
	return out;
}

// Each filter's own signals: renamed, turned on or off.
void ObsFilters::watchFilters()
{
	OBSSourceAutoRelease source = shown();
	if (!source) {
		return;
	}
	obs_source_enum_filters(
		source,
		[](obs_source_t *, obs_source_t *filter, void *param) {
			auto self = static_cast<ObsFilters *>(param);
			const QString key = keyOf(filter);
			if (self->watched_.contains(key)) {
				return;
			}
			auto w = new Watched{self, key, obs_source_get_weak_source(filter)};
			self->watched_.insert(key, w);
			auto sh = obs_source_get_signal_handler(filter);
			signal_handler_connect(sh, "enable", filter_enable, w);
			signal_handler_connect(sh, "rename", filter_rename, w);
		},
		this);
}

void ObsFilters::unwatchFilters()
{
	for (auto w : watched_) {
		if (OBSSourceAutoRelease filter = obs_weak_source_get_source(w->filter)) {
			auto sh = obs_source_get_signal_handler(filter);
			signal_handler_disconnect(sh, "enable", filter_enable, w);
			signal_handler_disconnect(sh, "rename", filter_rename, w);
		}
		obs_weak_source_release(w->filter);
		delete w;
	}
	watched_.clear();
}

obs_source_t *ObsFilters::filterSource(const QString &key) const
{
	OBSSourceAutoRelease source = shown();
	if (!source || key.isEmpty()) {
		return nullptr;
	}
	struct Find {
		QByteArray uuid;
		obs_source_t *found = nullptr;
	} find{key.toUtf8()};
	obs_source_enum_filters(
		source,
		[](obs_source_t *, obs_source_t *filter, void *param) {
			auto f = static_cast<Find *>(param);
			if (!f->found && f->uuid == obs_source_get_uuid(filter)) {
				f->found = obs_source_get_ref(filter);
			}
		},
		&find);
	return find.found;
}

model::MediaFlags ObsFilters::sourceFlags()
{
	OBSSourceAutoRelease source = shown();
	return source ? flagsOf(obs_source_get_output_flags(source)) : model::MediaFlags{};
}

QList<model::FilterType> ObsFilters::filterTypes()
{
	QList<model::FilterType> types;
	size_t idx = 0;
	const char *id;
	while (obs_enum_filter_types(idx++, &id)) {
		const uint32_t caps = obs_get_source_output_flags(id);
		model::FilterType type;
		type.id = QString::fromUtf8(id);
		type.displayName = QString::fromUtf8(obs_source_get_display_name(id));
		type.flags = flagsOf(caps);
		type.retired = (caps & (OBS_SOURCE_DEPRECATED | OBS_SOURCE_CAP_DISABLED | OBS_SOURCE_CAP_OBSOLETE)) != 0;
		types.append(type);
	}
	return types;
}

QString ObsFilters::add(const QString &typeId, const QString &name)
{
	OBSSourceAutoRelease source = shown();
	if (!source) {
		return {};
	}
	OBSSourceAutoRelease filter =
		obs_source_create(typeId.toUtf8().constData(), name.toUtf8().constData(), nullptr, nullptr);
	if (!filter) {
		return {};
	}
	obs_source_filter_add(source, filter);
	return keyOf(filter);
}

QString ObsFilters::duplicate(const QString &key, const QString &name)
{
	OBSSourceAutoRelease source = shown();
	OBSSourceAutoRelease filter = filterSource(key);
	if (!source || !filter) {
		return {};
	}
	OBSSourceAutoRelease copy = obs_source_duplicate(filter, name.toUtf8().constData(), true);
	if (!copy) {
		return {};
	}
	obs_source_filter_add(source, copy);
	return keyOf(copy);
}

void ObsFilters::remove(const QString &key)
{
	OBSSourceAutoRelease source = shown();
	OBSSourceAutoRelease filter = filterSource(key);
	if (!source || !filter) {
		return;
	}
	OBSDataAutoRelease wrapper = obs_save_source(filter);
	obs_data_set_string(wrapper, "undo_uuid", obs_source_get_uuid(source));
	const std::string undo_json = obs_data_get_json(wrapper);

	obs_source_filter_remove(source, filter);

	OBSDataAutoRelease rwrapper = obs_data_create();
	obs_data_set_string(rwrapper, "fname", obs_source_get_name(filter));
	obs_data_set_string(rwrapper, "suuid", obs_source_get_uuid(source));
	const std::string redo_json = obs_data_get_json(rwrapper);

	const auto actionName =
		QString::fromUtf8(obs_frontend_get_locale_string("Undo.Delete")).arg(QString::fromUtf8(obs_source_get_name(filter)));
	obs_frontend_add_undo_redo_action(actionName.toUtf8().constData(), restore_filter, remove_filter, undo_json.c_str(),
					  redo_json.c_str(), false);
}

void ObsFilters::restore_filter(const char *json)
{
	OBSDataAutoRelease data = obs_data_create_from_json(json);
	OBSSourceAutoRelease existing = obs_get_source_by_uuid(obs_data_get_string(data, "uuid"));
	if (existing) {
		return;
	}
	OBSSourceAutoRelease source = obs_get_source_by_uuid(obs_data_get_string(data, "undo_uuid"));
	if (!source) {
		return;
	}
	OBSSourceAutoRelease filter = obs_load_source(data);
	obs_source_filter_add(source, filter);
}

void ObsFilters::remove_filter(const char *json)
{
	OBSDataAutoRelease data = obs_data_create_from_json(json);
	OBSSourceAutoRelease source = obs_get_source_by_uuid(obs_data_get_string(data, "suuid"));
	if (!source) {
		return;
	}
	OBSSourceAutoRelease filter = obs_source_get_filter_by_name(source, obs_data_get_string(data, "fname"));
	if (filter) {
		obs_source_filter_remove(source, filter);
	}
}

void ObsFilters::rename(const QString &key, const QString &name)
{
	if (OBSSourceAutoRelease filter = filterSource(key)) {
		obs_source_set_name(filter, name.toUtf8().constData());
	}
}

void ObsFilters::setEnabled(const QString &key, bool enabled)
{
	if (OBSSourceAutoRelease filter = filterSource(key)) {
		obs_source_set_enabled(filter, enabled);
	}
}

void ObsFilters::move(const QString &key, model::FilterMove move)
{
	OBSSourceAutoRelease source = shown();
	OBSSourceAutoRelease filter = filterSource(key);
	if (!source || !filter) {
		return;
	}
	obs_order_movement movement = OBS_ORDER_MOVE_UP;
	switch (move) {
	case model::FilterMove::Up:
		movement = OBS_ORDER_MOVE_UP;
		break;
	case model::FilterMove::Down:
		movement = OBS_ORDER_MOVE_DOWN;
		break;
	case model::FilterMove::Top:
		movement = OBS_ORDER_MOVE_TOP;
		break;
	case model::FilterMove::Bottom:
		movement = OBS_ORDER_MOVE_BOTTOM;
		break;
	}
	obs_source_filter_set_order(source, filter, movement);
}

// ---- OBS's signals (on OBS's threads); the model changes on the UI thread --------------------------------------
void ObsFilters::filter_add(void *data, calldata_t *cd)
{
	auto self = static_cast<ObsFilters *>(data);
	auto filter = static_cast<obs_source_t *>(calldata_ptr(cd, "filter"));
	if (!filter) {
		return;
	}
	const model::Filter f{keyOf(filter), QString::fromUtf8(obs_source_get_name(filter)), obs_source_enabled(filter)};
	QMetaObject::invokeMethod(
		self,
		[self, f] {
			self->filters_.added(f);
			self->watchFilters();
		},
		Qt::QueuedConnection);
}

void ObsFilters::filter_remove(void *data, calldata_t *cd)
{
	auto self = static_cast<ObsFilters *>(data);
	auto filter = static_cast<obs_source_t *>(calldata_ptr(cd, "filter"));
	if (!filter) {
		return;
	}
	const QString key = keyOf(filter);
	QMetaObject::invokeMethod(
		self,
		[self, key] {
			if (Watched *w = self->watched_.take(key)) {
				if (OBSSourceAutoRelease f = obs_weak_source_get_source(w->filter)) {
					auto sh = obs_source_get_signal_handler(f);
					signal_handler_disconnect(sh, "enable", filter_enable, w);
					signal_handler_disconnect(sh, "rename", filter_rename, w);
				}
				obs_weak_source_release(w->filter);
				delete w;
			}
			self->filters_.removed(key);
		},
		Qt::QueuedConnection);
}

void ObsFilters::filter_reorder(void *data, calldata_t *)
{
	auto self = static_cast<ObsFilters *>(data);
	QMetaObject::invokeMethod(self, [self] { self->filters_.reordered(self->readFilters()); }, Qt::QueuedConnection);
}

void ObsFilters::source_remove(void *data, calldata_t *cd)
{
	auto self = static_cast<ObsFilters *>(data);
	auto source = static_cast<obs_source_t *>(calldata_ptr(cd, "source"));
	if (!self->source_ || !obs_weak_source_references_source(self->source_, source)) {
		return;
	}
	QMetaObject::invokeMethod(
		self,
		[self] {
			OBSSourceAutoRelease s = self->shown();
			if (!s || obs_source_removed(s)) {
				self->show(nullptr);
			}
		},
		Qt::QueuedConnection);
}

void ObsFilters::filter_enable(void *data, calldata_t *cd)
{
	auto w = static_cast<Watched *>(data);
	auto self = w->owner;
	const QString key = w->key;
	const bool enabled = calldata_bool(cd, "enabled");
	QMetaObject::invokeMethod(self, [self, key, enabled] { self->filters_.enabledChanged(key, enabled); },
				  Qt::QueuedConnection);
}

void ObsFilters::filter_rename(void *data, calldata_t *cd)
{
	auto w = static_cast<Watched *>(data);
	auto self = w->owner;
	const QString key = w->key;
	const QString name = QString::fromUtf8(calldata_string(cd, "new_name"));
	QMetaObject::invokeMethod(self, [self, key, name] { self->filters_.renamed(key, name); }, Qt::QueuedConnection);
}

} // namespace fiji::obs
