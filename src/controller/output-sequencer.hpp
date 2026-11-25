#pragma once

// CONTROLLER layer: starts outputs one after another (model::StartSequence) through obs::StartableOutputs. Lives on
// the UI thread. Starting a new set replaces whatever was still waiting; stop() drops the rest.

#include "../model/output-sequence.hpp"

#include <QObject>

namespace fiji::obs {
class StartableOutputs;
}

namespace fiji::controller {

class OutputSequencer : public QObject {
	Q_OBJECT

public:
	explicit OutputSequencer(obs::StartableOutputs &outputs, QObject *parent = nullptr);

	void start(const QStringList &ids);
	void stop(); // forget what still waits (a "stop" was asked)
	QStringList waiting() const { return sequence_.waiting(); }

signals:
	void gaveUp(const QStringList &ids); // these could not be started in a whole round

private:
	void next();

	obs::StartableOutputs &outputs_;
	model::StartSequence sequence_;
	int generation_ = 0; // a new start() makes the callbacks of the old one do nothing
};

} // namespace fiji::controller
