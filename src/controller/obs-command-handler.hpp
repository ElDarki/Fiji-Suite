#pragma once

// CONTROLLER layer: runs the OBS and deck commands (from the bridge today; hotkeys and docks as they move over) on the
// dispatcher's thread, through obs::ObsApi only. Never throws: every failure is a CommandResult{ok = false}.
//
// Actions: scene.switch, transition.set, transition.run, output.start, output.stop, deck.list, deck.press,
// deck.fire, deck.release, deck.stop, deck.save, deck.delete, deck.file, look.get, deck.triggers, and the local
// state.snapshot (result: model::ObsSnapshot::toJson()).

#include "command-dispatcher.hpp"

namespace fiji::obs {
class ObsApi;
}

namespace fiji::controller {

class ObsCommandHandler final : public CommandHandler {
public:
	explicit ObsCommandHandler(obs::ObsApi &obs) : obs_(obs) {}

	QStringList actions() const override;
	CommandResult handle(const Command &command) override;

private:
	obs::ObsApi &obs_;
};

} // namespace fiji::controller
