#include "live-scenes-controller.hpp"

#include "../model/live-scenes.hpp"
#include "../obs/scene-port.hpp"

namespace fiji::controller {

LiveScenesController::LiveScenesController(model::LiveScenes &scenes, obs::ScenePort &obs) : scenes_(scenes), obs_(obs) {}

QStringList LiveScenesController::addable() const
{
	return obs_.sceneNames();
}

bool LiveScenesController::add(const QString &name)
{
	if (name.isEmpty()) {
		return false;
	}
	const QString uuid = obs_.uuidOf(name);
	if (uuid.isEmpty()) {
		return false;
	}
	scenes_.add({name, uuid});
	return true;
}

bool LiveScenesController::remove(const QString &name)
{
	return scenes_.removeNamed(name);
}

bool LiveScenesController::removeAt(int index)
{
	return scenes_.removeAt(index);
}

int LiveScenesController::move(int index, int delta)
{
	return scenes_.move(index, delta);
}

bool LiveScenesController::show(int index)
{
	const auto entries = scenes_.entries();
	if (index < 0 || index >= entries.size()) {
		return false;
	}
	return obs_.showOnProgram({entries[index].name, entries[index].uuid});
}

void LiveScenesController::programChanged()
{
	const auto program = obs_.programScene();
	if (program.name.isEmpty() && program.uuid.isEmpty()) {
		scenes_.markLive(QString(), QString());
		return;
	}
	scenes_.markLive(program.name, program.uuid);
}

} // namespace fiji::controller
