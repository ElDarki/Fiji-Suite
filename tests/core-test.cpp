// Tests for the MVC + State core with Qt Core only (no OBS): the link state machine, the command dispatcher, the
// bridge envelope and the bridge ACL driven through a fake ObsApi.
//   cmake --build build_x86_64 --target fiji-core-test && ctest --test-dir build_x86_64 -R core

#include "../src/bridge/bridge-acl.hpp"
#include "../src/bridge/catalogue.hpp"
#include "../src/bridge/envelope.hpp"
#include "../src/controller/command-dispatcher.hpp"
#include "../src/controller/deck-controller.hpp"
#include "../src/model/deck-model.hpp"
#include "../src/model/local-store.hpp"
#include "../src/obs/deck-port.hpp"
#include "../src/state/deck-keys.hpp"
#include "../src/controller/obs-command-handler.hpp"
#include "../src/controller/output-controller.hpp"
#include "../src/model/output-targets.hpp"
#include "../src/model/theme-files.hpp"
#include "../src/model/release-version.hpp"
#include "../src/controller/update-checker.hpp"
#include "../src/controller/output-sequencer.hpp"
#include "../src/obs/startable-port.hpp"
#include "../src/obs/update-port.hpp"
#include "../src/obs/output-port.hpp"
#include "../src/state/output-states.hpp"
#include "../src/obs/obs-api.hpp"
#include "../src/state/link-state.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QThread>
#include <cstdio>
#include <thread>
#include <vector>

using namespace fiji;

static int failures = 0;

static void check(bool ok, const char *what, int line)
{
	if (!ok) {
		std::printf("FAIL line %d: %s\n", line, what);
		failures++;
	}
}
#define CHECK(x) check((x), #x, __LINE__)

static void settle()
{
	for (int i = 0; i < 20; i++) {
		QCoreApplication::processEvents();
	}
}

// ---- a fake OBS ----------------------------------------------------------------------------------------
class FakeObs final : public obs::ObsApi, public obs::OutputBackend {
public:
	QStringList calls;
	obs::ObsCall next;

	model::ObsSnapshot snapshot() override
	{
		model::ObsSnapshot s;
		s.canvases = {model::CanvasState{"Main", true, "Gameplay", "", false, {"Gameplay", "BRB"}, {"Fade"}},
			      model::CanvasState{"Vertical", false, "Camera", "Full", true, {"Camera", "Full"}, {}}};
		s.outputs = {model::OutputStatus{"Stream", "stream", "", "", true, true, false, 90, 3, 1000},
			     model::OutputStatus{"TikTok", "rtmp", "Vertical", "Phones", false, false, false, 0, 0, 0}};
		s.decks = {model::DeckSummary{"d1", "Stream", 15}};
		s.running = {"b7"};
		return s;
	}
	obs::ObsCall record(const QString &call)
	{
		calls.append(call);
		auto result = next;
		next = obs::ObsCall();
		return result;
	}
	obs::ObsCall switchScene(const QString &scene, const QString &canvas) override { return record("switch " + scene + "@" + canvas); }
	obs::ObsCall setTransition(const QString &c, const QString &t) override { return record("transition " + c + " " + t); }
	obs::ObsCall runTransition(const QString &c) override { return record("run " + c); }
	QList<model::OutputStatus> outputs() override { return snapshot().outputs; }
	obs::ObsCall startAll() override { return record("start all"); }
	obs::ObsCall stopAll() override { return record("stop all"); }
	obs::ObsCall startGroup(const QString &g) override { return record("start group " + g); }
	obs::ObsCall stopGroup(const QString &g) override { return record("stop group " + g); }
	obs::ObsCall startMain(const QString &k) override { return record("start main " + k); }
	obs::ObsCall stopMain(const QString &k) override { return record("stop main " + k); }
	obs::ObsCall startOutput(const QString &n) override { return record("start " + n); }
	obs::ObsCall stopOutput(const QString &n) override { return record("stop " + n); }
	obs::ObsCall webFile(const QString &n) override { return record("file " + n); }
	obs::ObsCall look() override { return record("look"); }
};

