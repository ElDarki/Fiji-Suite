#pragma once

// MODEL layer: starting several outputs one after another. Starting many at once overloads the encoders and the
// network, so each output starts only once the previous one is live. An output that cannot start yet (its encoder is
// busy, its canvas is not ready) is passed over and tried again later in the round; when a whole round passes without
// any of the remaining outputs starting, the rest are given up. Qt Core only; controller::OutputSequencer drives it.

#include <QString>
#include <QStringList>

namespace fiji::model {

class StartSequence {
public:
	void reset(const QStringList &ids); // what to start, in order; replaces what was waiting
	void clear();
	bool empty() const { return waiting_.isEmpty(); }
	QStringList waiting() const { return waiting_; }

	// The output to try now (empty when nothing waits).
	QString current() const;
	// It accepted: it leaves the sequence; the next one is tried once it is live.
	void accepted();
	// It refused: the next one is tried now. False when the round is over (every waiting output refused since the
	// last one that accepted): the rest are given up and the sequence is empty.
	bool refused();

private:
	QStringList waiting_;
	int at_ = 0;
	int refusedInRow_ = 0;
};

// The groups the outputs belong to, in the order they first appear (outputs: their group, "" for none).
QStringList groupsOf(const QStringList &outputGroups);

} // namespace fiji::model
