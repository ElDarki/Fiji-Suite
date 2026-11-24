// Tests for the MVC + State core with Qt Core only (no OBS): the link state machine, the command dispatcher, the
// bridge envelope and the bridge ACL driven through a fake ObsApi.
//   cmake --build build_x86_64 --target fiji-core-test && ctest --test-dir build_x86_64 -R core

#include "../src/bridge/bridge-acl.hpp"
#include "../src/bridge/catalogue.hpp"
#include "../src/bridge/envelope.hpp"
#include "../src/controller/capture-controller.hpp"
#include "../src/controller/command-dispatcher.hpp"
#include "../src/controller/deck-controller.hpp"
#include "../src/controller/outputs-controller.hpp"
#include "../src/model/deck-model.hpp"
#include "../src/model/local-store.hpp"
#include "../src/model/profile-model.hpp"
#include "../src/model/theme-files.hpp"
#include "../src/obs/deck-ports.hpp"
#include "../src/obs/obs-data-store.hpp"
#include "../src/controller/obs-command-handler.hpp"
#include "../src/obs/obs-api.hpp"
#include "../src/state/link-state.hpp"
#include "../src/state/output-state.hpp"

#include "fake-capture-sources.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QFile>
#include <QTemporaryDir>
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
class FakeObs final : public obs::ObsApi {
public:
	QStringList calls;
	obs::ObsCall next;