// ---- the link state machine ----------------------------------------------------------------------------
static void test_link_states()
{
	using state::LinkReason;
	using state::LinkStateId;
	state::LinkStateMachine m;
	QList<LinkStateId> seen;
	QList<bool> twitch;
	QObject::connect(&m, &state::LinkStateMachine::stateChanged, [&](LinkStateId s) { seen.append(s); });
	QObject::connect(&m, &state::LinkStateMachine::twitchFeaturesAvailableChanged, [&](bool on) { twitch.append(on); });

	CHECK(m.state() == LinkStateId::STANDALONE && m.lastReason() == LinkReason::NoAddress);
	CHECK(!m.twitchFeaturesAvailable());
	m.dropped();           // ignored: not a transition from STANDALONE
	m.helloCompleted({"scenes"});
	CHECK(m.state() == LinkStateId::STANDALONE && seen.isEmpty());

	m.addressSet("wss://bot/x");
	CHECK(m.state() == LinkStateId::CONNECTING && m.lastReason() == LinkReason::None);
	m.failed(LinkReason::Refused, "connection refused"); // passing: keeps CONNECTING
	CHECK(m.state() == LinkStateId::CONNECTING && m.lastDetail() == "connection refused");
	m.helloCompleted({"scenes", "chat"});
	CHECK(m.state() == LinkStateId::LINKED && m.twitchFeaturesAvailable() && m.accepts("chat"));
	m.dropped();
	CHECK(m.state() == LinkStateId::CONNECTING && m.lastReason() == LinkReason::Dropped);
	CHECK(m.acceptedCapabilities().isEmpty() && !m.twitchFeaturesAvailable());
	m.helloCompleted({"scenes"});
	m.failed(LinkReason::VersionMismatch, "2.0"); // final: STANDALONE with the reason
	CHECK(m.state() == LinkStateId::STANDALONE && m.lastReason() == LinkReason::VersionMismatch);
	m.addressSet("wss://bot/x");
	m.failed(LinkReason::BadAddress, "no scheme");
	CHECK(m.state() == LinkStateId::STANDALONE && m.lastReason() == LinkReason::BadAddress);
	m.addressSet("wss://bot/x");
	m.helloCompleted({});
	m.addressCleared();
	CHECK(m.state() == LinkStateId::STANDALONE && m.lastReason() == LinkReason::AddressCleared);
	CHECK((seen == QList<LinkStateId>{LinkStateId::CONNECTING, LinkStateId::LINKED, LinkStateId::CONNECTING,
					  LinkStateId::LINKED, LinkStateId::STANDALONE, LinkStateId::CONNECTING,
					  LinkStateId::STANDALONE, LinkStateId::CONNECTING, LinkStateId::LINKED,
					  LinkStateId::STANDALONE}));
	CHECK((twitch == QList<bool>{true, false, true, false, true, false}));
}

// ---- the dispatcher ------------------------------------------------------------------------------------
class Recorder final : public controller::CommandHandler {
public:
	QStringList ran;
	QList<Qt::HANDLE> threads;
	QStringList actions() const override { return {"a.one", "a.two"}; }
	controller::CommandResult handle(const controller::Command &c) override
	{
		ran.append(c.action + ":" + c.payload.value("n").toString());
		threads.append(QThread::currentThreadId());
		return {};
	}
};

static void test_dispatcher()
{
	controller::CommandDispatcher d;
	Recorder r;
	d.registerHandler(&r);
	QList<controller::CommandResult> replies;
	std::thread other([&d, &replies] {
		for (int i = 0; i < 3; i++) {
			d.post({"a.one", {{"n", QString::number(i)}}, controller::CommandSource::Bridge, {}},
			       [&replies](const controller::CommandResult &res) { replies.append(res); });
		}
	});
	other.join();
	d.post({"a.nope", {}, controller::CommandSource::Ui, {}},
	       [&replies](const controller::CommandResult &res) { replies.append(res); });
	CHECK(d.pending() == 4);
	CHECK(r.ran.isEmpty()); // nothing runs on the posting thread
	settle();
	CHECK((r.ran == QStringList{"a.one:0", "a.one:1", "a.one:2"}));
	for (auto t : r.threads) {
		CHECK(t == QThread::currentThreadId()); // all on the consumer (this) thread
	}
	CHECK(replies.size() == 4 && replies[3].code == "UNKNOWN_ACTION" && !replies[3].ok);
	CHECK(d.pending() == 0);
}

// ---- the envelope --------------------------------------------------------------------------------------
static void test_envelope()
{
	bridge::Envelope e;
	e.type = bridge::MessageType::COMMAND;
	e.id = "c-1";
	e.action = "scene.switch";
	e.payload = {{"scene", "BRB"}};
	const auto back = bridge::Envelope::parse(e.serialise());
	CHECK(back && back->type == e.type && back->id == "c-1" && back->payload == e.payload && back->version == "1.2");
	bridge::Envelope event;
	event.action = "deck.changed";
	CHECK(QJsonDocument::fromJson(event.serialise()).object().value("id").isNull());
	for (const char *bad : {"not json", "[]", R"({"type":"EVENT","action":"x"})",
				R"({"version":"1.2","type":"NOPE","action":"x"})", R"({"version":"1.2","type":"EVENT"})",
				R"({"version":"1.2","type":"EVENT","action":"x","payload":[]})"}) {
		CHECK(!bridge::Envelope::parse(bad));
	}
	CHECK(bridge::catalogue().version == bridge::kContractVersion);
	CHECK(bridge::catalogue().actions.value("scene.switch").defines("canvas"));
}

// ---- the ACL -------------------------------------------------------------------------------------------
struct Wire {
	std::vector<QJsonObject> sent;
	QJsonObject last(const QString &type, const QString &action = QString()) const
	{
		for (auto it = sent.rbegin(); it != sent.rend(); ++it) {
			if (it->value("type").toString() == type && (action.isEmpty() || it->value("action").toString() == action)) {
				return *it;
			}
		}
		return {};
	}
	int count(const QString &type, const QString &action) const
	{
		int n = 0;
		for (const auto &m : sent) {
			n += m.value("type").toString() == type && m.value("action").toString() == action;
		}
		return n;
	}
};

static QByteArray frame(const QString &type, const QString &action, const QJsonObject &payload, const QString &id = {},
			const QString &version = "1.2")
{
	return QJsonDocument(QJsonObject{{"version", version}, {"id", id}, {"type", type}, {"action", action}, {"payload", payload}})
		.toJson(QJsonDocument::Compact);
}

