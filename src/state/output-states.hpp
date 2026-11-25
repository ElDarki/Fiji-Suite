#pragma once

// STATE layer: the life cycle of each output, in the bridge contract's words (BRIDGE_CONTRACT.md section 4):
//
//   state          start asked   started    reconnecting   reconnected   stop asked   stopped    failed
//   IDLE           STARTING      ACTIVE     -              -             -            -          FAILED
//   STARTING       -             ACTIVE     RECONNECTING   -             STOPPING     IDLE       FAILED
//   ACTIVE         -             -          RECONNECTING   -             STOPPING     IDLE       FAILED
//   RECONNECTING   -             ACTIVE     -              ACTIVE        STOPPING     IDLE       FAILED
//   STOPPING       -             -          -              -             -            IDLE       FAILED
//   FAILED         STARTING      ACTIVE     -              -             -            IDLE       -
//
// "started" and "reconnected" come from OBS (an output started outside the plugin goes straight to ACTIVE); "stopped"
// is a stop without an error, "failed" one with an error. Qt Core only.

#include <QHash>
#include <QObject>
#include <QString>

namespace fiji::state {

enum class OutputStateId { IDLE, STARTING, ACTIVE, RECONNECTING, STOPPING, FAILED };
enum class OutputInput { StartAsked, Started, Reconnecting, Reconnected, StopAsked, Stopped, Failed };

OutputStateId nextOutputState(OutputStateId from, OutputInput input);
QString outputStateName(OutputStateId state); // IDLE, STARTING, ... as in the contract

class OutputStates : public QObject {
	Q_OBJECT

public:
	explicit OutputStates(QObject *parent = nullptr);

	OutputStateId state(const QString &output) const;
	QString lastError(const QString &output) const;
	void input(const QString &output, OutputInput input, const QString &error = QString());

	// The plugin's own output events ("start_output", "stop_output" {last_error}, "output_reconnecting",
	// "output_reconnected"), as the output widgets emit them. Other names are ignored.
	void onPluginEvent(const QString &event, const QString &output, const QString &error);

signals:
	void stateChanged(const QString &output, fiji::state::OutputStateId state, const QString &error);

private:
	QHash<QString, OutputStateId> states_; // outputs that are not IDLE
	QHash<QString, QString> errors_;
};

} // namespace fiji::state