	model::ObsSnapshot snapshot() override
	{
		model::ObsSnapshot s;
		s.canvases = {model::CanvasState{"Main", true, "Gameplay", "", false, {"Gameplay", "BRB"}, {"Fade"}},
			      model::CanvasState{"Vertical", false, "Camera", "Full", true, {"Camera", "Full"}, {}}};
		s.outputs = {model::OutputStatus{"Stream", "stream", "", "", true, true, false, "", 90, 3, 1000},
			     model::OutputStatus{"TikTok", "rtmp", "Vertical", "Phones", false, false, false, "", 0, 0, 0}};
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
	obs::ObsCall startOutputs(const QString &t) override
	{
		auto r = record("start " + t);
		if (r.ok) {
			r.data = {{"started", QJsonArray{"Stream", "TikTok"}}, {"secret", 1}};
		}
		return r;
	}
	obs::ObsCall stopOutputs(const QString &t) override { return record("stop " + t); }
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

static void test_acl()
{
	controller::CommandDispatcher dispatcher;
	FakeObs obs;
	controller::ObsCommandHandler handler(obs);
	dispatcher.registerHandler(&handler);
	struct Decks final : controller::CommandHandler {
		QStringList *calls;
		QStringList actions() const override { return {"deck.press"}; }
		controller::CommandResult handle(const controller::Command &c) override
		{
			calls->append("press " + c.payload.value("button").toString() + " " +
				      c.payload.value("vars").toObject().value("user").toString());
			return {};
		}
	} decks;
	decks.calls = &obs.calls;
	dispatcher.registerHandler(&decks);
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
	CHECK(started.value("started").toArray().size() == 2 && !started.contains("secret")); // contract fields only

	obs.next = obs::ObsCall::failed("NOT_FOUND", "There is no output called Nope");
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
	CHECK(obs.calls.contains("press b1 Ana"));

	// state.sync: the ACK, then a SYNC_STATE with the command's id
	const int snapshots = wire.count("SYNC_STATE", "state.snapshot");
	acl.onFrame(frame("COMMAND", "state.sync", {}, "sync-1"));
	settle();
	CHECK(wire.count("SYNC_STATE", "state.snapshot") == snapshots + 1);
	CHECK(wire.last("SYNC_STATE").value("id").toString() == "sync-1");
	CHECK(wire.last("ACK", "state.sync").value("id").toString() == "sync-1");

	// the plugin's own events -> contract EVENTs
	acl.onPluginEvent("stop_output", {{"output", "Stream"}, {"last_error", "Disconnected"}, {"code", -1}});
	auto state = wire.last("EVENT", "output.state").value("payload").toObject();
	CHECK(state.value("state").toString() == "FAILED" && state.value("kind").toString() == "stream");
	CHECK(state.value("error").toString() == "Disconnected" && !state.contains("code"));
	acl.onPluginEvent("output_reconnecting", {{"output", "TikTok"}});
	CHECK(wire.last("EVENT", "output.state").value("payload").toObject().value("state").toString() == "RECONNECTING");
	acl.onPluginEvent("output_starting", {{"output", "TikTok"}, {"state", "STARTING"}});
	CHECK(wire.last("EVENT", "output.state").value("payload").toObject().value("state").toString() == "STARTING");
	acl.onPluginEvent("output_stopping", {{"output", "TikTok"}});
	CHECK(wire.last("EVENT", "output.state").value("payload").toObject().value("state").toString() == "STOPPING");
	acl.onPluginEvent("stop_output", {{"output", "TikTok"}, {"state", "IDLE"}, {"last_error", "ignored when idle"}});
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
	CHECK(handler.handle({"output.start", {}, controller::CommandSource::Ui, {}}).code == "BAD_REQUEST");
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
	bool monitoring = true;
	bool run(const QJsonObject &step) override
	{
		ran.append(step.value("type").toString() + ":" + step.value("scene").toString() + step.value("file").toString());
		return true;
	}
	void stopSounds(const QString &name) override { stopped.append(name.isEmpty() ? QStringLiteral("*") : name); }
	void setSoundMonitoring(bool on) override { monitoring = on; }
};

class FakeHotkeys final : public obs::DeckHotkeys {
public:
	int binds = 0;
	QStringList buttons;
	KeyFn key;
	QHash<QString, QJsonArray> bound;
	void bind(const QJsonArray &decks, KeyFn onKey) override
	{
		binds++;
		key = onKey;
		buttons.clear();
		for (const auto &d : decks) {
			for (const auto &b : d.toObject().value("buttons").toArray()) {
				buttons.append(b.toObject().value("id").toString());
			}
		}
	}
	QHash<QString, QJsonArray> bindings() const override { return bound; }
	void clear() override { buttons.clear(); }
};

static QJsonObject button(const QString &id, const QJsonArray &steps, const QJsonArray &triggers = {})
{
	return {{"id", id}, {"text", id.toUpper()}, {"steps", steps}, {"triggers", triggers}};
}

static void test_deck_model()
{
	MemoryStore store;
	model::DeckModel decks(store);
	int counter = 0;
	decks.setIdSource([&counter] { return QStringLiteral("id%1").arg(++counter); });
	int changes = 0;
	QObject::connect(&decks, &model::DeckModel::decksChanged, [&changes] { changes++; });

	decks.load();
	CHECK(decks.decks().isEmpty() && decks.soundMonitoring());
	const auto first = decks.saveDeck({{"name", "Stream"},
					   {"buttons", QJsonArray{button("b1", {}, QJsonArray{QJsonObject{{"type", "chat"}, {"message", "hi"}}}),
								  button("", {})}}});
	CHECK(first.value("id").toString() == "id1");
	CHECK(first.value("buttons").toArray().at(1).toObject().value("id").toString() == "id2"); // a missing id is filled in
	// another deck reusing b1 gets a new id for it: button ids are unique across decks
	const auto second = decks.saveDeck({{"name", "Other"}, {"buttons", QJsonArray{button("b1", {})}}});
	CHECK(second.value("buttons").toArray().at(0).toObject().value("id").toString() == "id4");
	// saving a deck again keeps its own button ids
	const auto again = decks.saveDeck(first);
	CHECK(again.value("buttons").toArray().at(0).toObject().value("id").toString() == "b1" && decks.decks().size() == 2);
	CHECK(changes == 3 && store.saves == 3);
	const auto saved = store.saved.value("decks");
	CHECK(saved.value("version").toInt() == 1 && saved.value("sound_monitoring").toBool() && saved.value("decks").toArray().size() == 2);

	const auto triggers = decks.triggers();
	CHECK(triggers.size() == 1 && triggers.at(0).toObject().value("button").toString() == "b1" &&
	      triggers.at(0).toObject().value("deck").toString() == "id1");

	decks.setHotkeyBindings({{"b1", QJsonArray{QJsonObject{{"key", "OBS_KEY_F1"}}}}});
	CHECK(decks.decks().at(0).toObject().value("buttons").toArray().at(0).toObject().value("hotkey").toArray().size() == 1);
	CHECK(store.saves == 3); // bindings alone do not save
	CHECK(decks.deleteDeck(second.value("id").toString()) && !decks.deleteDeck("nope") && decks.decks().size() == 1);

	MemoryStore old; // a decks.json from before, with sounds kept off the monitor
	old.saved.insert("decks", {{"version", 1}, {"sound_monitoring", false}, {"decks", QJsonArray{first}}});
	model::DeckModel reloaded(old);
	reloaded.load();
	CHECK(reloaded.decks().size() == 1 && !reloaded.soundMonitoring());
}

static void test_store_on_disk()
{
	QTemporaryDir dir;
	QStringList log;
	obs::ObsDataStore store(dir.path() + "/config", [&log](const QString &line) { log.append(line); });
	CHECK(store.load("decks").isEmpty()); // nothing saved yet, and no complaint
	CHECK(store.save("decks", {{"version", 1}}) && store.save("decks", {{"version", 2}}));
	CHECK(store.load("decks").value("version").toInt() == 2);
	QFile backup(dir.path() + "/config/decks.json.bak");
	CHECK(backup.open(QIODevice::ReadOnly) && backup.readAll().contains("1"));
	QFile broken(dir.path() + "/config/broken.json");
	CHECK(broken.open(QIODevice::WriteOnly) && broken.write("{nope") > 0);
	broken.close();
	CHECK(store.load("broken").isEmpty() && log.size() == 1 && QFile::exists(dir.path() + "/config/broken.json"));
}

static void test_deck_controller()
{
	MemoryStore store;
	store.saved.insert("decks", {{"version", 1}, {"sound_monitoring", false}, {"decks", QJsonArray{QJsonObject{
		{"id", "d1"}, {"name", "Stream"},
		{"buttons", QJsonArray{
			button("go", QJsonArray{QJsonObject{{"type", "scene"}, {"scene", "BRB"}, {"at", 0}},
						QJsonObject{{"type", "chat"}, {"text", "Be right back, /$user$/!"}, {"at", 0}},
						QJsonObject{{"type", "sound"}, {"file", "/x.mp3"}, {"at", 100}}}),
			button("note", QJsonArray{QJsonObject{{"type", "notify"}, {"text", "hi"}}})}}}}}});
	model::DeckModel decks(store);
	FakeSteps steps;
	FakeHotkeys hotkeys;
	QList<QPair<QString, QJsonObject>> events;
	controller::DeckController deck(decks, steps, &hotkeys,
					[&events](const QString &name, const QJsonObject &data) { events.append({name, data}); });
	qint64 now = 0;
	deck.setClock([&now] { return now; });
	deck.start();
	CHECK(!steps.monitoring && hotkeys.binds == 1 && (hotkeys.buttons == QStringList{"go", "note"}));

	CHECK(deck.press("go", {{"user", "Ana"}}));
	CHECK((steps.ran == QStringList{"scene:BRB"}));           // steps at 0 run at once; the sound waits
	CHECK(events.size() >= 2 && events.at(0).first == "button_state" && events.at(0).second.value("running").toBool());
	CHECK(events.at(1).first == "say" && events.at(1).second.value("text").toString() == "Be right back, Ana!");
	CHECK((deck.running() == QStringList{"go"}));
	now = 150;
	deck.tick();
	CHECK((steps.ran == QStringList{"scene:BRB", "sound:/x.mp3"}));
	CHECK(deck.running().isEmpty() && events.last().first == "button_state" && !events.last().second.value("running").toBool());

	hotkeys.key("note", true); // an OBS hotkey
	CHECK(events.at(events.size() - 2).first == "deck_notification");
	CHECK(!deck.press("nope"));

	// the bridge's deck actions, through the dispatcher's handler interface
	using controller::Command;
	CHECK(deck.handle(Command{"deck.list", {}, controller::CommandSource::Bridge, {}}).result.value("decks").toArray().size() == 1);
	CHECK(deck.handle(Command{"deck.press", {{"button", "nope"}}, controller::CommandSource::Bridge, {}}).code == "NOT_FOUND");
	const auto fired = deck.handle(Command{"deck.fire", {{"button", "go"}, {"vars", QJsonObject{{"user", "Bo"}}}},
					       controller::CommandSource::Bridge, {}});
	CHECK(fired.ok && events.at(events.size() - 1).second.value("text").toString() == "Be right back, Bo!");
	CHECK(deck.handle(Command{"deck.stop", {}, controller::CommandSource::Bridge, {}}).code == "BAD_REQUEST");
	CHECK(deck.handle(Command{"deck.stop", {{"button", "all"}}, controller::CommandSource::Bridge, {}}).ok);
	CHECK(deck.running().isEmpty() && steps.stopped.contains("*"));

	// an edit saves, keeps OBS's hotkey bindings and binds the hotkeys again
	hotkeys.bound.insert("go", QJsonArray{QJsonObject{{"key", "OBS_KEY_F2"}}});
	const int savesBefore = store.saves;
	const auto saved = deck.handle(Command{"deck.save", {{"deck", QJsonObject{{"name", "New"}, {"buttons", QJsonArray{button("x", {})}}}}},
					       controller::CommandSource::Bridge, {}});
	CHECK(saved.ok && saved.result.value("deck").toObject().value("name").toString() == "New");
	CHECK(hotkeys.binds == 2 && hotkeys.buttons.contains("x") && store.saves == savesBefore + 2);
	CHECK(store.saved.value("decks").value("decks").toArray().at(0).toObject().value("buttons").toArray().at(0)
		      .toObject().value("hotkey").toArray().size() == 1);
	CHECK(events.last().first == "decks_changed");
	CHECK(deck.handle(Command{"deck.delete", {{"id", "zzz"}}, controller::CommandSource::Bridge, {}}).code == "NOT_FOUND");
	CHECK(deck.handle(Command{"deck.triggers", {}, controller::CommandSource::Bridge, {}}).ok);
}

// ---- an output's life cycle ------------------------------------------------------------------------
static void test_output_lifecycle()
{
	using state::OutputStateId;
	state::OutputLifecycle out;
	QStringList seen;
	QObject::connect(&out, &state::OutputLifecycle::changed,
			 [&seen](OutputStateId now, OutputStateId before, const QString &error) {
				 seen.append(state::outputStateName(before) + ">" + state::outputStateName(now) +
					     (error.isEmpty() ? QString() : "(" + error + ")"));
			 });
	CHECK(out.state() == OutputStateId::IDLE && !out.running());
	out.reconnected(); // ignored: not a transition from IDLE
	out.stopped("noise");
	CHECK(seen.isEmpty());
	out.startRequested();
	out.started();
	out.started(); // libobs repeats itself: nothing new
	out.reconnecting();
	out.reconnected();
	out.stopRequested();
	out.stopped();
	CHECK((seen == QStringList{"IDLE>STARTING", "STARTING>ACTIVE", "ACTIVE>RECONNECTING", "RECONNECTING>ACTIVE",
				   "ACTIVE>STOPPING", "STOPPING>IDLE"}));
	seen.clear();
	out.startRequested();
	out.stopped("Could not connect"); // the start failed
	CHECK(out.state() == OutputStateId::FAILED && out.lastError() == "Could not connect");
	out.startRequested(); // try again: the old error is gone
	CHECK(out.lastError().isEmpty());
	out.started();
	out.reconnecting();
	out.stopped("Disconnected"); // dropped while reconnecting
	out.started(); // OBS started it some other way
	CHECK((seen == QStringList{"IDLE>STARTING", "STARTING>FAILED(Could not connect)", "FAILED>STARTING",
				   "STARTING>ACTIVE", "ACTIVE>RECONNECTING", "RECONNECTING>FAILED(Disconnected)",
				   "FAILED>ACTIVE"}));
	CHECK(state::outputStateName(OutputStateId::STOPPING) == "STOPPING");
}

// ---- starting and stopping outputs together --------------------------------------------------------------
class FakeOutputs final : public obs::Outputs {
public:
	QList<obs::OutputEntry> entries;
	QSet<QString> live;
	QSet<QString> refuse;                 // these cannot even try to start
	QStringList started, stopped;
	QHash<QString, std::function<void()>> pendingStarts; // onStarted callbacks not called yet

	QList<obs::OutputEntry> list() const override { return entries; }
	bool start(const QString &id, std::function<void()> onStarted) override
	{
		if (refuse.contains(id)) {
			return false;
		}
		started.append(id);
		pendingStarts.insert(id, onStarted);
		return true;
	}
	void stop(const QString &id) override
	{
		stopped.append(id);
		live.remove(id);
	}
	bool active(const QString &id) const override { return live.contains(id); }
	void goesLive(const QString &id)
	{
		live.insert(id);
		pendingStarts.take(id)();
	}
};

static void test_outputs_controller()
{
	FakeOutputs o;
	o.entries = {{"main:stream", "", true, true, false},     {"main:record", "", true, false, true},
		     {"main:virtual_cam", "", true, false, false}, {"Twitch", "Main", false, true, false},
		     {"TikTok", "Phones", false, true, false},     {"YouTube Shorts", "Phones", false, true, false},
		     {"Local copy", "", false, false, true}};
	QStringList log;
	controller::OutputsController c(o, [&log](const QString &line) { log.append(line); });

	CHECK((c.groups() == QStringList{"Main", "Phones"}));
	c.startGroup("Phones"); // one at a time: the second waits for the first
	CHECK((o.started == QStringList{"TikTok"}) && c.waiting() == 1);
	o.goesLive("TikTok");
	settle();
	CHECK((o.started == QStringList{"TikTok", "YouTube Shorts"}) && c.waiting() == 0);
	o.goesLive("YouTube Shorts");
	settle();
	CHECK(c.groupStatus("Phones") == std::make_pair(2, 2) && c.groupStatus("Main") == std::make_pair(0, 1));

	c.stopGroup("Phones");
	CHECK((o.stopped == QStringList{"TikTok", "YouTube Shorts"}) && c.groupStatus("Phones").first == 0);

	o.started.clear();
	o.refuse = {"main:stream"}; // skipped and tried again after the others
	c.startAll(controller::OutputsController::Which::Streams);
	CHECK((o.started == QStringList{"Twitch"})); // the virtual camera and recordings are not streams
	o.goesLive("Twitch");
	settle();
	o.goesLive("TikTok");
	settle();
	o.goesLive("YouTube Shorts");
	settle();
	CHECK((o.started == QStringList{"Twitch", "TikTok", "YouTube Shorts"}) && c.waiting() == 0);
	CHECK(log.last().contains("could not be started")); // a whole round with only the refused one left

	o.started.clear();
	o.refuse.clear();
	c.startAll(controller::OutputsController::Which::Recordings);
	CHECK((o.started == QStringList{"main:record"}) && c.waiting() == 1);
	c.stopAll(controller::OutputsController::Which::Recordings); // a stop drops what was still waiting
	CHECK(c.waiting() == 0);
	o.goesLive("main:record");
	settle();
	CHECK((o.started == QStringList{"main:record"})); // nothing more starts after the stop

	o.stopped.clear();
	c.stopAll(); // only what is live is stopped
	CHECK((o.stopped == QStringList{"main:record", "Twitch", "TikTok", "YouTube Shorts"}));
}

// ---- the themes as files ------------------------------------------------------------------------------
static void write(const QString &file, const QByteArray &content)
{
	QDir().mkpath(QFileInfo(file).absolutePath());
	QFile f(file);
	if (f.open(QIODevice::WriteOnly)) {
		f.write(content);
	}
}

static QByteArray read(const QString &file)
{
	QFile f(file);
	return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray("(missing)");
}

static void test_theme_files()
{
	namespace fs = std::filesystem;
	QTemporaryDir dir;
	const QString from = dir.path() + "/plugin/themes", to = dir.path() + "/obs-studio/themes";
	write(from + "/Fiji_Sun.obt", "sun");
	write(from + "/Fiji_Moon.obt", "moon");
	write(from + "/FijiSun/check.svg", "<svg/>");
	write(from + "/notes.txt", "not a theme");
	write(to + "/Fiji_Sun.ovt", "the 0.4.0 style"); // retired: removed
	write(to + "/Someone_Else.obt", "theirs");      // not ours: untouched
	int added = -1;
	const fs::path src = from.toStdString(), dst = to.toStdString();
	CHECK(model::themes::CopyThemes(src, dst, &added) == 3 && added == 2); // icons are written but are not themes
	CHECK(read(to + "/Fiji_Sun.obt") == "sun" && read(to + "/FijiSun/check.svg") == "<svg/>");
	CHECK(read(to + "/notes.txt") == "(missing)" && read(to + "/Fiji_Sun.ovt") == "(missing)");
	CHECK(read(to + "/Someone_Else.obt") == "theirs");
	CHECK(model::themes::CopyThemes(src, dst, &added) == 0 && added == 0); // nothing changed: nothing written
	write(from + "/Fiji_Moon.obt", "moon 2");
	CHECK(model::themes::CopyThemes(src, dst, &added) == 1 && added == 0 && read(to + "/Fiji_Moon.obt") == "moon 2");
	CHECK(model::themes::CopyThemes(fs::path(dir.path().toStdString()) / "nowhere", dst) == 0);
	CHECK(model::themes::UserThemesDir("/home/a/.config/obs-studio/plugin_config/fiji-suite/") ==
	      fs::path("/home/a/.config/obs-studio/themes"));
	CHECK(model::themes::UserThemesDir("/home/a/.config/obs-studio/plugin_config/fiji-suite") ==
	      fs::path("/home/a/.config/obs-studio/themes"));
}

// ---- the profile's settings ----------------------------------------------------------------------------
static void test_profile_model()
{
	// a brand new profile: OBS's own outputs shown, and a Vertical canvas with three outputs
	auto [fresh, created] = model::withFirstRunDefaults({}, false, "/videos");
	CHECK(created && fresh.value("main_stream_output_show").toBool() && fresh.value("main_virtual_cam_output_show").toBool());
	CHECK(fresh.value("canvas").toArray().at(0).toObject().value("name").toString() == "Vertical");
	const auto outputs = fresh.value("outputs").toArray();
	CHECK(outputs.size() == 3 && outputs.at(1).toObject().value("type").toString() == "backtrack");
	CHECK(outputs.at(1).toObject().value("path").toString() == "/videos" && outputs.at(1).toObject().value("max_time_sec").toInt() == 10);

	// an existing file is kept as it is: its outputs, its switches (a missing one is off), no new canvas
	QJsonObject saved{{"canvas", QJsonArray{QJsonObject{{"name", "Square"}}}}, {"outputs", QJsonArray{}},
			  {"main_stream_output_show", false}, {"main_record_output_show", true}};
	auto [kept, again] = model::withFirstRunDefaults(saved, true, "/videos");
	CHECK(!again && kept == saved);
	// a file from before canvases: the canvas and its outputs are added, the switches are not touched
	auto [old, added] = model::withFirstRunDefaults({{"main_record_output_show", true}}, true, "");
	CHECK(added && old.value("outputs").toArray().size() == 3 && !old.contains("main_stream_output_show"));
	CHECK(!old.value("outputs").toArray().at(1).toObject().contains("path"));

	model::ProfileModel profile;
	int changes = 0;
	QObject::connect(&profile, &model::ProfileModel::changed, [&changes] { changes++; });
	profile.load(kept);
	CHECK(changes == 1);
	const auto shown = profile.mainOutputs();
	CHECK(!shown.stream && shown.record && !shown.backtrack && !shown.virtualCam);
	CHECK(!profile.setMainOutputs(shown) && changes == 1); // nothing changed
	CHECK(profile.setMainOutputs({true, true, false, false}) && changes == 2);
	const auto owned = profile.owned();
	CHECK(owned.size() == 6 && owned.contains("canvas") && owned.value("main_stream_output_show").toBool() && !owned.value("main_backtrack_output_show").toBool());
	CHECK(owned.value("outputs").toArray().isEmpty()); // the file had an (empty) list, so it is written back
	CHECK(model::ProfileModel::ownedKeys().size() == 12);

	// hotkey bindings: kept as OBS saved them, only the ones that exist are written back
	CHECK(!profile.owned().contains("group_hotkeys"));
	const QJsonArray f1{QJsonObject{{"key", "OBS_KEY_F1"}}};
	CHECK(profile.setHotkey("start_all_outputs_hotkey", f1) && !profile.setHotkey("start_all_outputs_hotkey", f1));
	CHECK(!profile.setHotkey("someone_elses_hotkey", f1)); // not one of the profile's
	CHECK(profile.setHotkey("group_hotkeys", QJsonObject{{"Phones", QJsonObject{{"start", f1}, {"stop", QJsonArray{}}}}}));
	CHECK(profile.owned().value("start_all_outputs_hotkey").toArray() == f1 && profile.owned().contains("group_hotkeys"));
	model::ProfileModel reloaded;
	reloaded.load({{"multiview_over_preview_hotkey", f1}});
	CHECK(reloaded.hotkey("multiview_over_preview_hotkey").toArray() == f1 && reloaded.hotkey("group_hotkeys").isUndefined());

	// Fiji Suite's outputs: the list as the dialog saves it, and each output changing its own settings
	CHECK(!reloaded.owned().contains("outputs")); // a profile without the key stays without it
	model::ProfileModel own;
	int changed = 0;
	QObject::connect(&own, &model::ProfileModel::changed, [&changed] { changed++; });
	own.load(fresh);
	CHECK(own.outputs().size() == 3 && own.owned().value("outputs").toArray().size() == 3);
	auto vertical = own.outputs().at(0).toObject();
	vertical.insert("group", "Phones");
	CHECK(own.updateOutput("Vertical Stream", vertical) && own.outputs().at(0).toObject().value("group") == "Phones");
	CHECK(!own.updateOutput("Vertical Stream", vertical)); // the same again: nothing
	CHECK(!own.updateOutput("Nope", vertical));
	CHECK(own.setOutputs(QJsonArray{vertical}) && !own.setOutputs(QJsonArray{vertical}));
	CHECK(own.outputs().size() == 1 && changed == 3);

	// the canvases: found by uuid, or by name before OBS gave one
	model::ProfileModel canvases;
	canvases.load(fresh);
	CHECK(canvases.canvases().size() == 1 && canvases.owned().contains("canvas"));
	auto canvas = canvases.canvases().at(0).toObject();
	CHECK(canvas.value("uuid").toString().isEmpty());
	canvas.insert("uuid", "abc");
	CHECK(canvases.updateCanvas(canvas)); // matched by name
	canvas.insert("studio_mode", true);
	canvas.insert("name", "Renamed");
	CHECK(canvases.updateCanvas(canvas) && canvases.canvases().at(0).toObject().value("studio_mode").toBool()); // by uuid
	CHECK(!canvases.updateCanvas(canvas));
	CHECK(!canvases.updateCanvas(QJsonObject{{"uuid", "other"}, {"name", "Renamed"}})); // a different canvas
	CHECK(canvases.canvas("Renamed").value("uuid") == "abc" && canvases.canvas("Nope").isEmpty() && canvases.canvas("").isEmpty());
	CHECK(canvases.setCanvases(QJsonArray{}) && canvases.canvases().isEmpty() && canvases.owned().contains("canvas"));

	// view state: layouts and such, under the keys the docks build
	using PM = model::ProfileModel;
	CHECK(PM::isViewKey("stats_state") && PM::isViewKey("stats_state_Live") && PM::isViewKey("Main_canvas_split_order_Live"));
	CHECK(PM::isViewKey("video_encoder_description2") && PM::isViewKey("config_geometry") && PM::isViewKey("preview_locked"));
	CHECK(!PM::isViewKey("canvas") && !PM::isViewKey("video_encoder_descriptionX") && !PM::isViewKey("group_hotkeys"));
	model::ProfileModel view;
	view.load({{"stats_state_Live", "abc"}, {"somebody_else", 1}});
	CHECK(view.view("stats_state_Live") == "abc" && view.owned().value("stats_state_Live") == "abc");
	CHECK(!view.owned().contains("somebody_else"));
	CHECK(view.setView("preview_locked", true) && !view.setView("preview_locked", true) && !view.setView("not_view", 1));
	CHECK(view.setView("stats_state_Live", QJsonValue(QJsonValue::Undefined)) && !view.owned().contains("stats_state_Live"));

	// the whole document: unknown keys are kept, owned ones come from the model; clear forgets the profile
	CHECK(view.isLoaded() && view.document().value("somebody_else") == 1 && view.document().value("preview_locked") == true);
	CHECK(!view.document().contains("stats_state_Live"));
	view.clear();
	CHECK(!view.isLoaded() && view.document().isEmpty());
	CHECK(!model::ProfileModel().isLoaded());
}

// The capture controller: libobs's reports (from any thread) arrive queued, in order, as signals and a list.
static void test_capture_controller()
{
	CHECK(controller::captureText("game.exe", "Level 1") == "[game.exe]: Level 1");
	CHECK(controller::captureText("", "Notepad") == "Notepad" && controller::captureText("", "").isEmpty());

	FakeCaptureSources fake;
	controller::CaptureController captures(fake);
	captures.start();
	captures.start();
	CHECK(fake.listens == 1);
	QStringList seen;
	QObject::connect(&captures, &controller::CaptureController::added, [&seen](const controller::CaptureEntry &e) { seen << "added " + e.name; });
	QObject::connect(&captures, &controller::CaptureController::changed,
			 [&seen](const controller::CaptureEntry &e) { seen << "changed " + e.name + " " + e.capture; });
	QObject::connect(&captures, &controller::CaptureController::removed, [&seen](const QString &uuid) { seen << "removed " + uuid; });

	fake.listener.added("u1", "Game");
	CHECK(captures.entries().isEmpty()); // queued: nothing until the event loop runs
	fake.listener.hooked("u1", "game.exe", "Level 1");
	fake.listener.renamed("u1", "Game 2");
	fake.listener.added("u1", "Game"); // twice: once
	fake.listener.renamed("nope", "x"); // unknown: ignored
	QCoreApplication::processEvents();
	CHECK(captures.entries().size() == 1 && captures.entries().at(0).name == "Game 2");
	CHECK(captures.entries().at(0).capture == "[game.exe]: Level 1");
	fake.listener.hooked("u1", "", "");
	fake.listener.removed("u1");
	QCoreApplication::processEvents();
	CHECK(captures.entries().isEmpty());
	CHECK(seen == QStringList({"added Game", "changed Game [game.exe]: Level 1", "changed Game 2 [game.exe]: Level 1",
				   "changed Game 2 ", "removed u1"}));

	fake.windowsOf["u2"] = {{"Notepad", QString("notepad:1")}};
	CHECK(captures.windows("u2").size() == 1);
	captures.pickWindow("u2", QString("notepad:1"));
	CHECK(fake.picks.size() == 1 && fake.picks.at(0).second.toString() == "notepad:1");
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
	test_store_on_disk();
	test_deck_controller();
	test_output_lifecycle();
	test_outputs_controller();
	test_theme_files();
	test_profile_model();
	test_capture_controller();
	if (failures) {
		std::printf("%d check(s) failed\n", failures);
		return 1;
	}
	std::printf("core: all checks passed\n");
	return 0;
}