// Stands in for the deck controller in the bridge test.
class PressRecorder final : public controller::CommandHandler {
public:
	QStringList pressed;
	QStringList actions() const override { return {"deck.press"}; }
	controller::CommandResult handle(const controller::Command &c) override
	{
		pressed.append(c.payload.value("button").toString() + " " + c.payload.value("vars").toObject().value("user").toString());
		return {};
	}
};

static void test_acl()
{
	controller::CommandDispatcher dispatcher;
	FakeObs obs;
	controller::ObsCommandHandler handler(obs);
	dispatcher.registerHandler(&handler);
	PressRecorder presses;
	dispatcher.registerHandler(&presses);
	state::OutputStates outputStates;
	controller::OutputController outputs(obs, outputStates);
	dispatcher.registerHandler(&outputs);
	state::LinkStateMachine link;
	bridge::BridgeAcl acl(dispatcher, link, "0.6.0");
	Wire wire;
	bool refused = false;
	acl.send = [&wire](const QByteArray &f) { wire.sent.push_back(QJsonDocument::fromJson(f).object()); };
	acl.refuse = [&refused] { refused = true; };

	link.addressSet("wss://bot/x");
	acl.onOpen();
	const auto hello = wire.last("HELLO");
	CHECK(hello.value("payload").toObject().value("capabilities").toArray().size() == 8);
	CHECK(hello.value("payload").toObject().value("app_version").toString() == "0.6.0");
	CHECK(hello.value("version").toString() == "1.2");

	acl.onFrame(frame("COMMAND", "scene.switch", {{"scene", "BRB"}}, "early")); // before the handshake: dropped
	settle();
	CHECK(wire.sent.size() == 1 && obs.calls.isEmpty());

	acl.onFrame(frame("HELLO", "link.hello",
			  {{"role", "bot"}, {"app", "fiji-chan"}, {"app_version", "1.4.0"},
			   {"capabilities", QJsonArray{"scenes", "outputs", "output.alerts", "decks", "chat"}},
			   {"accepted", QJsonArray{"scenes", "outputs", "output.alerts", "decks", "chat", "warp"}}},
			  hello.value("id").toString(), "1.0"));
	settle();
	CHECK(acl.linked() && link.state() == state::LinkStateId::LINKED);
	CHECK((link.acceptedCapabilities() == QStringList{"scenes", "outputs", "output.alerts", "decks", "chat"}));
	const auto snapshot = wire.last("SYNC_STATE").value("payload").toObject();
	const auto main = snapshot.value("canvases").toArray().at(0).toObject();
	CHECK(main.value("canvas").toString() == "Main" && main.value("main").toBool() && main.value("preview").isNull());
	const auto stream = snapshot.value("outputs").toArray().at(0).toObject();
	CHECK(stream.value("state").toString() == "ACTIVE" && stream.value("uptime").toInt() == 90 && stream.value("main").toBool());
	CHECK(snapshot.value("decks").toArray().size() == 1 && snapshot.value("running").toArray().at(0).toString() == "b7");

	// commands -> the handler -> ACKs
	acl.onFrame(frame("COMMAND", "scene.switch", {{"scene", "BRB"}, {"fade", true}}, "c-1"));
	acl.onFrame(frame("COMMAND", "scene.switch", {{"scene", "Full"}, {"canvas", "Vertical"}}, "c-2"));
	settle();
	CHECK((obs.calls == QStringList{"switch BRB@", "switch Full@Vertical"}));
	auto ack = wire.last("ACK", "scene.switch");
	CHECK(ack.value("id").toString() == "c-2" && ack.value("payload").toObject().value("ok").toBool());

	acl.onFrame(frame("COMMAND", "output.start", {{"target", "all"}}, "c-3"));
	settle();
	const auto started = wire.last("ACK", "output.start").value("payload").toObject().value("result").toObject();
	CHECK(started.value("started").toArray().size() == 2 && obs.calls.contains("start all"));
	CHECK(outputStates.state("TikTok") == state::OutputStateId::STARTING);
	CHECK(outputStates.state("Stream") == state::OutputStateId::IDLE); // already live (the fake says so) is untouched by asking

	acl.onFrame(frame("COMMAND", "output.stop", {{"target", "Nope"}}, "c-4"));
	acl.onFrame(frame("COMMAND", "output.stop", {}, "c-5"));                       // target missing
	acl.onFrame(frame("COMMAND", "transition.run", {{"canvas", "Vertical"}}, "c-6")); // transitions not accepted
	acl.onFrame(frame("COMMAND", "scene.teleport", {}, "c-7"));
	acl.onFrame(frame("COMMAND", "output.state", {}, "c-8")); // an EVENT, not a COMMAND
	settle();
	const auto error = [&wire](const QString &id) {
		for (const auto &m : wire.sent) {
			if (m.value("type").toString() == "ACK" && m.value("id").toString() == id) {
				return m.value("payload").toObject().value("error").toObject().value("code").toString();
			}
		}
		return QString("no ACK");
	};
	CHECK(error("c-4") == "NOT_FOUND");
	CHECK(error("c-5") == "BAD_REQUEST");
	CHECK(error("c-6") == "UNKNOWN_ACTION" && !obs.calls.contains("run Vertical"));
	CHECK(error("c-7") == "UNKNOWN_ACTION" && error("c-8") == "UNKNOWN_ACTION");

	acl.onFrame(frame("COMMAND", "deck.press", {{"button", "b1"}, {"vars", QJsonObject{{"user", "Ana"}}}}, "c-9"));
	settle();
	CHECK((presses.pressed == QStringList{"b1 Ana"}));

	// state.sync: the ACK, then a SYNC_STATE with the command's id
	const int snapshots = wire.count("SYNC_STATE", "state.snapshot");
	acl.onFrame(frame("COMMAND", "state.sync", {}, "sync-1"));
	settle();
	CHECK(wire.count("SYNC_STATE", "state.snapshot") == snapshots + 1);
	CHECK(wire.last("SYNC_STATE").value("id").toString() == "sync-1");
	CHECK(wire.last("ACK", "state.sync").value("id").toString() == "sync-1");

	// the plugin's own events -> contract EVENTs
	acl.publishOutputState("Stream", "FAILED", "Disconnected");
	auto state = wire.last("EVENT", "output.state").value("payload").toObject();
	CHECK(state.value("state").toString() == "FAILED" && state.value("kind").toString() == "stream");
	CHECK(state.value("error").toString() == "Disconnected");
	acl.publishOutputState("TikTok", "STARTING");
	CHECK(wire.last("EVENT", "output.state").value("payload").toObject().value("state").toString() == "STARTING");
	CHECK(!wire.last("EVENT", "output.state").value("payload").toObject().contains("error"));
	acl.onPluginEvent("switch_scene", {{"canvas", "Vertical"}, {"new_scene", "Full"}, {"old_scene", "Camera"}, {"width", 1080}});
	const auto program = wire.last("EVENT", "scene.program").value("payload").toObject();
	CHECK(program.value("scene").toString() == "Full" && program.value("previous").toString() == "Camera" && !program.contains("width"));
	acl.onPluginEvent("say", {{"text", QString(600, QChar('x'))}});
	CHECK(wire.last("EVENT", "chat.say").value("payload").toObject().value("text").toString().size() == 480);
	acl.onPluginEvent("button_state", {{"button", "b1"}, {"running", true}});
	CHECK(wire.last("EVENT", "deck.key").value("payload").toObject().value("running").toBool());
	CHECK(wire.last("EVENT", "deck.key").value("id").isNull());
	const int before = wire.count("SYNC_STATE", "state.snapshot");
	acl.onPluginEvent("outputs_changed", {});
	settle();
	CHECK(wire.count("SYNC_STATE", "state.snapshot") == before + 1); // no event covers it: a fresh snapshot
	acl.onPluginEvent("studio_mode_changed", {{"enabled", true}});     // not in the contract: nothing sent
	CHECK(wire.last("EVENT", "studio_mode_changed").isEmpty());

	acl.reportDroppedFrames();
	settle();
	CHECK(wire.count("EVENT", "output.dropped") == 1); // only the active output
	CHECK(wire.last("EVENT", "output.dropped").value("payload").toObject().value("dropped_frames").toInt() == 3);

	// the connection drops: CONNECTING, and nothing more is sent
	acl.onClosed();
	settle();
	CHECK(!acl.linked() && link.state() == state::LinkStateId::CONNECTING);
	const auto sentBefore = wire.sent.size();
	acl.onPluginEvent("say", {{"text", "hi"}});
	CHECK(wire.sent.size() == sentBefore);

	// a bot speaking another major version turns the plugin away: STANDALONE, and no more retries
	acl.onOpen();
	acl.onFrame(frame("HELLO", "link.hello", {{"accepted", QJsonArray{}}, {"reject", "version"}}, "h", "2.0"));
	settle();
	CHECK(refused && !acl.linked() && link.state() == state::LinkStateId::STANDALONE);
	CHECK(link.lastReason() == state::LinkReason::VersionMismatch);
}

