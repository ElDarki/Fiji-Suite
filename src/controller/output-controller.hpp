#pragma once

// CONTROLLER layer: starting and stopping outputs, whoever asks (the bot today; docks and hotkeys as they move
// over). Works out what the target means (model::planOutputs), asks obs::OutputBackend, and marks the outputs it
// asked as STARTING or STOPPING in state::OutputStates; OBS's own signals then move them on.
//
// Actions: output.start {target} -> {started: [names]}; output.stop {target} -> {stopped: [names]}.

#include "command-dispatcher.hpp"

namespace fiji::obs {
class OutputBackend;
}
namespace fiji::state {
class OutputStates;
}

namespace fiji::controller {

class OutputController final : public CommandHandler {
public:
	OutputController(obs::OutputBackend &backend, state::OutputStates &states) : backend_(backend), states_(states) {}

	QStringList actions() const override { return {"output.start", "output.stop"}; }
	CommandResult handle(const Command &command) override;

private:
	obs::OutputBackend &backend_;
	state::OutputStates &states_;
};

} // namespace fiji::controller
