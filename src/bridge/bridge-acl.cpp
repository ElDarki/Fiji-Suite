#include "bridge-acl.hpp"

#include "../controller/command-dispatcher.hpp"
#include "../model/obs-status.hpp"
#include "../state/link-state.hpp"
#include "catalogue.hpp"

#include <QJsonArray>
#include <QMetaObject>
#include <QMutexLocker>

namespace fiji::bridge {

namespace {

const int kDroppedEveryMs = 15000;
const int kChatLimit = 480;

QJsonObject only(const QJsonObject &from, const QStringList &fields)
{
	QJsonObject out;
	for (const auto &f : fields) {
		if (from.contains(f)) {
			out.insert(f, from.value(f));
		}
	}
	return out;
}

QString outputState(const model::OutputStatus &o)
{
	if (!o.state.isEmpty()) {
		return o.state; // the output's own life cycle
	}
	return o.active ? (o.reconnecting ? QStringLiteral("RECONNECTING") : QStringLiteral("ACTIVE")) : QStringLiteral("IDLE");
}

} // namespace

BridgeAcl::BridgeAcl(controller::CommandDispatcher &dispatcher, state::LinkStateMachine &link, QString appVersion,
		     QObject *parent)
	: QObject(parent),
	  dispatcher_(dispatcher),
	  link_(link),
	  appVersion_(std::move(appVersion))
{
	droppedTimer_.setInterval(kDroppedEveryMs);
	connect(&droppedTimer_, &QTimer::timeout, this, &BridgeAcl::reportDroppedFrames);
	droppedTimer_.start();
}

QStringList BridgeAcl::offeredCapabilities()
{
	return {"scenes", "transitions", "outputs", "output.alerts", "decks", "decks.pages", "chat", "triggers"};
}

QString BridgeAcl::mainCanvasName()
{
	return QStringLiteral("Main");
}

bool BridgeAcl::linked() const
{
	return linked_;
}

bool BridgeAcl::accepts(const QString &capability) const
{
	if (capability.isEmpty()) {
		return true;
	}
	QMutexLocker lock(&mutex_);
	return accepted_.contains(capability);
}

// ---- the transport's side --------------------------------------------------------------------------
void BridgeAcl::onOpen()
{
	linked_ = false;
	Envelope hello;
	hello.type = MessageType::HELLO;
	hello.action = QStringLiteral("link.hello");
	hello.id = newId();
	hello.payload = QJsonObject{{"role", "plugin"},
				    {"app", "fiji-suite"},
				    {"app_version", appVersion_},
				    {"capabilities", QJsonArray::fromStringList(offeredCapabilities())}};
	{
		QMutexLocker lock(&mutex_);
		accepted_.clear();
		helloId_ = hello.id;
	}
	sendEnvelope(hello);
}

void BridgeAcl::onClosed()
{
	const bool wasLinked = linked_.exchange(false);
	{
		QMutexLocker lock(&mutex_);
		accepted_.clear();
	}
	if (wasLinked) {
		QMetaObject::invokeMethod(this, [this] { link_.dropped(); }, Qt::QueuedConnection);
	}
}

void BridgeAcl::onFrame(const QByteArray &frame)
{
	const auto envelope = Envelope::parse(frame);
	if (!envelope) {
		return; // not an envelope: dropped, the link stays up
	}
	if (envelope->type == MessageType::HELLO) {
		handleHello(*envelope);
	} else if (!linked_) {
		return; // nothing but HELLO before the handshake
	} else if (envelope->type == MessageType::COMMAND) {
		handleCommand(*envelope);
	}
	// The bot sends no EVENT or SYNC_STATE, and the plugin sends no COMMAND that would need an ACK back.
}

void BridgeAcl::handleHello(const Envelope &hello)
{
	const auto &p = hello.payload;
	if (majorOf(hello.version) != majorOf(QString::fromLatin1(kContractVersion)) ||
	    p.value("reject").toString() == QStringLiteral("version")) {
		linked_ = false;
		const auto detail = QStringLiteral("The bot speaks bridge contract %1, this plugin %2.")
					    .arg(hello.version, QString::fromLatin1(kContractVersion));
		QMetaObject::invokeMethod(
			this, [this, detail] { link_.failed(state::LinkReason::VersionMismatch, detail); },
			Qt::QueuedConnection);
		if (refuse) {
			refuse();
		}
		return;
	}
	QStringList accepted;
	const auto offered = offeredCapabilities();
	for (const auto &v : p.value("accepted").toArray()) {
		if (offered.contains(v.toString()) && !accepted.contains(v.toString())) {
			accepted.append(v.toString());
		}
	}
	{
		QMutexLocker lock(&mutex_);
		accepted_ = accepted;
	}
	linked_ = true;
	QMetaObject::invokeMethod(
		this,
		[this, accepted] {
			link_.helloCompleted(accepted);
			publishSnapshot();
		},
		Qt::QueuedConnection);
}

void BridgeAcl::handleCommand(const Envelope &command)
{
	const auto &actions = catalogue().actions;
	const auto it = actions.constFind(command.action);
	controller::CommandResult refused;
	refused.ok = false;
	if (it == actions.constEnd() || it->type != QStringLiteral("COMMAND") ||
	    (it->dir != QStringLiteral("B>P") && it->dir != QStringLiteral("both")) || !accepts(it->capability)) {
		refused.code = QStringLiteral("UNKNOWN_ACTION");
		refused.message = QStringLiteral("This plugin does not take %1").arg(command.action);
		sendAck(command.id, command.action, refused);
		return;
	}
	for (const auto &field : it->required) {
		if (!command.payload.contains(field)) {
			refused.code = QStringLiteral("BAD_REQUEST");
			refused.message = QStringLiteral("%1 needs %2").arg(command.action, field);
			sendAck(command.id, command.action, refused);
			return;
		}
	}
	if (command.action == QStringLiteral("state.sync")) {
		sendAck(command.id, command.action, controller::CommandResult{});
		const auto id = command.id;
		QMetaObject::invokeMethod(this, [this, id] { publishSnapshot(id); }, Qt::QueuedConnection);
		return;
	}
	controller::Command c;
	c.action = command.action;
	c.payload = only(command.payload, it->required + it->optional);
	c.source = controller::CommandSource::Bridge;
	c.replyTo = command.id;
	const auto id = command.id;
	const auto action = command.action;
	dispatcher_.post(c, [this, id, action](const controller::CommandResult &result) { sendAck(id, action, result); });
}

// ---- the plugin's side -----------------------------------------------------------------------------
void BridgeAcl::onPluginEvent(const QString &name, const QJsonObject &data)
{
	if (!linked_) {
		return;
	}
	const auto text = [&data](const char *key) { return data.value(QLatin1String(key)).toString(); };
	if (name == "say") {
		sendEvent("chat.say", {{"text", text("text").left(kChatLimit)}});
	} else if (name == "button_state") {
		sendEvent("deck.key", {{"button", text("button")}, {"running", data.value("running").toBool()}});
	} else if (name == "deck_notification") {
		sendEvent("deck.notice", {{"text", text("text")}});
	} else if (name == "decks_changed") {
		sendEvent("deck.changed", {});
		publishSnapshot();
	} else if (name == "switch_scene") {
		sendEvent("scene.program", {{"canvas", text("canvas")}, {"scene", text("new_scene")}, {"previous", text("old_scene")}});
	} else if (name == "main_scene_changed") {
		sendEvent("scene.program", {{"canvas", mainCanvasName()}, {"scene", text("scene")}, {"previous", text("previous")}});
	} else if (name == "start_output" || name == "stop_output" || name == "output_reconnecting" ||
		   name == "output_reconnected" || name == "output_starting" || name == "output_stopping") {
		const auto output = text("output");
		QJsonObject payload{{"output", output}, {"kind", outputKinds_.value(output)}};
		const auto error = text("last_error");
		QString state = text("state"); // set by the output's life cycle; derived for older senders
		if (state.isEmpty()) {
			state = name == "output_reconnecting" ? QStringLiteral("RECONNECTING")
				: name == "output_starting"   ? QStringLiteral("STARTING")
				: name == "output_stopping"   ? QStringLiteral("STOPPING")
				: name == "stop_output"       ? (error.isEmpty() ? QStringLiteral("IDLE") : QStringLiteral("FAILED"))
							      : QStringLiteral("ACTIVE");
		}
		if (state == QStringLiteral("FAILED") && !error.isEmpty()) {
			payload.insert("error", error);
		}
		payload.insert("state", state);
		sendEvent("output.state", payload);
	} else if (name == "scene_added" || name == "scene_removed" || name == "scene_renamed" || name == "scene_list_changed" ||
		   name == "canvas_changed" || name == "outputs_changed") {
		publishSnapshot(); // no event covers these (BRIDGE_CONTRACT.md section 4)
	}
}

void BridgeAcl::publishSnapshot(const QString &replyTo)
{
	if (!linked_) {
		return;
	}
	withSnapshot([this, replyTo](const model::ObsSnapshot &snapshot) {
		Envelope e;
		e.type = MessageType::SYNC_STATE;
		e.action = QStringLiteral("state.snapshot");
		e.id = replyTo.isEmpty() ? newId() : replyTo;
		e.payload = wireSnapshot(snapshot);
		sendEnvelope(e);
	});
}

void BridgeAcl::reportDroppedFrames()
{
	if (!linked_ || !accepts(QStringLiteral("output.alerts"))) {
		return;
	}
	withSnapshot([this](const model::ObsSnapshot &snapshot) {
		for (const auto &o : snapshot.outputs) {
			if (o.active) {
				sendEvent("output.dropped", {{"output", o.name},
							     {"dropped_frames", double(o.droppedFrames)},
							     {"total_frames", double(o.totalFrames)}});
			}
		}
	});
}

void BridgeAcl::withSnapshot(std::function<void(const model::ObsSnapshot &)> then)
{
	controller::Command c;
	c.action = QStringLiteral("state.snapshot");
	c.source = controller::CommandSource::Bridge;
	dispatcher_.post(c, [then = std::move(then)](const controller::CommandResult &result) {
		if (result.ok) {
			then(model::ObsSnapshot::fromJson(result.result));
		}
	});
}

QJsonObject BridgeAcl::wireSnapshot(const model::ObsSnapshot &s)
{
	QJsonObject payload;
	if (accepts(QStringLiteral("scenes"))) {
		QJsonArray canvases;
		for (const auto &c : s.canvases) {
			canvases.append(QJsonObject{{"canvas", c.main ? mainCanvasName() : c.name},
						    {"main", c.main},
						    {"program", c.program},
						    {"preview", c.preview.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(c.preview)},
						    {"studio_mode", c.studioMode},
						    {"scenes", QJsonArray::fromStringList(c.scenes)},
						    {"transitions", QJsonArray::fromStringList(c.transitions)}});
		}
		payload.insert("canvases", canvases);
	}
	outputKinds_.clear();
	QJsonArray outputs;
	for (const auto &o : s.outputs) {
		outputKinds_.insert(o.name, o.kind);
		outputs.append(QJsonObject{{"output", o.name},
					   {"kind", o.kind},
					   {"canvas", o.canvas},
					   {"group", o.group},
					   {"main", o.main},
					   {"state", outputState(o)},
					   {"uptime", double(o.uptime)},
					   {"dropped_frames", double(o.droppedFrames)},
					   {"total_frames", double(o.totalFrames)}});
	}
	if (accepts(QStringLiteral("outputs"))) {
		payload.insert("outputs", outputs);
	}
	if (accepts(QStringLiteral("decks"))) {
		QJsonArray decks;
		for (const auto &d : s.decks) {
			decks.append(QJsonObject{{"id", d.id}, {"name", d.name}, {"buttons", d.buttons}});
		}
		payload.insert("decks", decks);
		payload.insert("running", QJsonArray::fromStringList(s.running));
	}
	return payload;
}

// ---- sending -----------------------------------------------------------------------------------------
void BridgeAcl::sendEnvelope(const Envelope &envelope)
{
	if (send) {
		send(envelope.serialise());
	}
}

void BridgeAcl::sendEvent(const QString &action, const QJsonObject &payload)
{
	const auto spec = catalogue().actions.value(action);
	if (spec.type != QStringLiteral("EVENT") || !accepts(spec.capability)) {
		return; // only accepted capabilities, only catalogue events
	}
	Envelope e;
	e.type = MessageType::EVENT;
	e.action = action;
	e.payload = only(payload, spec.required + spec.optional);
	sendEnvelope(e);
}

void BridgeAcl::sendAck(const QString &id, const QString &action, const controller::CommandResult &result)
{
	Envelope e;
	e.type = MessageType::ACK;
	e.id = id;
	e.action = action;
	if (result.ok) {
		e.payload = QJsonObject{{"ok", true}, {"result", only(result.result, catalogue().actions.value(action).result)}};
	} else {
		const auto code = catalogue().errors.contains(result.code) ? result.code : QStringLiteral("OBS_ERROR");
		e.payload = QJsonObject{
			{"ok", false},
			{"error", QJsonObject{{"code", code}, {"message", result.message}, {"retryable", result.retryable}}}};
	}
	sendEnvelope(e);
}

} // namespace fiji::bridge