static void test_handler_rejects_bad_payloads()
{
	FakeObs obs;
	controller::ObsCommandHandler handler(obs);
	CHECK(handler.handle({"scene.switch", {}, controller::CommandSource::Ui, {}}).code == "BAD_REQUEST");
	CHECK(handler.handle({"deck.save", {{"deck", "x"}}, controller::CommandSource::Ui, {}}).code == "UNKNOWN_ACTION"); // the decks' own
	const auto snapshot = handler.handle({"state.snapshot", {}, controller::CommandSource::Ui, {}});
	CHECK(snapshot.ok && model::ObsSnapshot::fromJson(snapshot.result).outputs.size() == 2);
	CHECK(model::ObsSnapshot::fromJson(snapshot.result).canvases.at(1).studioMode);
	CHECK(obs.calls.isEmpty());
}


// ---- the decks -----------------------------------------------------------------------------------------
class MemoryStore final : public model::LocalStore {
public:
	QHash<QString, QJsonObject> saved;
	int saves = 0;
	QJsonObject load(const QString &key) const override { return saved.value(key); }
	bool save(const QString &key, const QJsonObject &value) override
	{
		saved.insert(key, value);
		saves++;
		return true;
	}
};

class FakeSteps final : public obs::DeckSteps {
public:
	QStringList ran;
	QStringList stopped;
	bool monitoring = false;
	void run(const QJsonObject &step) override { ran.append(step.value("type").toString() + ":" + step.value("scene").toString()); }
	void stopSounds(const QString &name) override { stopped.append(name.isEmpty() ? "*" : name); }
	void setSoundMonitoring(bool on) override { monitoring = on; }
};

