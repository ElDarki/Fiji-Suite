#include "deck-engine.hpp"

#include <algorithm>
#include <QRegularExpression>
#include <random>

namespace decks {

static QVariant lookup(const QString &name, const Variables &local, const Variables &global)
{
	if (name.startsWith(QStringLiteral("global."))) {
		return global.value(name.mid(7));
	}
	const auto found = local.constFind(name);
	return found != local.constEnd() ? *found : global.value(name);
}

static QString as_text(const QVariant &value)
{
	// whole numbers print without a decimal point, as they were typed
	if (value.typeId() == QMetaType::Double) {
		const double d = value.toDouble();
		if (d == qint64(d)) {
			return QString::number(qint64(d));
		}
	}
	return value.toString();
}

QString expand(const QString &text, const Variables &local, const Variables &global)
{
	static const QRegularExpression pattern(QStringLiteral("/\\$([^$/]+)\\$/"));
	QString out;
	int last = 0;
	auto it = pattern.globalMatch(text);
	while (it.hasNext()) {
		const auto match = it.next();
		out += text.mid(last, match.capturedStart() - last);
		out += as_text(lookup(match.captured(1).trimmed(), local, global));
		last = match.capturedEnd();
	}
	return last ? out + text.mid(last) : text;
}

static QJsonValue expand_value(const QJsonValue &value, const Variables &local, const Variables &global)
{
	if (value.isString()) {
		return expand(value.toString(), local, global);
	}
	if (value.isObject()) {
		return expand(value.toObject(), local, global);
	}
	if (value.isArray()) {
		QJsonArray out;
		for (const auto &item : value.toArray()) {
			out.append(expand_value(item, local, global));
		}
		return out;
	}
	return value;
}

QJsonObject expand(const QJsonObject &step, const Variables &local, const Variables &global)
{
	QJsonObject out;
	for (auto it = step.constBegin(); it != step.constEnd(); ++it) {
		out.insert(it.key(), expand_value(it.value(), local, global));
	}
	return out;
}

bool compare(const QVariant &left, const QString &op, const QVariant &right)
{
	bool leftIsNumber = false;
	bool rightIsNumber = false;
	const double l = left.toString().toDouble(&leftIsNumber);
	const double r = right.toString().toDouble(&rightIsNumber);
	const bool numbers = leftIsNumber && rightIsNumber;
	const QString ls = left.toString();
	const QString rs = right.toString();
	if (op == QStringLiteral("==")) {
		return numbers ? l == r : ls == rs;
	} else if (op == QStringLiteral("!=")) {
		return numbers ? l != r : ls != rs;
	} else if (op == QStringLiteral("<")) {
		return numbers ? l < r : ls < rs;
	} else if (op == QStringLiteral(">")) {
		return numbers ? l > r : ls > rs;
	} else if (op == QStringLiteral("<=")) {
		return numbers ? l <= r : ls <= rs;
	} else if (op == QStringLiteral(">=")) {
		return numbers ? l >= r : ls >= rs;
	} else if (op == QStringLiteral("contains")) {
		return ls.contains(rs, Qt::CaseInsensitive);
	} else if (op == QStringLiteral("starts_with")) {
		return ls.startsWith(rs, Qt::CaseInsensitive);
	}
	return false;
}

// A step's value as a variant: JSON numbers stay numbers, everything else is text.
static QVariant value_of(const QJsonValue &value)
{
	return value.isDouble() ? QVariant(value.toDouble()) : QVariant(value.toString());
}

static double number_of(const QJsonValue &value)
{
	return value.isDouble() ? value.toDouble() : value.toString().toDouble();
}

Engine::Engine(Host *host_) : host(host_)
{
	randomSource = [](int low, int high) {
		static std::mt19937 generator{std::random_device{}()};
		return std::uniform_int_distribution<int>(low, high)(generator);
	};
}

void Engine::setDecks(const QJsonArray &decks)
{
	buttons.clear();
	for (const auto &deck : decks) {
		for (const auto &button : deck.toObject().value(QStringLiteral("buttons")).toArray()) {
			const auto object = button.toObject();
			const auto id = object.value(QStringLiteral("id")).toString();
			if (!id.isEmpty()) {
				buttons.insert(id, object);
			}
		}
	}
}

bool Engine::isRunning(const QString &button) const
{
	return std::any_of(runs.begin(), runs.end(), [&](const Run &run) { return run.button == button && !run.finished; });
}

QStringList Engine::running() const
{
	QStringList names;
	for (const auto &run : runs) {
		if (!run.finished && !names.contains(run.button)) {
			names.append(run.button);
		}
	}
	return names;
}

void Engine::start(const QString &button, const QJsonArray &steps, const Variables &trigger, qint64 now)
{
	const bool wasRunning = isRunning(button);
	Run run;
	run.button = button;
	run.steps = steps;
	run.start = now;
	run.local = trigger;
	runs.push_back(run);
	if (!wasRunning) {
		host->buttonStateChanged(button, true);
	}
}

bool Engine::press(const QString &button, const Variables &trigger, qint64 now)
{
	const auto found = buttons.constFind(button);
	if (found == buttons.constEnd()) {
		return false;
	}
	if (isRunning(button) && !found->value(QStringLiteral("overlappable")).toBool()) {
		if (!found->value(QStringLiteral("queueable")).toBool()) {
			return false;
		}
		queue.push_back({button, trigger});
		return true;
	}
	start(button, found->value(QStringLiteral("steps")).toArray(), trigger, now);
	return true;
}

bool Engine::release(const QString &button, qint64 now)
{
	const auto found = buttons.constFind(button);
	if (found == buttons.constEnd()) {
		return false;
	}
	const auto steps = found->value(QStringLiteral("release")).toArray();
	if (steps.isEmpty()) {
		return false;
	}
	start(button, steps, {}, now);
	return true;
}

void Engine::stop(const QString &button)
{
	for (auto &run : runs) {
		if (run.button == button) {
			run.finished = true;
		}
	}
	queue.remove_if([&](const Queued &q) { return q.button == button; });
}

void Engine::stopAll()
{
	for (auto &run : runs) {
		run.finished = true;
	}
	queue.clear();
}

void Engine::setVariable(Run &run, const QString &name, const QVariant &value)
{
	if (name.startsWith(QStringLiteral("global."))) {
		globalVariables.insert(name.mid(7), value);
	} else if (!name.isEmpty()) {
		run.local.insert(name, value);
	}
}

int Engine::skipBlock(const QJsonArray &steps, int from, bool stopAtElse)
{
	int depth = 0;
	for (int i = from; i < steps.size(); i++) {
		const auto type = steps.at(i).toObject().value(QStringLiteral("type")).toString();
		if (type == QStringLiteral("if")) {
			depth++;
		} else if (type == QStringLiteral("end")) {
			if (depth == 0) {
				return i + 1;
			}
			depth--;
		} else if (type == QStringLiteral("else") && depth == 0 && stopAtElse) {
			return i + 1;
		}
	}
	return int(steps.size());
}

void Engine::advance(Run &run, qint64 now)
{
	// a run that never waits could loop forever through press; this bounds one tick's work
	int budget = 10000;
	while (!run.finished && run.next < run.steps.size() && budget-- > 0) {
		const auto raw = run.steps.at(run.next).toObject();
		const auto type = raw.value(QStringLiteral("type")).toString();
		const qint64 due = run.start + run.pushedBack + qint64(raw.value(QStringLiteral("at")).toDouble());
		if (run.holdingSince < 0 && now < due) {
			return;
		}
		const auto step = expand(raw, run.local, globalVariables);

		if (type == QStringLiteral("wait_until")) {
			if (run.holdingSince < 0) {
				// from when the step was due, not from the tick that happened to notice it
				run.holdingSince = due;
			}
			const auto current = lookup(step.value(QStringLiteral("variable")).toString(), run.local, globalVariables);
			const qint64 timeout = qint64(number_of(step.value(QStringLiteral("timeout"))));
			const bool met = compare(current, step.value(QStringLiteral("op")).toString(QStringLiteral("==")),
						 value_of(step.value(QStringLiteral("value"))));
			if (!met && !(timeout > 0 && now - run.holdingSince >= timeout)) {
				return;
			}
			run.pushedBack += now - run.holdingSince;
			run.holdingSince = -1;
			run.next++;
			continue;
		}

		run.next++;
		if (type == QStringLiteral("wait")) {
			run.pushedBack += qint64(number_of(step.value(QStringLiteral("ms"))));
		} else if (type == QStringLiteral("set")) {
			setVariable(run, step.value(QStringLiteral("variable")).toString(), value_of(step.value(QStringLiteral("value"))));
		} else if (type == QStringLiteral("random")) {
			int low = int(number_of(step.value(QStringLiteral("min"))));
			int high = int(number_of(step.value(QStringLiteral("max"))));
			if (low > high) {
				std::swap(low, high);
			}
			setVariable(run, step.value(QStringLiteral("variable")).toString(), double(randomSource(low, high)));
		} else if (type == QStringLiteral("math")) {
			const double l = number_of(step.value(QStringLiteral("left")));
			const double r = number_of(step.value(QStringLiteral("right")));
			const auto op = step.value(QStringLiteral("op")).toString();
			const double result = op == QStringLiteral("+")   ? l + r
					      : op == QStringLiteral("-") ? l - r
					      : op == QStringLiteral("*") ? l * r
					      : (op == QStringLiteral("/") && r != 0) ? l / r
					      : 0.0;
			setVariable(run, step.value(QStringLiteral("variable")).toString(), result);
		} else if (type == QStringLiteral("if")) {
			const bool met = compare(value_of(step.value(QStringLiteral("left"))),
						 step.value(QStringLiteral("op")).toString(QStringLiteral("==")),
						 value_of(step.value(QStringLiteral("right"))));
			if (!met) {
				run.next = skipBlock(run.steps, run.next, true);
			}
		} else if (type == QStringLiteral("else")) {
			// reached by finishing the "if" branch: skip the "else" branch
			run.next = skipBlock(run.steps, run.next, false);
		} else if (type == QStringLiteral("end") || type == QStringLiteral("comment") ||
			   type == QStringLiteral("unsupported")) {
			// nothing to do
		} else if (type == QStringLiteral("stop")) {
			run.finished = true;
		} else if (type == QStringLiteral("press")) {
			press(step.value(QStringLiteral("button")).toString(), run.local, now);
		} else if (type == QStringLiteral("stop_button")) {
			// a release list usually stops its own button; that must not end the release list itself
			stop(step.value(QStringLiteral("button")).toString());
			run.finished = false;
		} else if (!step.value(QStringLiteral("disabled")).toBool()) {
			host->execute(step);
		}
	}
	if (run.next >= run.steps.size()) {
		run.finished = true;
	}
}

void Engine::tick(qint64 now)
{
	// runs started during this tick (by press) are appended and picked up by the same loop
	for (auto it = runs.begin(); it != runs.end(); ++it) {
		advance(*it, now);
	}
	QStringList ended;
	for (auto it = runs.begin(); it != runs.end();) {
		if (it->finished) {
			if (!ended.contains(it->button)) {
				ended.append(it->button);
			}
			it = runs.erase(it);
		} else {
			++it;
		}
	}
	for (const auto &button : ended) {
		if (isRunning(button)) {
			continue;
		}
		// a queued press takes over before the button is reported idle
		const auto queued = std::find_if(queue.begin(), queue.end(), [&](const Queued &q) { return q.button == button; });
		if (queued != queue.end()) {
			const auto trigger = queued->trigger;
			queue.erase(queued);
			const auto found = buttons.constFind(button);
			if (found != buttons.constEnd()) {
				Run run;
				run.button = button;
				run.steps = found->value(QStringLiteral("steps")).toArray();
				run.start = now;
				run.local = trigger;
				runs.push_back(run);
				continue;
			}
		}
		host->buttonStateChanged(button, false);
	}
}

} // namespace decks
