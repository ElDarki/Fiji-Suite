#pragma once

#include <obs.h>
#include <QString>
#include <string>

// Optional outbound connection to a Fiji-chan bot server. The plugin dials out (ws:// or wss://),
// so nothing has to be reachable from outside. Over it the bot can call the same requests, and
// receives the same events, as OBS WebSocket clients do (see docs/websocket.md and docs/bot-link.md).
// With no address set, nothing here runs.
namespace BotLink {

enum class State { Off, BadAddress, Connecting, Connected };

// Starts (or restarts) the link. An empty address turns it off.
void Start(const std::string &address);
// Closes the link and waits for its thread. Call from the UI thread.
void Stop();

State GetState();
// Why the last connection attempt failed; empty when there is nothing to report.
QString LastError();

} // namespace BotLink

// Sends an event to OBS WebSocket clients and to the bot, whichever are there.
void fiji_emit_event(const char *event, obs_data_t *data);
// True when someone would receive an event, so callers can skip building one otherwise.
bool fiji_events_wanted();
// Runs a request by name. False when there is no such request.
bool fiji_call_request(const char *name, obs_data_t *request, obs_data_t *response);