class FakeHotkeys final : public obs::DeckHotkeys {
public:
	QStringList registered;
	QHash<QString, QJsonArray> bound;
	int unregistered = 0;
	void registerAll(const QJsonArray &decks) override
	{
		registered.clear();
		for (const auto &d : decks) {
			for (const auto &b : d.toObject().value("buttons").toArray()) {
				registered.append(b.toObject().value("id").toString());
			}
		}
	}
	void unregisterAll() override
	{
		registered.clear();
		unregistered++;
	}
	QHash<QString, QJsonArray> bindings() const override { return bound; }
};

static QJsonObject button(const char *id, QJsonArray steps, QJsonArray triggers = {})
{
	return {{"id", id}, {"text", id}, {"steps", steps}, {"triggers", triggers}};
}

static void test_deck_model()
{
	MemoryStore store;
	store.saved.insert("decks", QJsonObject{{"version", 1}, {"sound_monitoring", false},
					       {"decks", QJsonArray{QJsonObject{{"id", "d1"}, {"buttons", QJsonArray{button("b1", {})}}}}}});
	model::DeckModel m(store);
	m.load();
	CHECK(m.decks().size() == 1 && !m.soundMonitoring() && m.hasButton("b1") && !m.hasButton("b9"));
	int changes = 0;
	QObject::connect(&m, &model::DeckModel::decksChanged, [&] { changes++; });

	// a new deck gets an id, and a button id already used by another deck is replaced
	const auto saved = m.saveDeck(QJsonObject{{"name", "Two"},
						  {"buttons", QJsonArray{button("b1", {}), button("", {}), button("b3", {}, QJsonArray{QJsonObject{{"type", "sub"}}})}}});
	const auto ids = saved.value("buttons").toArray();
	CHECK(!saved.value("id").toString().isEmpty() && changes == 1 && store.saves == 1);
	CHECK(ids.at(0).toObject().value("id").toString() != "b1" && !ids.at(1).toObject().value("id").toString().isEmpty());
	CHECK(ids.at(2).toObject().value("id").toString() == "b3");
	CHECK(store.saved.value("decks").value("decks").toArray().size() == 2 && store.saved.value("decks").value("version").toInt() == 1);
	const auto triggers = m.triggers();
	CHECK(triggers.size() == 1 && triggers.at(0).toObject().value("button").toString() == "b3");
	CHECK(triggers.at(0).toObject().value("deck").toString() == saved.value("id").toString());

	// saving the same deck again replaces it, keeping its own button ids
	auto again = saved;
	again.insert("name", "Renamed");
	CHECK(m.saveDeck(again).value("buttons") == saved.value("buttons") && m.decks().size() == 2);

	CHECK(m.deleteDeck("d1", {{"b3", QJsonArray{QJsonObject{{"key", "OBS_KEY_F5"}}}}}) && !m.deleteDeck("d1"));
	CHECK(m.decks().size() == 1 && changes == 3);
	CHECK(store.saved.value("decks").value("decks").toArray().at(0).toObject().value("buttons").toArray().at(2).toObject().value("hotkey").toArray().size() == 1);
}

static void test_key_states()
{
	using state::KeyInput;
	using state::KeyState;
	CHECK(state::next(KeyState::IDLE, KeyInput::Pressed) == KeyState::HELD);
	CHECK(state::next(KeyState::HELD, KeyInput::RunStarted) == KeyState::HELD_RUNNING);
	CHECK(state::next(KeyState::HELD_RUNNING, KeyInput::Released) == KeyState::RUNNING);
	CHECK(state::next(KeyState::RUNNING, KeyInput::RunEnded) == KeyState::IDLE);
	CHECK(state::next(KeyState::HELD_RUNNING, KeyInput::RunEnded) == KeyState::HELD);
	CHECK(state::next(KeyState::IDLE, KeyInput::Released) == KeyState::IDLE); // not a transition: nothing changes
	state::DeckKeys keys;
	QStringList seen;
	QObject::connect(&keys, &state::DeckKeys::keyStateChanged,
			 [&](const QString &b, KeyState s) { seen.append(b + "=" + state::keyStateName(s)); });
	keys.input("b1", KeyInput::Pressed);
	keys.input("b1", KeyInput::Pressed);
	keys.input("b1", KeyInput::Released);
	keys.input("b2", KeyInput::RunStarted);
	keys.clear();
	CHECK((seen == QStringList{"b1=held", "b1=idle", "b2=running", "b2=idle"}));
	CHECK(keys.state("b2") == KeyState::IDLE);
}

