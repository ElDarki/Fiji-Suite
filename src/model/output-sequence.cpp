#include "output-sequence.hpp"

namespace fiji::model {

void StartSequence::reset(const QStringList &ids)
{
	waiting_ = ids;
	at_ = 0;
	refusedInRow_ = 0;
}

void StartSequence::clear()
{
	reset({});
}

QString StartSequence::current() const
{
	return waiting_.isEmpty() ? QString() : waiting_.at(at_ % waiting_.size());
}

void StartSequence::accepted()
{
	if (waiting_.isEmpty()) {
		return;
	}
	waiting_.removeAt(at_ % waiting_.size());
	if (at_ >= waiting_.size()) {
		at_ = 0;
	}
	refusedInRow_ = 0;
}

bool StartSequence::refused()
{
	if (waiting_.isEmpty()) {
		return false;
	}
	refusedInRow_++;
	if (refusedInRow_ >= waiting_.size()) {
		clear();
		return false;
	}
	at_ = (at_ + 1) % waiting_.size();
	return true;
}

QStringList groupsOf(const QStringList &outputGroups)
{
	QStringList groups;
	for (const auto &group : outputGroups) {
		if (!group.isEmpty() && !groups.contains(group)) {
			groups.append(group);
		}
	}
	return groups;
}

} // namespace fiji::model
