# Controlling Fiji Suite from a bot

Fiji Suite does not run its own server. It adds requests and events to the WebSocket server that ships with
OBS (obs-websocket v5), so one connection controls both OBS itself and Fiji Suite.

## Turning it on

1. OBS: **Tools → WebSocket Server Settings** → enable the server and note the port (4455 by default) and
   password.
2. Fiji Suite: **Tools → Fiji Suite Settings → General → Allow remote control**. It is on by default. Turning
   it off removes every request and event below; the change applies the next time OBS starts.

Normal OBS functions (main-canvas scenes, sources, the main stream and recording, hotkeys) use the standard
obs-websocket requests. Everything below is specific to Fiji Suite.

## Calling a request

Fiji Suite's vendor name is `fiji-suite`. Send a standard `CallVendorRequest`:

```json
{
  "op": 6,
  "d": {
    "requestType": "CallVendorRequest",
    "requestId": "1",
    "requestData": {
      "vendorName": "fiji-suite",
      "requestType": "set_preview_scene",
      "requestData": { "canvas": "Vertical", "scene": "Gameplay" }
    }
  }
}
```

The reply's `responseData.responseData` always has `success` (bool), and `error` (string) when it failed.
Wherever a request takes `canvas`, it accepts the canvas name or its uuid.

## Requests

### Canvases and scenes

| Request | Input | Output |
|---|---|---|
| `version` | | `version` |
| `get_canvas` | | `canvas`: list of `{type, name, uuid, width, height}` |
| `get_scenes` | `canvas` | `scenes` |
| `current_scene` | `canvas` | `scene`, `scene_uuid`: the scene the canvas is outputting |
| `switch_scene` | `scene`, `canvas` (optional: all canvases if left out) | Puts the scene live. In studio mode the preview is left alone. |
| `get_transitions` | `canvas` | `transitions` |
| `switch_transition` | `canvas`, `transition` | |
| `transitions_add` | `canvas`, `name`, `type`, `settings` | |
| `transitions_remove` | `canvas`, `name` | |

### Studio mode

| Request | Input | Output |
|---|---|---|
| `get_studio_mode` | `canvas` | `enabled`, `program_scene`, `preview_scene` |
| `set_studio_mode` | `canvas`, `enabled` | |
| `set_preview_scene` | `canvas`, `scene` | With studio mode off this puts the scene live, like `switch_scene`. |
| `transition` | `canvas` | Sends the preview scene to program. Does nothing with studio mode off. |

### Outputs

| Request | Input | Output |
|---|---|---|
| `get_outputs` | | `outputs`: list of `{name, type, canvas, group, active, reconnecting, total_frames, dropped_frames, total_bytes}` |
| `start_output` / `stop_output` | `output` | |
| `start_all_outputs` / `stop_all_outputs` | | |
| `start_all_streams` / `stop_all_streams` | | |
| `start_all_recordings` / `stop_all_recordings` | | |
| `save_backtrack` | `output` | Saves the replay of a backtrack output. |
| `add_chapter` | `output`, `chapter_name` | |
| `get_groups` | | `groups`: list of `{name, live, outputs: [{name, active}]}` |
| `start_group` / `stop_group` | `group` | |

`get_outputs` lists the outputs Fiji Suite manages. OBS's own stream, recording, replay buffer and virtual
camera are controlled with the standard obs-websocket requests.

### Main canvas and overview

| Request | Input | Output |
|---|---|---|
| `get_status` | | `main_scene`, `main_scenes`, `canvases`: list of `{name, scene, studio_mode, preview_scene, scenes}`, `outputs`: list of `{name, type, canvas, group, main, active, reconnecting, uptime, total_frames, dropped_frames, total_bytes}`. Includes OBS's own outputs, marked `main`. |
| `set_main_scene` | `scene` | Changes the main canvas's scene. |
| `start_main` / `stop_main` | `output`: `stream`, `record`, `backtrack` or `virtual_cam` | Starts or stops one of OBS's own outputs. |

### Decks

| Request | Input | Output |
|---|---|---|
| `get_decks` | | `decks`: every deck with its buttons, steps and triggers (see `docs/decks.md`) |
| `save_deck` | `deck` | `deck` as stored. Adds it, or replaces the deck with the same `id`; missing ids are filled in. |
| `delete_deck` | `id` | |
| `press_button` | `button`, `data` (optional variables for the run) | Fails when there is no such button or it is busy. |
| `fire_trigger` | `button`, `data` | The same as `press_button`; used by the bot for Twitch triggers. |
| `release_button` | `button` | Runs the button's release list, if it has one. |
| `stop_button` | `button` (`all` stops every button and sound) | |
| `get_deck_state` | | `running`: list of `{button}` |
| `get_triggers` | | `triggers`: every trigger with its `button` and `deck` |
| `get_web_file` | `name` | `content`, `type`: a file of the deck web interface |

### Window

| Request | Input | Output |
|---|---|---|
| `show_multiview` | | Shows the Multiview dock. |
| `get_docks` | | `docks` |
| `dock_show` / `dock_hide` | `dock` | |
| `canvas_dock_show_panel` / `canvas_dock_hide_panel` | `canvas`, `panel` | |
| `get_live_scenes` | | `live_scenes` (the Favorite Scenes dock) |
| `live_scenes_add` / `live_scenes_remove` | `scene` | |

## Events

Events arrive as standard `VendorEvent` messages with `vendorName` `fiji-suite`; the fields below are in
`eventData.eventData`.

| Event | Fields |
|---|---|
| `switch_scene` | `canvas`, `old_scene`, `new_scene`, `width`, `height`: the canvas output changed scene |
| `preview_scene_changed` | same fields, for the scene selected in the canvas dock (the preview in studio mode) |
| `studio_mode_changed` | `canvas`, `enabled` |
| `start_output` | `output` |
| `stop_output` | `output`, `code`, `last_error` (when there was one) |
| `output_reconnecting` / `output_reconnected` | `output` |
| `outputs_changed` | the output list was edited in settings |
| `canvas_changed` | the canvas list was edited in settings |
| `scene_added` / `scene_removed` | `canvas`, `name`, `uuid` |
| `decks_changed` | a deck was saved or deleted |
| `button_state` | `button`, `running` |
| `say` | `text`: a deck button wants this said in chat (the bot does it) |
| `deck_notification` | `text`: a note for open deck pages |

## Fiji-chan bot link

The same requests and events are also available to a Fiji-chan bot without OBS's WebSocket server: paste the
bot's address into **Tools → Fiji Suite Settings → General → Fiji-chan bot address** and the plugin connects out
to it. This link is independent of the "Allow remote control" switch and is off while the field is empty.

Messages are JSON text frames:

| Direction | Message |
|---|---|
| plugin → bot | `{"type": "hello", "plugin": "fiji-suite", "version": "..."}` once, on connecting |
| bot → plugin | `{"type": "request", "id": "7", "request": "start_group", "data": {...}}` |
| plugin → bot | `{"type": "response", "id": "7", "data": {"success": true, ...}}` |
| plugin → bot | `{"type": "event", "event": "switch_scene", "data": {...}}` |

The plugin reconnects by itself, waiting 2 seconds after the first failure and up to 60 seconds between later ones.