static void test_deck_controller()
{
	MemoryStore store;
	store.saved.insert("decks", QJsonObject{{"decks", QJsonArray{QJsonObject{
		{"id", "d1"}, {"name", "Stream"},
		{"buttons", QJsonArray{
			button("brb", QJsonArray{QJsonObject{{"type", "scene"}, {"scene", "BRB"}, {"at", 0}},
						 QJsonObject{{"type", "chat"}, {"text", "brb, /$user$/!"}, {"at", 0}},
						 QJsonObject{{"type", "notify"}, {"text", "on a break"}, {"at", 100}}}),
			button("long", QJsonArray{QJsonObject{{"type", "wait"}, {"ms", 1000}}, QJsonObject{{"type", "scene"}, {"scene", "Live"}}})}}}}}});
	model::DeckModel model(store);
	state::DeckKeys keys;
	FakeSteps steps;
	FakeHotkeys hotkeys;
	controller::DeckController decks(model, keys, steps, hotkeys);
	qint64 clock = 0;
	decks.setClock([&clock] { return clock; });
	QStringList said, notes, changes;
	QObject::connect(&decks, &controller::DeckController::say, [&](const QString &t) { said.append(t); });
	QObject::connect(&decks, &controller::DeckController::notice, [&](const QString &t) { notes.append(t); });
	QObject::connect(&decks, &controller::DeckController::keyChanged,
			 [&](const QString &b, bool r) { changes.append(b + (r ? "+" : "-")); });
	decks.start();
	CHECK(steps.monitoring && (hotkeys.registered == QStringList{"brb", "long"}));

	const auto cmd = [](const char *action, QJsonObject payload, controller::CommandSource source = controller::CommandSource::Bridge) {
		return controller::Command{QString::fromLatin1(action), payload, source, {}};
	};
	CHECK(decks.handle(cmd("deck.press", {{"button", "brb"}, {"vars", QJsonObject{{"user", "Ana"}}}})).ok);
	CHECK((steps.ran == QStringList{"scene:BRB"}) && (said == QStringList{"brb, Ana!"}) && notes.isEmpty());
	clock = 150;
	decks.tick();
	CHECK((notes == QStringList{"on a break"}) && (changes == QStringList{"brb+", "brb-"}));
	CHECK(decks.handle(cmd("deck.press", {{"button", "nope"}})).code == "NOT_FOUND");

	// a hotkey holds the key down; the state shows it
	CHECK(decks.handle(cmd("deck.press", {{"button", "long"}}, controller::CommandSource::Hotkey)).ok);
	CHECK(keys.state("long") == state::KeyState::HELD_RUNNING);
	CHECK(decks.handle(cmd("deck.press", {{"button", "long"}})).code == "BUSY");
	auto stateNow = decks.handle(cmd("deck.state", {})).result;
	CHECK(stateNow.value("running").toArray().at(0).toString() == "long" && stateNow.value("keys").toObject().value("long").toString() == "held_running");
	decks.handle(cmd("deck.release", {{"button", "long"}}, controller::CommandSource::Hotkey));
	CHECK(keys.state("long") == state::KeyState::RUNNING);
	CHECK(decks.handle(cmd("deck.stop", {})).code == "BAD_REQUEST");
	CHECK(decks.handle(cmd("deck.stop", {{"button", "all"}})).ok && (steps.stopped == QStringList{"*"}));
	CHECK(keys.state("long") == state::KeyState::IDLE);

	// editing keeps what was bound in OBS and registers the hotkeys again
	hotkeys.bound.insert("brb", QJsonArray{QJsonObject{{"key", "OBS_KEY_F1"}}});
	auto deck = model.decks().at(0).toObject();
	deck.insert("name", "Renamed");
	const auto saved = decks.handle(cmd("deck.save", {{"deck", deck}}));
	CHECK(saved.ok && saved.result.value("deck").toObject().value("name").toString() == "Renamed");
	CHECK(hotkeys.unregistered >= 1 && (hotkeys.registered == QStringList{"brb", "long"}));
	CHECK(store.saved.value("decks").value("decks").toArray().at(0).toObject().value("buttons").toArray().at(0).toObject().value("hotkey").toArray().size() == 1);
	CHECK(decks.handle(cmd("deck.save", {})).code == "BAD_REQUEST" && (hotkeys.registered == QStringList{"brb", "long"}));
	CHECK(decks.handle(cmd("deck.delete", {{"id", "zz"}})).code == "NOT_FOUND");
	CHECK(decks.handle(cmd("deck.delete", {{"id", "d1"}})).ok && hotkeys.registered.isEmpty());
	CHECK(decks.handle(cmd("deck.list", {})).result.value("decks").toArray().isEmpty());

	// the dispatcher's call runs it right away on its own thread
	controller::CommandDispatcher dispatcher;
	dispatcher.registerHandler(&decks);
	CHECK(dispatcher.call(cmd("deck.list", {})).ok && dispatcher.call(cmd("deck.nope", {})).code == "UNKNOWN_ACTION");
	decks.shutdown();
	CHECK(hotkeys.registered.isEmpty());
}

// ---- outputs ---------------------------------------------------------------------------------------
static void test_output_targets()
{
	using Kind = model::OutputPlan::Kind;
	QList<model::OutputStatus> list{{"Stream", "stream", "", "", true, true, false, 0, 0, 0},
					{"TikTok", "rtmp", "Vertical", "Phones", false, false, false, 0, 0, 0},
					{"Shorts", "rtmp", "Vertical", "Phones", false, false, false, 0, 0, 0},
					{"Backup", "rtmp", "", "", false, false, false, 0, 0, 0}};
	auto all = model::planOutputs("all", list);
	CHECK(all.kind == Kind::All && all.names.size() == 4);
	auto group = model::planOutputs("Phones", list);
	CHECK(group.kind == Kind::Group && (group.names == QStringList{"TikTok", "Shorts"}));
	auto byKind = model::planOutputs("stream", list);
	CHECK(byKind.kind == Kind::Main && byKind.mainKind == "stream" && (byKind.names == QStringList{"Stream"}));
	auto byName = model::planOutputs("Stream", list);
	CHECK(byName.kind == Kind::Main && byName.mainKind == "stream");
	auto record = model::planOutputs("record", list);
	CHECK(record.kind == Kind::Main && (record.names == QStringList{"record"}));
	CHECK(model::planOutputs("Backup", list).kind == Kind::Named);
	CHECK(model::planOutputs("Nope", list).kind == Kind::None);
}

