#include "live-scenes-controller.hpp"

#include <QMetaObject>
#include <QPointer>

namespace fiji::controller {

LiveScenesController::LiveScenesController(model::LiveScenesModel &model, obs::LiveScenesObs &obs, QObject *parent)
	: QObject(parent),
	  model_(model),
	  obs_(obs)
{
}

void LiveScenesController::start()
{
	if (started_) {
		return;
	}
	started_ = true;
	QPointer<LiveScenesController> self(this);
	obs_.listen({
		[self](const QString &previous, const QString &next) {
			if (!self) {
				return;
			}
			QMetaObject::invokeMethod(
				self.data(),
				[self, previous, next] {
					if (self)
						self->model_.renamed(previous, next);
				},
				Qt::QueuedConnection);
		},
		[this] { return model_.toJson(); },
		[this](const QJsonArray &scenes) {
			model_.load(scenes);
			programChanged();
		},
	});
}

bool LiveScenesController::addScene(const QString &name)
{
	const QString uuid = obs_.uuidOf(name);
	return !uuid.isEmpty() && model_.add(name, uuid);
}

void LiveScenesController::pick(int row)
{
	const auto scenes = model_.scenes();
	if (row < 0 || row >= scenes.size()) {
		return;
	}
	obs_.showScene(scenes.at(row).uuid, scenes.at(row).name);
}

void LiveScenesController::programChanged()
{
	const auto program = obs_.program();
	model_.setProgram(program.name, program.uuid);
}

} // namespace fiji::controller
