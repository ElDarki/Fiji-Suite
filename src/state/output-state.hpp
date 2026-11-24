#pragma once

// STATE layer: the life cycle of one output (a stream, a recording, the replay buffer, the virtual camera, an FFmpeg
// output), as an explicit State pattern. Qt Core only. The same six states as BRIDGE_CONTRACT.md's output states.
//
//   IDLE / FAILED --start requested--> STARTING --started--> ACTIVE <--reconnected-- RECONNECTING
//                                         |                    |  --reconnecting-->      |
//                                         |                    +--stop requested--> STOPPING
//   any running state --stopped (error)--> FAILED, --stopped (no error)--> IDLE; STOPPING --stopped--> IDLE
//   IDLE / FAILED --started--> ACTIVE (OBS started it some other way)
//
// The output's code feeds the inputs (libobs signals arrive on other threads: post them to this object's thread).

#include <QObject>
#include <QString>
#include <memory>

namespace fiji::state {

enum class OutputStateId { IDLE, STARTING, ACTIVE, RECONNECTING, STOPPING, FAILED };

// IDLE -> "IDLE", ... (the contract's words).
QString outputStateName(OutputStateId state);

class OutputLifecycle;

class OutputState {
public:
	virtual ~OutputState() = default;
	virtual OutputStateId id() const = 0;
	bool running() const { return id() != OutputStateId::IDLE && id() != OutputStateId::FAILED; }

	// Inputs. A state ignores the ones that are not transitions from it.
	virtual void onStartRequested(OutputLifecycle &) {}
	virtual void onStarted(OutputLifecycle &);
	virtual void onReconnecting(OutputLifecycle &) {}
	virtual void onReconnected(OutputLifecycle &) {}
	virtual void onStopRequested(OutputLifecycle &) {}
	virtual void onStopped(OutputLifecycle &, const QString & /*error*/);
};

class OutputIdle final : public OutputState {
public:
	OutputStateId id() const override { return OutputStateId::IDLE; }
	void onStartRequested(OutputLifecycle &output) override;
	void onStopped(OutputLifecycle &, const QString &) override {}
};

class OutputFailed final : public OutputState {
public:
	OutputStateId id() const override { return OutputStateId::FAILED; }
	void onStartRequested(OutputLifecycle &output) override;
	void onStopped(OutputLifecycle &, const QString &) override {}
};

class OutputStarting final : public OutputState {
public:
	OutputStateId id() const override { return OutputStateId::STARTING; }
	void onStopRequested(OutputLifecycle &output) override;
};

class OutputActive final : public OutputState {
public:
	OutputStateId id() const override { return OutputStateId::ACTIVE; }
	void onStarted(OutputLifecycle &) override {}
	void onReconnecting(OutputLifecycle &output) override;
	void onStopRequested(OutputLifecycle &output) override;
};

class OutputReconnecting final : public OutputState {
public:
	OutputStateId id() const override { return OutputStateId::RECONNECTING; }
	void onStarted(OutputLifecycle &) override {}
	void onReconnected(OutputLifecycle &output) override;
	void onStopRequested(OutputLifecycle &output) override;
};

class OutputStopping final : public OutputState {
public:
	OutputStateId id() const override { return OutputStateId::STOPPING; }
	void onStarted(OutputLifecycle &) override {}
	void onStopped(OutputLifecycle &output, const QString &error) override;
};

class OutputLifecycle : public QObject {
	Q_OBJECT

public:
	explicit OutputLifecycle(QObject *parent = nullptr);
	~OutputLifecycle() override;

	OutputStateId state() const;
	bool running() const { return current_->running(); }
	QString lastError() const { return error_; }

	void startRequested();
	void started();
	void reconnecting();
	void reconnected();
	void stopRequested();
	void stopped(const QString &error = QString());

	// Used by the states only.
	void transitionTo(std::unique_ptr<OutputState> next, const QString &error = QString());

signals:
	// Every change, with the state it came from; error is set when it ended in FAILED.
	void changed(fiji::state::OutputStateId now, fiji::state::OutputStateId before, const QString &error);

private:
	std::unique_ptr<OutputState> current_;
	QString error_;
};

} // namespace fiji::state