static void test_output_states()
{
	using I = state::OutputInput;
	using S = state::OutputStateId;
	CHECK(state::nextOutputState(S::IDLE, I::StartAsked) == S::STARTING);
	CHECK(state::nextOutputState(S::STARTING, I::Started) == S::ACTIVE);
	CHECK(state::nextOutputState(S::ACTIVE, I::Reconnecting) == S::RECONNECTING);
	CHECK(state::nextOutputState(S::RECONNECTING, I::Reconnected) == S::ACTIVE);
	CHECK(state::nextOutputState(S::ACTIVE, I::StopAsked) == S::STOPPING);
	CHECK(state::nextOutputState(S::STOPPING, I::Stopped) == S::IDLE);
	CHECK(state::nextOutputState(S::ACTIVE, I::Failed) == S::FAILED);
	CHECK(state::nextOutputState(S::FAILED, I::StartAsked) == S::STARTING);
	CHECK(state::nextOutputState(S::STOPPING, I::StartAsked) == S::STOPPING); // not a transition
	state::OutputStates states;
	QStringList seen;
	QObject::connect(&states, &state::OutputStates::stateChanged, [&](const QString &o, S s, const QString &e) {
		seen.append(o + "=" + state::outputStateName(s) + (e.isEmpty() ? "" : "(" + e + ")"));
	});
	states.input("Twitch", I::StartAsked);
	states.onPluginEvent("start_output", "Twitch", "");
	states.onPluginEvent("output_reconnecting", "Twitch", "");
	states.onPluginEvent("output_reconnected", "Twitch", "");
	states.onPluginEvent("stop_output", "Twitch", "Disconnected");
	states.onPluginEvent("start_output", "Local", ""); // started outside the plugin
	states.onPluginEvent("stop_output", "Local", "");
	states.onPluginEvent("canvas_changed", "Twitch", "");
	CHECK((seen == QStringList{"Twitch=STARTING", "Twitch=ACTIVE", "Twitch=RECONNECTING", "Twitch=ACTIVE",
				   "Twitch=FAILED(Disconnected)", "Local=ACTIVE", "Local=IDLE"}));
	CHECK(states.lastError("Twitch") == "Disconnected" && states.state("Local") == S::IDLE);
}

static void test_output_controller()
{
	FakeObs obs;
	state::OutputStates states;
	controller::OutputController outputs(obs, states);
	const auto run = [&](const char *action, const char *target) {
		return outputs.handle({action, target ? QJsonObject{{"target", target}} : QJsonObject{}, controller::CommandSource::Ui, {}});
	};
	CHECK(run("output.start", "stream").ok && obs.calls.last() == "start main stream");
	CHECK(run("output.stop", "TikTok").result.value("stopped").toArray().at(0).toString() == "TikTok");
	CHECK(obs.calls.last() == "stop TikTok" && states.state("TikTok") == state::OutputStateId::IDLE); // idle stays idle
	CHECK(run("output.start", "Phones").ok && obs.calls.last() == "start group Phones");
	CHECK(states.state("TikTok") == state::OutputStateId::STARTING);
	CHECK(run("output.start", "Nope").code == "NOT_FOUND" && run("output.start", nullptr).code == "BAD_REQUEST");
	obs.next = obs::ObsCall::failed("OBS_ERROR", "no encoder");
	const auto refused = run("output.stop", "all");
	CHECK(!refused.ok && refused.message == "no encoder" && states.state("TikTok") == state::OutputStateId::STARTING);
}

// ---- the look ------------------------------------------------------------------------------------------
static void test_themes()
{
	const QMap<QString, QByteArray> shipped{{"Fiji_Sun.obt", "sun 2"}, {"Fiji_Moon.obt", "moon"}, {"FijiSun/check.svg", "icon"},
						{"README.txt", "not a theme"}};
	const QMap<QString, QByteArray> fresh = {};
	auto first = model::planThemes(shipped, fresh);
	CHECK(first.write.size() == 3 && first.added == 2 && first.remove.isEmpty()); // icons do not count as themes
	const QMap<QString, QByteArray> installed{{"Fiji_Sun.obt", "sun 1"}, {"Fiji_Moon.obt", "moon"}, {"FijiSun/check.svg", "icon"},
						  {"Fiji_Sun.ovt", "old style"}, {"Someone_Else.obt", "theirs"}};
	auto update = model::planThemes(shipped, installed);
	CHECK((update.write == QStringList{"Fiji_Sun.obt"}) && update.added == 0);
	CHECK((update.remove == QStringList{"Fiji_Sun.ovt"})); // only our retired files; never someone else's theme
	CHECK(model::userThemesDir("/home/a/.config/obs-studio/plugin_config/fiji-suite/") == "/home/a/.config/obs-studio/themes");
	CHECK(model::userThemesDir("C:/Users/a/AppData/Roaming/obs-studio/plugin_config/fiji-suite") ==
	      "C:/Users/a/AppData/Roaming/obs-studio/themes");
}

