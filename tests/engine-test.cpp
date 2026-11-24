// Tests for the deck engine core. Build target: decks-engine-test. Exit code 0 means every check passed.
#include "../src/state/deck-engine.hpp"

#include <cstdio>
#include <QJsonDocument>

using namespace decks;

static int failures = 0;

#define CHECK(condition)                                                              \
	do {                                                                          \
		if (!(condition)) {                                                   \
			std::printf("FAILED %s:%d  %s\n", __FILE__, __LINE__, #condition); \
			failures++;                                                   \
		}                                                                     \
	} while (0)

struct Recorder : Host {
	QStringList done;   // "type:label" of every step handed to the host
	QStringList states; // "button:on" / "button:off"
	void execute(const QJsonObject &step) override
	{
		done.append(step.value("type").toString() + ":" + step.value("label").toString());
	}
	void buttonStateChanged(const QString &button, bool running) override
	{
		states.append(button + (running ? ":on" : ":off"));
	}
};

static QJsonArray decks_of(const char *json)
{
	const auto doc = QJsonDocument::fromJson(QByteArray(json));
	return doc.array();
}

static void test_text_and_comparisons()
{
	Variables local{{"user", "Ana"}, {"n", 3.0}};
	Variables global{{"scene", "Game"}, {"user", "Global"}};
	CHECK(expand("Hi /$user$/, /$n$/ on /$global.scene$/ /$ scene $/ /$nope$/!", local, global) == "Hi Ana, 3 on Game Game !");
	CHECK(expand("no variables", local, global) == "no variables");
	CHECK(expand("/$global.user$/", local, global) == "Global");
	QJsonObject step{{"text", "/$user$/"}, {"n", 5}, {"inner", QJsonObject{{"t", "/$n$/"}}}, {"list", QJsonArray{"/$user$/", 1}}};
	const auto out = expand(step, local, global);
	CHECK(out.value("text").toString() == "Ana" && out.value("n").toInt() == 5);
	CHECK(out.value("inner").toObject().value("t").toString() == "3");
	CHECK(out.value("list").toArray().at(0).toString() == "Ana");

	CHECK(compare(10, "==", "10") && compare("10", ">", 9) && !compare("10", "<", "9")); // numbers, not text
	CHECK(compare("abc", "!=", "abd") && compare("abc", "<", "abd"));
	CHECK(compare("Hello World", "contains", "world") && compare("Hello", "starts_with", "he"));
	CHECK(compare(2, "<=", 2) && compare(2, ">=", 2) && !compare(1, "nonsense", 1));
}

static void test_timeline_and_waits()
{
	Recorder host;
	Engine engine(&host);
	engine.setDecks(decks_of(R"([{"buttons":[{"id":"b","steps":[
		{"type":"act","label":"first","at":0},
		{"type":"act","label":"second","at":100},
		{"type":"wait","ms":500,"at":100},
		{"type":"act","label":"third","at":150}
	]}]}])"));
	CHECK(!engine.press("missing", {}, 0));
	CHECK(engine.press("b", {}, 1000));
	CHECK(!engine.press("b", {}, 1000)); // busy, and neither queueable nor overlappable
	engine.tick(1000);
	CHECK(host.done == QStringList{"act:first"} && engine.isRunning("b"));
	engine.tick(1099);
	CHECK(host.done.size() == 1);
	engine.tick(1100);
	CHECK(host.done == (QStringList{"act:first", "act:second"}));
	engine.tick(1649); // third is at 150 + 500 of waiting
	CHECK(host.done.size() == 2);
	engine.tick(1650);
	CHECK(host.done.last() == "act:third" && !engine.isRunning("b") && engine.idle());
	CHECK(host.states == (QStringList{"b:on", "b:off"}));
}

