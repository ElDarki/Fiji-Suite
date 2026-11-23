#include "request-obs-api.hpp"

#include <obs.hpp>
#include <obs-frontend-api.h>

#include <QJsonArray>
#include <QJsonDocument>

// The plugin's request table (src/utils/obs-websocket.cpp).
bool fiji_call_request(const char *name, obs_data_t *request, obs_data_t *response);

namespace fiji::obs {

namespace {

const QStringList kMainOutputs = {"stream", "record", "backtrack", "virtual_cam"};

QJsonObject call(const char *name, const QJsonObject &data = {})
{
	OBSDataAutoRelease request = obs_data_create_from_json(QJsonDocument(data).toJson(QJsonDocument::Compact).constData());
	if (!request) {
		request = obs_data_create();
	}
	OBSDataAutoRelease response = obs_data_create();
	if (!fiji_call_request(name, request, response)) {
		return {{"success", false}, {"error", QStringLiteral("unknown request %1").arg(QLatin1String(name))}};
	}
	return QJsonDocument::fromJson(QByteArray(obs_data_get_json(response))).object();
}

// A reply of the request table -> ObsCall, keeping only `fields` of it.
ObsCall result(const QJsonObject &reply, const QStringList &fields = {})
{
	if (!reply.value("success").toBool()) {
		const auto error = reply.value("error").toString();
		const bool missing = error.contains(QStringLiteral("not found"), Qt::CaseInsensitive);
		return ObsCall::failed(missing ? QStringLiteral("NOT_FOUND") : QStringLiteral("OBS_ERROR"),
				       error.isEmpty() ? QStringLiteral("OBS refused") : error);
	}
	ObsCall ok;
	for (const auto &f : fields) {
		if (reply.contains(f)) {
			ok.data.insert(f, reply.value(f));
		}
	}
	return ok;
}

QStringList names(const QJsonValue &list)
{
	QStringList out;
	for (const auto &v : list.toArray()) {
		out.append(v.toObject().value("name").toString());
	}
	return out;
}

QStringList mainTransitions()
{
	QStringList out;
	obs_frontend_source_list list = {};
	obs_frontend_get_transitions(&list);
	for (size_t i = 0; i < list.sources.num; i++) {
		out.append(QString::fromUtf8(obs_source_get_name(list.sources.array[i])));
	}
	obs_frontend_source_list_free(&list);
	return out;
}

} // namespace

model::ObsSnapshot RequestObsApi::snapshot()
{
	model::ObsSnapshot s;
	const auto status = call("get_status");
	OBSSourceAutoRelease current = obs_frontend_get_current_scene();
	OBSSourceAutoRelease preview = obs_frontend_preview_program_mode_active() ? obs_frontend_get_current_preview_scene()
										  : nullptr;
	s.canvases.append(model::CanvasState{QStringLiteral("Main"), true, status.value("main_scene").toString(),
					     preview ? QString::fromUtf8(obs_source_get_name(preview)) : QString(),
					     obs_frontend_preview_program_mode_active(), names(status.value("main_scenes")),
					     mainTransitions()});
	for (const auto &v : status.value("canvases").toArray()) {
		const auto c = v.toObject();
		s.canvases.append(model::CanvasState{c.value("name").toString(), false, c.value("scene").toString(),
						     c.value("preview_scene").toString(), c.value("studio_mode").toBool(),
						     names(c.value("scenes")), {}});
	}
	for (const auto &v : status.value("outputs").toArray()) {
		const auto o = v.toObject();
		s.outputs.append(model::OutputStatus{o.value("name").toString(), o.value("type").toString(),
						     o.value("canvas").toString(), o.value("group").toString(),
						     o.value("main").toBool(), o.value("active").toBool(),
						     o.value("reconnecting").toBool(), qint64(o.value("uptime").toDouble()),
						     qint64(o.value("dropped_frames").toDouble()),
						     qint64(o.value("total_frames").toDouble())});
	}
	for (const auto &v : call("get_decks").value("decks").toArray()) {
		const auto d = v.toObject();
		s.decks.append(model::DeckSummary{d.value("id").toString(), d.value("name").toString(),
						  int(d.value("buttons").toArray().size())});
	}
	for (const auto &v : call("get_deck_state").value("running").toArray()) {
		s.running.append(v.toObject().value("button").toString());
	}
	return s;
}

ObsCall RequestObsApi::switchScene(const QString &scene, const QString &canvas)
{
	if (canvas.isEmpty() || canvas == QStringLiteral("Main")) {
		return result(call("set_main_scene", {{"scene", scene}}));
	}
	return result(call("switch_scene", {{"canvas", canvas}, {"scene", scene}}));
}

ObsCall RequestObsApi::setTransition(const QString &canvas, const QString &transition)
{
	return result(call("switch_transition", {{"canvas", canvas}, {"transition", transition}}));
}

ObsCall RequestObsApi::runTransition(const QString &canvas)
{
	return result(call("transition", {{"canvas", canvas}}));
}

ObsCall RequestObsApi::startOutputs(const QString &target)
{
	return outputs(true, target);
}

ObsCall RequestObsApi::stopOutputs(const QString &target)
{
	return outputs(false, target);
}

ObsCall RequestObsApi::outputs(bool start, const QString &target)
{
	const char *verb = start ? "start" : "stop";
	const auto key = QLatin1String(start ? "started" : "stopped");
	const auto list = snapshot().outputs;
	QStringList affected;
	if (target == QStringLiteral("all")) {
		for (const auto &o : list) {
			affected.append(o.name);
		}
		auto call_ = result(call(start ? "start_all_outputs" : "stop_all_outputs"));
		call_.data.insert(key, QJsonArray::fromStringList(affected));
		return call_;
	}
	for (const auto &o : list) {
		if (o.group == target) {
			affected.append(o.name);
		}
	}
	if (!affected.isEmpty()) {
		auto call_ = result(call(start ? "start_group" : "stop_group", {{"group", target}}));
		call_.data.insert(key, QJsonArray::fromStringList(affected));
		return call_;
	}
	QString mainKind = kMainOutputs.contains(target) ? target : QString();
	bool known = !mainKind.isEmpty();
	for (const auto &o : list) {
		if (o.name == target) {
			known = true;
			if (o.main) {
				mainKind = o.kind;
			}
		}
	}
	if (!known) {
		return ObsCall::failed(QStringLiteral("NOT_FOUND"), QStringLiteral("There is no output or group called %1").arg(target));
	}
	auto call_ = mainKind.isEmpty() ? result(call(QStringLiteral("%1_output").arg(QLatin1String(verb)).toUtf8().constData(),
						      {{"output", target}}))
					: result(call(QStringLiteral("%1_main").arg(QLatin1String(verb)).toUtf8().constData(),
						      {{"output", mainKind}}));
	call_.data.insert(key, QJsonArray{target});
	return call_;
}

ObsCall RequestObsApi::decks()
{
	return result(call("get_decks"), {"decks"});
}

ObsCall RequestObsApi::pressButton(const QString &button, const QJsonObject &vars)
{
	return result(call("press_button", {{"button", button}, {"data", vars}}));
}

ObsCall RequestObsApi::releaseButton(const QString &button)
{
	return result(call("release_button", {{"button", button}}));
}

ObsCall RequestObsApi::stopButton(const QString &button)
{
	return result(call("stop_button", {{"button", button}}));
}

ObsCall RequestObsApi::saveDeck(const QJsonObject &deck)
{
	return result(call("save_deck", {{"deck", deck}}), {"deck"});
}

ObsCall RequestObsApi::deleteDeck(const QString &id)
{
	return result(call("delete_deck", {{"id", id}}));
}

ObsCall RequestObsApi::triggers()
{
	return result(call("get_triggers"), {"triggers"});
}

ObsCall RequestObsApi::webFile(const QString &name)
{
	return result(call("get_web_file", {{"name", name}}), {"content", "type"});
}

ObsCall RequestObsApi::look()
{
	return result(call("get_look"), {"dark"});
}

} // namespace fiji::obs
