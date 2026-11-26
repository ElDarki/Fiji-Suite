#include "output-sequencer.hpp"

#include "../obs/startable-port.hpp"

#include <QMetaObject>
#include <QPointer>

namespace fiji::controller {

OutputSequencer::OutputSequencer(obs::StartableOutputs &outputs, QObject *parent) : QObject(parent), outputs_(outputs) {}

void OutputSequencer::start(const QStringList &ids)
{
	generation_++;
	sequence_.reset(ids);
	next();
}

void OutputSequencer::stop()
{
	generation_++;
	sequence_.clear();
}

void OutputSequencer::next()
{
	while (!sequence_.empty()) {
		const QString id = sequence_.current();
		const int generation = generation_;
		QPointer<OutputSequencer> self(this);
		const bool accepted = outputs_.startOutput(id, [self, generation] {
			if (!self) {
				return;
			}
			// queued, so an output that is live at once does not start the next one inside its own start call
			QMetaObject::invokeMethod(
				self.data(),
				[self, generation] {
					if (self && self->generation_ == generation) {
						self->next();
					}
				},
				Qt::QueuedConnection);
		});
		if (accepted) {
			sequence_.accepted();
			return; // the next one starts once this one is live
		}
		const QStringList before = sequence_.waiting();
		if (!sequence_.refused()) {
			emit gaveUp(before);
			return;
		}
	}
}

} // namespace fiji::controller
