#pragma once

#include <functional>
#include <list>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariant>

// The part of the deck engine that decides what runs when. It knows nothing about OBS: every
// step that touches the outside world is handed to a Host, so this file can be unit tested.
//
// A button is a JSON object {"id", "steps": [...], "release": [...], "queueable", "overlappable"}.
// A step is {"type": "...", "at": <ms after the run started>, ...}. Steps run in list order, each
// once its time has come. The engine itself handles:
//   wait        {ms}                              pushes every later step back
//   wait_until  {variable, op, value, timeout}    holds the run until the comparison is true
//   set         {variable, value}                 "global.name" sets a variable shared by all buttons
//   random      {variable, min, max}              a whole number, both ends included
//   math        {variable, left, op, right}       op is + - * /
//   if          {left, op, right} ... else ... end
//   stop        {}                                ends this run
//   press       {button}    stop_button {button}
// Any other type goes to Host::execute. Before a step is used, /$name$/ in its text values is
// replaced by the variable's value.
namespace decks {

using Variables = QHash<QString, QVariant>;

// Replaces every /$name$/ in text. "global.name" reads a global; a plain name reads the run's
// own variables first, then the globals. Unknown names become empty.
QString expand(const QString &text, const Variables &local, const Variables &global);
QJsonObject expand(const QJsonObject &step, const Variables &local, const Variables &global);

// ops: == != < > <= >= contains starts_with. Both sides compare as numbers when they are numbers.
bool compare(const QVariant &left, const QString &op, const QVariant &right);

class Host {
public:
	virtual ~Host() = default;
	// Carries out one step that is not the engine's own. Variables are already filled in.
	virtual void execute(const QJsonObject &step) = 0;
	// A button started running, or its last run ended.
	virtual void buttonStateChanged(const QString &button, bool running)
	{
		Q_UNUSED(button);
		Q_UNUSED(running);
	}
};

class Engine {
public:
	explicit Engine(Host *host);

	// Replaces the known buttons with those of the given decks: [{"buttons": [...]}, ...].
	// Runs in progress finish with the steps they started with.
	void setDecks(const QJsonArray &decks);

	// Starts a button. trigger holds what set it off (user, message, ...) and becomes the run's
	// first variables. False when there is no such button, or it is busy and takes no more presses.
	bool press(const QString &button, const Variables &trigger, qint64 now);
	// Runs the button's release list, if it has one.
	bool release(const QString &button, qint64 now);
	void stop(const QString &button);
	void stopAll();

	// Runs everything that is due. Call it regularly with a steadily increasing clock (ms).
	void tick(qint64 now);

	bool isRunning(const QString &button) const;
	QStringList running() const;
	bool idle() const { return runs.empty(); }
	Variables &globals() { return globalVariables; }

	// For tests: replaces the random number source.
	void setRandom(std::function<int(int, int)> source) { randomSource = std::move(source); }

private:
	struct Run {
		QString button;
		QJsonArray steps;
		int next = 0;
		qint64 start = 0;
		qint64 pushedBack = 0;      // total of the waits so far
		qint64 holdingSince = -1;   // when a wait_until started holding, or -1
		Variables local;
		bool finished = false;
	};
	struct Queued {
		QString button;
		Variables trigger;
	};

	Host *host;
	QHash<QString, QJsonObject> buttons;
	std::list<Run> runs;
	std::list<Queued> queue;
	Variables globalVariables;
	std::function<int(int, int)> randomSource;

	void start(const QString &button, const QJsonArray &steps, const Variables &trigger, qint64 now);
	// Runs the due steps of one run. Returns after a hold, a step that is not due yet, or the end.
	void advance(Run &run, qint64 now);
	void setVariable(Run &run, const QString &name, const QVariant &value);
	// Index of the step after the else / end that closes the block opened just before `from`.
	static int skipBlock(const QJsonArray &steps, int from, bool stopAtElse);
};

} // namespace decks
