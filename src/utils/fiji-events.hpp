#pragma once

#include <obs.h>

// The plugin's events and requests, shared by obs-websocket clients, the local deck server and the bot link
// (src/bridge/, which translates them into the bridge contract). Implemented in obs-websocket.cpp.

// Sends an event to OBS WebSocket clients, the deck pages and the bot, whichever are there.
void fiji_emit_event(const char *event, obs_data_t *data);
// True when someone would receive an event, so callers can skip building one otherwise.
bool fiji_events_wanted();
// Runs a request by name. False when there is no such request.
bool fiji_call_request(const char *name, obs_data_t *request, obs_data_t *response);

// The bot link's state machine (src/state/), for views that show the link or disable Twitch-only controls; null
// before OBS has finished loading. And the address setter. Both live in fiji-suite.cpp, the plugin's composition.
#include <QString>
namespace fiji::state {
class LinkStateMachine;
}
namespace fiji::controller {
class DeckController;
}
namespace fiji::model {
class ProfileModel;
}
// The current OBS profile's Fiji Suite settings (src/model/), for the parts that have moved onto it; null before the
// module has loaded.
fiji::model::ProfileModel *fiji_profile();
fiji::state::LinkStateMachine *fiji_link_state();
// The deck controller (src/controller/), for the deck requests; null before the module has loaded.
fiji::controller::DeckController *fiji_decks();
void fiji_set_bot_address(const QString &address);
