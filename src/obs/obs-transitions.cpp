#include "obs-transitions.hpp"

#include <obs-frontend-api.h>
#include <obs.hpp>

#include <QMetaObject>
#include <QObject>

#include <functional>

namespace fiji::obs {

namespace {

// Runs with OBS's transition of that name (not referenced past the call), if there is one.
bool withTransition(const QString &name, const std::function<void(obs_source_t *)> &use)
{
	const QByteArray wanted = name.toUtf8();
	obs_frontend_source_list list = {};
	obs_frontend_get_transitions(&list);
	bool found = false;
	for (size_t i = 0; i < list.sources.num && !found; i++) {
		obs_source_t *t = list.sources.array[i];
		if (wanted == obs_source_get_name(t)) {
			use(t);
			found = true;
		}
	}
	obs_frontend_source_list_free(&list);
	return found;
}

} // namespace

QList<model::TransitionInfo> FrontendTransitions::transitions()
{
	QList<model::TransitionInfo> out;
	obs_frontend_source_list list = {};
	obs_frontend_get_transitions(&list);
	for (size_t i = 0; i < list.sources.num; i++) {
		obs_source_t *t = list.sources.array[i];
		out.append({QString::fromUtf8(obs_source_get_name(t)), obs_is_source_configurable(obs_source_get_id(t))});
	}
	obs_frontend_source_list_free(&list);
	return out;
}

QString FrontendTransitions::current()
{
	OBSSourceAutoRelease t = obs_frontend_get_current_transition();
	return t ? QString::fromUtf8(obs_source_get_name(t)) : QString();
}

void FrontendTransitions::select(const QString &name)
{
	withTransition(name, [](obs_source_t *t) { obs_frontend_set_current_transition(t); });
}

bool FrontendTransitions::addNew(const QString &typeId, const QString &)
{
	auto main_window = static_cast<QObject *>(obs_frontend_get_main_window());
	if (!main_window) {
		return false;
	}
	// OBS's own "add transition": it asks for the name and keeps the transition with the scene collection.
	// The type id must outlive the queued call: OBS's registered ids do.
	const char *id = nullptr;
	size_t idx = 0;
	const QByteArray wanted = typeId.toUtf8();
	const char *candidate;
	while (obs_enum_transition_types(idx++, &candidate)) {
		if (wanted == candidate) {
			id = candidate;
			break;
		}
	}
	if (!id) {
		return false;
	}
	QMetaObject::invokeMethod(main_window, "AddTransition", Q_ARG(const char *, id));
	return true;
}

bool FrontendTransitions::rename(const QString &name, const QString &newName)
{
	const QByteArray n = newName.toUtf8();
	return withTransition(name, [&n](obs_source_t *t) { obs_source_set_name(t, n.constData()); });
}

void FrontendTransitions::openProperties(const QString &name)
{
	withTransition(name, [](obs_source_t *t) { obs_frontend_open_source_properties(t); });
}

QList<model::TransitionType> FrontendTransitions::types()
{
	QList<model::TransitionType> out;
	size_t idx = 0;
	const char *id;
	while (obs_enum_transition_types(idx++, &id)) {
		if (obs_is_source_configurable(id)) {
			out.append({QString::fromUtf8(id), QString::fromUtf8(obs_source_get_display_name(id))});
		}
	}
	return out;
}

int FrontendTransitions::duration()
{
	return obs_frontend_get_transition_duration();
}

void FrontendTransitions::setDuration(int ms)
{
	obs_frontend_set_transition_duration(ms);
}

} // namespace fiji::obs
