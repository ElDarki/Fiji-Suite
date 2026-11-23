// Tests for the MVC + State core with Qt Core only (no OBS): the link state machine, the command dispatcher, the
// bridge envelope and the bridge ACL driven through a fake ObsApi.
//   cmake --build build_x86_64 --target fiji-core-test && ctest --test-dir build_x86_64 -R core

#include "../src/bridge/bridge-acl.hpp"
#include "../src/bridge/catalogue.hpp"
#include "../src/bridge/envelope.hpp"
#include "../src/controller/command-dispatcher.hpp"
#include "../src/controller/obs-command-handler.hpp"
#include "../src/obs/obs-api.hpp"
#include "../src/state/link-state.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
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
	obs::ObsCall startOutputs(const QString &t) override
	{
		auto r = record("start " + t);
		if (r.ok) {
			r.data = {{"started", QJsonArray{"Stream", "TikTok"}}, {"secret", 1}};
		}
		return r;
	}
	obs::ObsCall stopOutputs(const QString &t) override { return record("stop " + t); }
	obs::ObsCall decks() override { return record("decks"); }
	obs::ObsCall pressButton(const QString &b, const QJsonObject &vars) override
	{
		return record("press " + b + " " + vars.value("user").toString());
	}
	obs::ObsCall releaseButton(const QString &b) override { return record("release " + b); }
	obs::ObsCall stopButton(const QString &b) override { return record("stopkey " + b); }
	obs::ObsCall saveDeck(const QJsonObject &) override { return record("save"); }
	obs::ObsCall deleteDeck(const QString &id) override { return record("delete " + id); }
	obs::ObsCall triggers() override { return record("triggers"); }
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
	CHECK(handler.handle({"deck.save", {{"deck", "x"}}, controller::CommandSource::Ui, {}}).code == "BAD_REQUEST");
	const auto snapshot = handler.handle({"state.snapshot", {}, controller::CommandSource::Ui, {}});
	CHECK(snapshot.ok && model::ObsSnapshot::fromJson(snapshot.result).outputs.size() == 2);
	CHECK(model::ObsSnapshot::fromJson(snapshot.result).canvases.at(1).studioMode);
	CHECK(obs.calls.isEmpty());
}

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	test_link_states();
	test_dispatcher();
	test_envelope();
	test_acl();
	test_handler_rejects_bad_payloads();
	if (failures) {
		std::printf("%d check(s) failed\n", failures);
		return 1;
	}
	std::printf("core: all checks passed\n");
	return 0;
}