// ---- the update check -----------------------------------------------------------------------------------
class FakeFeed final : public obs::ReleaseFeed {
public:
	QString tag;
	int asked = 0;
	void fetchLatest(std::function<void(const QString &)> done) override
	{
		asked++;
		done(tag);
	}
	void stop() override {}
};

class FakeUpdateSettings final : public obs::UpdateSettings {
public:
	bool on = true;
	QString skipped;
	bool checkOnStart() const override { return on; }
	QString skippedVersion() const override { return skipped; }
	void skipVersion(const QString &v) override { skipped = v; }
};

static void test_updates()
{
	CHECK(model::isNewer("v1.2.0", "1.1.9") && model::isNewer("1.10.0", "1.9.9") && !model::isNewer("1.2.0", "1.2.0"));
	CHECK(model::isNewer("1.2.0", "1.2.0-beta1") && !model::isNewer("1.2.0-beta2", "1.2.0"));
	CHECK(model::isNewer("1.2.1+build5", "1.2.0") && !model::isNewer("1.2", "1.0.0") && !model::isNewer("nightly", "1.0.0"));
	CHECK(model::bareVersion("v0.9.0") == "0.9.0");

	const auto outcomeOf = [](const QString &tag, FakeUpdateSettings &settings, QStringList *offers = nullptr) {
		FakeFeed feed;
		feed.tag = tag;
		controller::UpdateChecker checker(feed, settings, "0.8.0");
		QObject::connect(&checker, &controller::UpdateChecker::offerUpdate,
				 [offers](const QString &latest, const QString &) { if (offers) offers->append(latest); });
		checker.start();
		checker.start(); // once per start
		CHECK(feed.asked <= 1);
		return checker.outcome();
	};
	FakeUpdateSettings settings;
	QStringList offers;
	CHECK(outcomeOf("v0.9.0", settings, &offers) == "offered" && (offers == QStringList{"0.9.0"}));
	CHECK(outcomeOf("v0.8.0", settings) == "up to date" && outcomeOf("", settings) == "failed");
	settings.skipVersion("0.9.0");
	CHECK(outcomeOf("v0.9.0", settings) == "skipped" && outcomeOf("v0.9.1", settings) == "offered");
	settings.on = false;
	CHECK(outcomeOf("v0.9.1", settings) == "off");
}

// ---- starting outputs one after another ----------------------------------------------------------------------
class FakeStartables final : public obs::StartableOutputs {
public:
	QStringList tried;
	QSet<QString> refuse;    // these cannot start now
	QSet<QString> liveAtOnce; // these are live already
	QHash<QString, std::function<void()>> pending;
	bool startOutput(const QString &id, std::function<void()> onLive) override
	{
		tried.append(id);
		if (refuse.contains(id)) {
			return false;
		}
		if (liveAtOnce.contains(id)) {
			onLive();
		} else {
			pending.insert(id, onLive);
		}
		return true;
	}
	void live(const QString &id) { pending.take(id)(); }
};

static void test_output_sequence()
{
	model::StartSequence s;
	s.reset({"a", "b", "c"});
	CHECK(s.current() == "a");
	s.accepted();
	CHECK(s.current() == "b" && s.refused() && s.current() == "c");
	s.accepted();
	CHECK(s.current() == "b" && !s.refused() && s.empty()); // b refused for a whole round: given up
	CHECK((model::groupsOf({"", "Phones", "PC", "Phones"}) == QStringList{"Phones", "PC"}));

	FakeStartables outputs;
	controller::OutputSequencer sequencer(outputs);
	QStringList gaveUp;
	QObject::connect(&sequencer, &controller::OutputSequencer::gaveUp, [&](const QStringList &ids) { gaveUp = ids; });
	outputs.liveAtOnce = {"main:stream"};
	outputs.refuse = {"output:Busy"};
	sequencer.start({"main:stream", "output:TikTok", "output:Busy", "output:Shorts"});
	CHECK((outputs.tried == QStringList{"main:stream"})); // the next waits until this one is live (queued)
	settle();
	CHECK((outputs.tried == QStringList{"main:stream", "output:TikTok"})); // one at a time: TikTok is not live yet
	outputs.live("output:TikTok");
	settle();
	CHECK((outputs.tried == QStringList{"main:stream", "output:TikTok", "output:Busy", "output:Shorts"}));
	outputs.refuse.remove("output:Busy");
	outputs.live("output:Shorts");
	settle();
	CHECK(outputs.tried.last() == "output:Busy" && sequencer.waiting().isEmpty()); // tried again, and it started
	outputs.refuse = {"output:A", "output:B"};
	sequencer.start({"output:A", "output:B"});
	CHECK((gaveUp == QStringList{"output:A", "output:B"}) && sequencer.waiting().isEmpty());
	sequencer.start({"output:C", "output:D"});
	sequencer.stop(); // a stop: what still waits is forgotten, and C's going live starts nothing
	outputs.live("output:C");
	settle();
	CHECK(!outputs.tried.contains("output:D"));
}

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	test_link_states();
	test_dispatcher();
	test_envelope();
	test_acl();
	test_handler_rejects_bad_payloads();
	test_deck_model();
	test_key_states();
	test_deck_controller();
	test_output_targets();
	test_output_states();
	test_output_controller();
	test_themes();
	test_updates();
	test_output_sequence();
	if (failures) {
		std::printf("%d check(s) failed\n", failures);
		return 1;
	}
	std::printf("core: all checks passed\n");
	return 0;
}
