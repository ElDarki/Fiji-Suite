#pragma once

// CONTROLLER layer: starting and stopping outputs together. Starting several goes one at a time: each output gets
// going before the next one starts, which keeps OBS from opening every connection at once. An output that cannot even
// try is skipped and tried again on the next round; when a whole round fails, the rest are dropped. Lives on the UI
// thread; outputs reach it only through the obs::Outputs port.

#include "../obs/output-ports.hpp"

#include <QObject>
#include <QStringList>
#include <deque>
#include <functional>

namespace fiji::controller {

class OutputsController final : public QObject {
	Q_OBJECT

public:
	enum class Which { All, Streams, Recordings };

	explicit OutputsController(obs::Outputs &outputs, std::function<void(const QString &)> log = {},
				   QObject *parent = nullptr);

	void startAll(Which which = Which::All);
	void stopAll(Which which = Which::All);
	void startGroup(const QString &group);
	void stopGroup(const QString &group);

	QStringList groups() const;
	// (live, members) of a group.
	std::pair<int, int> groupStatus(const QString &group) const;
	// Outputs still waiting their turn to start.
	int waiting() const { return int(queue_.size()); }

private:
	static bool picked(const obs::OutputEntry &output, Which which);
	void startQueue(const QStringList &ids);
	void startNext();

	obs::Outputs &outputs_;
	std::function<void(const QString &)> log_;
	std::deque<QString> queue_;
	size_t at_ = 0;
};

} // namespace fiji::controller