static void test_variables_conditions_and_random()
{
	Recorder host;
	Engine engine(&host);
	engine.setRandom([](int low, int high) { return low + high; });
	engine.setDecks(decks_of(R"([{"buttons":[{"id":"b","steps":[
		{"type":"random","variable":"roll","min":1,"max":6},
		{"type":"set","variable":"global.last","value":"/$user$/"},
		{"type":"math","variable":"twice","left":"/$roll$/","op":"*","right":2},
		{"type":"if","left":"/$twice$/","op":"==","right":14},
			{"type":"act","label":"yes /$twice$/"},
			{"type":"if","left":"/$user$/","op":"==","right":"nobody"},
				{"type":"act","label":"inner yes"},
			{"type":"else"},
				{"type":"act","label":"inner no"},
			{"type":"end"},
		{"type":"else"},
			{"type":"act","label":"no"},
		{"type":"end"},
		{"type":"act","label":"skipped","disabled":true},
		{"type":"stop"},
		{"type":"act","label":"after stop"}
	]}]}])"));
	engine.press("b", {{"user", "Ana"}}, 0);
	engine.tick(0);
	CHECK(host.done == (QStringList{"act:yes 14", "act:inner no"}));
	CHECK(engine.globals().value("last").toString() == "Ana" && engine.idle());
}

static void test_wait_until_and_timeout()
{
	Recorder host;
	Engine engine(&host);
	engine.setDecks(decks_of(R"([{"buttons":[
		{"id":"b","steps":[
			{"type":"wait_until","variable":"global.ready","op":"==","value":1,"timeout":0},
			{"type":"act","label":"went","at":50}]},
		{"id":"t","steps":[
			{"type":"wait_until","variable":"global.never","op":"==","value":1,"timeout":300},
			{"type":"act","label":"gave up"}]}
	]}])"));
	engine.press("b", {}, 0);
	engine.press("t", {}, 0);
	engine.tick(200);
	CHECK(host.done.isEmpty());
	engine.tick(300);
	CHECK(host.done == QStringList{"act:gave up"});
	engine.globals().insert("ready", 1);
	engine.tick(400);
	CHECK(host.done.size() == 1); // held for 400 ms, so "went" (at 50) is due at 450
	engine.tick(450);
	CHECK(host.done.last() == "act:went" && engine.idle());
}

static void test_press_queue_overlap_release_and_stop()
{
	Recorder host;
	Engine engine(&host);
	engine.setDecks(decks_of(R"([{"buttons":[
		{"id":"chain","steps":[{"type":"press","button":"target"},{"type":"act","label":"chain"}]},
		{"id":"target","steps":[{"type":"act","label":"target for /$user$/"}]},
		{"id":"queued","queueable":true,"steps":[{"type":"act","label":"q /$n$/"},{"type":"act","label":"q end","at":100}]},
		{"id":"both","overlappable":true,"steps":[{"type":"act","label":"o","at":100}]},
		{"id":"held","steps":[{"type":"act","label":"down"},{"type":"act","label":"never","at":1000}],
		 "release":[{"type":"stop_button","button":"held"},{"type":"act","label":"up"}]}
	]}])"));
	engine.press("chain", {{"user", "Leo"}}, 0);
	engine.tick(0);
	CHECK(host.done == (QStringList{"act:chain", "act:target for Leo"})); // a pressed button sees the presser's variables

	host.done.clear();
	host.states.clear();
	CHECK(engine.press("queued", {{"n", 1}}, 0) && engine.press("queued", {{"n", 2}}, 0));
	engine.tick(0);
	engine.tick(100);
	CHECK(host.done == (QStringList{"act:q 1", "act:q end"}) && engine.isRunning("queued"));
	engine.tick(101);
	engine.tick(201);
	CHECK(host.done == (QStringList{"act:q 1", "act:q end", "act:q 2", "act:q end"}));
	CHECK(host.states == (QStringList{"queued:on", "queued:off"})); // one on, one off, across both runs

	host.done.clear();
	CHECK(engine.press("both", {}, 1000) && engine.press("both", {}, 1050));
	engine.tick(1100);
	engine.tick(1150);
	CHECK(host.done == (QStringList{"act:o", "act:o"}));

	host.done.clear();
	engine.press("held", {}, 2000);
	engine.tick(2000);
	CHECK(engine.release("held", 2100) && !engine.release("target", 2100));
	engine.tick(2100);
	engine.tick(3100);
	CHECK(host.done == (QStringList{"act:down", "act:up"}) && engine.idle());

	engine.press("both", {}, 4000);
	engine.stopAll();
	engine.tick(4200);
	CHECK(engine.idle() && host.done.size() == 2);
}

int main()
{
	test_text_and_comparisons();
	test_timeline_and_waits();
	test_variables_conditions_and_random();
	test_wait_until_and_timeout();
	test_press_queue_overlap_release_and_stop();
	std::printf(failures ? "%d check(s) failed\n" : "all engine checks passed\n", failures);
	return failures ? 1 : 0;
}
