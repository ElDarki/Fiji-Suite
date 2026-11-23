# Bridge contract: Fiji-chan bot ↔ Fiji Suite plugin

This file is the **Anti-Corruption Layer (ACL) contract** between the Fiji-chan bot (`twitchbot`) and the Fiji Suite
OBS plugin (`fiji-suite`). The same file sits at the root of both repositories, byte for byte; a test in each repo
checks it. Contract version: **1.1**.

## 1. Purpose and boundaries

- The bridge is the **only** place where either side meets the other's language. On the bot that is
  `app/adapters/outbound/obs_bridge/`; on the plugin, `src/bridge/`.
- Bot domain types (`app/domain/…`) and plugin models (`src/model/…`) **never** go on the wire as they are, and wire
  fields **never** reach a domain object or a model untranslated. Each ACL maps wire ⇄ its own types.
- Each side works without the other. The plugin is local-first and fully usable in `STANDALONE`; the bot answers OBS
  chat commands politely through its `NullObsAdapter` while no plugin is linked.
- The bot's web pages talk only to the bot's HTTP API. They never see an envelope.

## 2. Transport and envelope

The plugin dials the bot: `wss://<bot>/m/obs/ws/<overlay token>` (or `ws://` on a LAN). One JSON object per WebSocket
text frame, JSON-RPC style:

```json
{ "version": "1.1", "id": "6f1c…", "type": "COMMAND", "action": "scene.switch", "payload": { "scene": "BRB" } }
```

| Field | Type | Rule |
|---|---|---|
| `version` | string `MAJOR.MINOR` | The contract version the sender speaks. |
| `id` | string or `null` | Required (unique per sender, e.g. a UUID) on `COMMAND`, `HELLO` and `SYNC_STATE`; an `ACK` echoes the `id` it answers; `EVENT` has `null`. |
| `type` | `COMMAND` \| `EVENT` \| `ACK` \| `HELLO` \| `SYNC_STATE` | See below. |
| `action` | string `area.verb` | From the catalogue (section 5). `HELLO` uses `link.hello`, `SYNC_STATE` uses `state.snapshot`, an `ACK` repeats the action it answers. |
| `payload` | object | Validated against the action's schema (section 6). Always an object, `{}` when empty. |

Unknown top-level fields are ignored. A frame that is not valid JSON, or misses `version`, `type` or `action`, is
dropped and logged once; it never closes the link.

### Message types

- **COMMAND**: asks the other side to do something. Every COMMAND is answered by exactly one **ACK** within
  **10 seconds**; no answer counts as `{ok: false, error: {code: "TIMEOUT", retryable: true}}`.
- **ACK**: the answer. `payload` is `{ "ok": true, "result": {…} }` or
  `{ "ok": false, "error": { "code": "…", "message": "…", "retryable": false } }`.
- **EVENT**: news, fire and forget, no ACK. Sent only for capabilities both sides accepted (section 3).
- **HELLO**: the handshake (section 3). Answered by the other side's HELLO, not by an ACK.
- **SYNC_STATE**: a full snapshot of the accepted capabilities (section 4). Its `id` echoes the `state.sync` COMMAND
  that asked for it, or is a fresh id when sent after the handshake.

### Error codes

| Code | Meaning | Retryable |
|---|---|---|
| `BAD_REQUEST` | The payload does not match the schema. | no |
| `UNKNOWN_ACTION` | The action is not in the catalogue, or its capability was not accepted. | no |
| `NOT_FOUND` | The scene, output, deck or button does not exist. | no |
| `BUSY` | The target is already doing it (a button still running, an output starting). | yes |
| `OBS_ERROR` | libobs refused; `message` says why. | no |
| `TIMEOUT` | No ACK within 10 s. | yes |
| `NOT_LINKED` | Used by the bot's NullObsAdapter: no plugin is linked. Never sent on the wire. | yes |

### Versions

- Same `MAJOR`: the link works; each side uses only what both know. Unknown actions get
  `ACK {ok:false, error:{code:"UNKNOWN_ACTION"}}` (for a COMMAND) or are ignored (EVENT).
- Different `MAJOR`: the receiver of the first HELLO answers with its own HELLO carrying
  `"accepted": []` and `"reject": "version"`, then closes the socket with code 4001. The plugin stays `STANDALONE` and
  shows "The bot speaks a different bridge version; update the plugin or the bot."

## 3. The HELLO handshake and capability negotiation

1. The plugin connects and sends first:
   ```json
   { "version": "1.1", "id": "h-1", "type": "HELLO", "action": "link.hello",
     "payload": { "role": "plugin", "app": "fiji-suite", "app_version": "0.6.0",
                  "capabilities": ["scenes", "transitions", "outputs", "output.alerts", "decks", "decks.pages", "chat"] } }
   ```
2. The bot answers with its own HELLO. `accepted` is the intersection of both lists, in the plugin's order:
   ```json
   { "version": "1.1", "id": "h-1", "type": "HELLO", "action": "link.hello",
     "payload": { "role": "bot", "app": "fiji-chan", "app_version": "1.4.0",
                  "capabilities": ["scenes", "transitions", "outputs", "output.alerts", "decks", "decks.pages", "chat", "triggers"],
                  "accepted": ["scenes", "transitions", "outputs", "output.alerts", "decks", "decks.pages", "chat"] } }
   ```
3. Before both HELLOs nothing else may be sent; anything else is dropped. A HELLO arriving later restarts the
   negotiation.
4. Unknown capabilities are ignored, never errors. A side must not send a COMMAND or EVENT whose capability is not in
   `accepted`.

| Capability | What it covers |
|---|---|
| `scenes` | Scene lists and switching, per canvas. |
| `transitions` | Choosing the transition of a canvas. |
| `outputs` | Output list, start and stop, output state events. |
| `output.alerts` | Dropped-frame reports. |
| `decks` | Deck list, pressing, releasing and stopping buttons, button state events. |
| `decks.pages` | Serving the deck web pages through the bot (`deck.file`, `look.get`). |
| `chat` | The plugin asks the bot to say something in chat (deck buttons). |
| `triggers` | Twitch events firing deck buttons (`deck.triggers`, `deck.fire`). |

## 4. SYNC_STATE

Right after a successful handshake the plugin sends one SYNC_STATE with everything the accepted capabilities cover.
Either side may later ask for another with the COMMAND `state.sync`; the plugin then answers with an ACK
`{ok: true, result: {}}` followed by a SYNC_STATE whose `id` equals the command's `id`. Events after a snapshot are
deltas on top of it; a receiver that misses one asks for a new snapshot instead of guessing.

```json
{ "version": "1.0", "id": "s-1", "type": "SYNC_STATE", "action": "state.snapshot",
  "payload": {
    "canvases": [ { "canvas": "Main", "main": true, "program": "Just chatting", "preview": null, "studio_mode": false,
                    "scenes": ["Starting soon", "Just chatting", "BRB"], "transitions": ["Fade", "Cut"] } ],
    "outputs":  [ { "output": "Twitch", "kind": "stream", "canvas": "Main", "group": "Main", "main": true,
                    "state": "ACTIVE", "uptime": 300, "dropped_frames": 0, "total_frames": 18000 } ],
    "decks":    [ { "id": "d1", "name": "Stream", "buttons": 15 } ],
    "running":  ["b7"] } }
```

`state` of an output is one of `IDLE`, `STARTING`, `ACTIVE`, `RECONNECTING`, `STOPPING`, `FAILED`.

Optional fields (1.1): `main` is true for OBS's own main canvas and for the outputs OBS itself owns (its stream,
recording, replay buffer and virtual camera); left out means false. `uptime` is how many whole seconds an output has
been active when the snapshot was taken; left out means 0.

## 5. Action catalogue

Direction: **B→P** bot to plugin, **P→B** plugin to bot.

### link and state

| Action | Type | Dir | Payload | ACK result |
|---|---|---|---|---|
| `link.hello` | HELLO | both | `role`, `app`, `app_version`, `capabilities`, (`accepted`, `reject` from the bot) | n/a |
| `state.sync` | COMMAND | both | `{}` | `{}`, then a SYNC_STATE |
| `state.snapshot` | SYNC_STATE | P→B | see section 4 | n/a |

### scenes (`scenes`, `transitions`)

| Action | Type | Dir | Payload | ACK result |
|---|---|---|---|---|
| `scene.switch` | COMMAND | B→P | `scene`, `canvas`? (the main canvas when left out) | `{}` |
| `transition.set` | COMMAND | B→P | `canvas`, `transition` | `{}` |
| `transition.run` | COMMAND | B→P | `canvas` (studio mode: puts the canvas's preview on program) | `{}` |
| `scene.program` | EVENT | P→B | `canvas`, `scene`, `previous` | n/a |
| `scene.list` | EVENT | P→B | `canvas`, `scenes` | n/a |

### outputs (`outputs`, `output.alerts`)

| Action | Type | Dir | Payload | ACK result |
|---|---|---|---|---|
| `output.start` | COMMAND | B→P | `target`: an output name, a group name, `stream` \| `record` \| `backtrack` \| `virtual_cam`, or `all` | `{ "started": [names] }` |
| `output.stop` | COMMAND | B→P | `target` as above | `{ "stopped": [names] }` |
| `output.state` | EVENT | P→B | `output`, `kind`, `state`, `error`? | n/a |
| `output.dropped` | EVENT | P→B | `output`, `dropped_frames`, `total_frames` | n/a |

`output.dropped` is sent about every 15 seconds for each active output. Its counts are cumulative since the output
started; the bot works out the share dropped between two reports.

### decks (`decks`, `decks.pages`, `triggers`)

| Action | Type | Dir | Payload | ACK result |
|---|---|---|---|---|
| `deck.list` | COMMAND | B→P | `{}` | `{ "decks": [...] }` (deck format: plugin `docs/decks.md`) |
| `deck.press` | COMMAND | B→P | `button`, `vars`? | `{}` |
| `deck.release` | COMMAND | B→P | `button` | `{}` |
| `deck.stop` | COMMAND | B→P | `button` (`all` stops everything) | `{}` |
| `deck.save` | COMMAND | B→P | `deck` | `{ "deck": {...} }` |
| `deck.delete` | COMMAND | B→P | `id` | `{}` |
| `deck.file` | COMMAND | B→P | `name` (a file of the deck web pages) | `{ "content", "type" }` (text files only) |
| `look.get` | COMMAND | B→P | `{}` | `{ "dark": bool }` |
| `deck.triggers` | COMMAND | B→P | `{}` | `{ "triggers": [...] }` |
| `deck.fire` | COMMAND | B→P | `button`, `vars`? | `{}` |
| `deck.changed` | EVENT | P→B | `{}` | n/a |
| `deck.key` | EVENT | P→B | `button`, `running` | n/a |
| `deck.notice` | EVENT | P→B | `text` | n/a |

### chat (`chat`)

| Action | Type | Dir | Payload | ACK result |
|---|---|---|---|---|
| `chat.say` | EVENT | P→B | `text` (at most 480 characters) | n/a |

## 6. Machine-readable catalogue

Both ACLs and both test suites read this block. It must stay in step with the tables above.

```json bridge-contract
{
  "version": "1.1",
  "types": ["COMMAND", "EVENT", "ACK", "HELLO", "SYNC_STATE"],
  "errors": ["BAD_REQUEST", "UNKNOWN_ACTION", "NOT_FOUND", "BUSY", "OBS_ERROR", "TIMEOUT", "NOT_LINKED"],
  "output_states": ["IDLE", "STARTING", "ACTIVE", "RECONNECTING", "STOPPING", "FAILED"],
  "capabilities": ["scenes", "transitions", "outputs", "output.alerts", "decks", "decks.pages", "chat", "triggers"],
  "actions": {
    "link.hello":     {"type": "HELLO", "dir": "both", "capability": null,
                       "payload": {"required": ["role", "app", "app_version", "capabilities"],
                                   "optional": ["accepted", "reject"]}},
    "state.sync":     {"type": "COMMAND", "dir": "both", "capability": null,
                       "payload": {"required": [], "optional": []}, "result": []},
    "state.snapshot": {"type": "SYNC_STATE", "dir": "P>B", "capability": null,
                       "payload": {"required": [], "optional": ["canvases", "outputs", "decks", "running"]}},
    "scene.switch":   {"type": "COMMAND", "dir": "B>P", "capability": "scenes",
                       "payload": {"required": ["scene"], "optional": ["canvas"]}, "result": []},
    "transition.set": {"type": "COMMAND", "dir": "B>P", "capability": "transitions",
                       "payload": {"required": ["canvas", "transition"], "optional": []}, "result": []},
    "transition.run": {"type": "COMMAND", "dir": "B>P", "capability": "transitions",
                       "payload": {"required": ["canvas"], "optional": []}, "result": []},
    "scene.program":  {"type": "EVENT", "dir": "P>B", "capability": "scenes",
                       "payload": {"required": ["canvas", "scene"], "optional": ["previous"]}},
    "scene.list":     {"type": "EVENT", "dir": "P>B", "capability": "scenes",
                       "payload": {"required": ["canvas", "scenes"], "optional": []}},
    "output.start":   {"type": "COMMAND", "dir": "B>P", "capability": "outputs",
                       "payload": {"required": ["target"], "optional": []}, "result": ["started"]},
    "output.stop":    {"type": "COMMAND", "dir": "B>P", "capability": "outputs",
                       "payload": {"required": ["target"], "optional": []}, "result": ["stopped"]},
    "output.state":   {"type": "EVENT", "dir": "P>B", "capability": "outputs",
                       "payload": {"required": ["output", "kind", "state"], "optional": ["error"]}},
    "output.dropped": {"type": "EVENT", "dir": "P>B", "capability": "output.alerts",
                       "payload": {"required": ["output", "dropped_frames", "total_frames"], "optional": []}},
    "deck.list":      {"type": "COMMAND", "dir": "B>P", "capability": "decks",
                       "payload": {"required": [], "optional": []}, "result": ["decks"]},
    "deck.press":     {"type": "COMMAND", "dir": "B>P", "capability": "decks",
                       "payload": {"required": ["button"], "optional": ["vars"]}, "result": []},
    "deck.release":   {"type": "COMMAND", "dir": "B>P", "capability": "decks",
                       "payload": {"required": ["button"], "optional": []}, "result": []},
    "deck.stop":      {"type": "COMMAND", "dir": "B>P", "capability": "decks",
                       "payload": {"required": ["button"], "optional": []}, "result": []},
    "deck.save":      {"type": "COMMAND", "dir": "B>P", "capability": "decks",
                       "payload": {"required": ["deck"], "optional": []}, "result": ["deck"]},
    "deck.delete":    {"type": "COMMAND", "dir": "B>P", "capability": "decks",
                       "payload": {"required": ["id"], "optional": []}, "result": []},
    "deck.file":      {"type": "COMMAND", "dir": "B>P", "capability": "decks.pages",
                       "payload": {"required": ["name"], "optional": []}, "result": ["content", "type"]},
    "look.get":       {"type": "COMMAND", "dir": "B>P", "capability": "decks.pages",
                       "payload": {"required": [], "optional": []}, "result": ["dark"]},
    "deck.triggers":  {"type": "COMMAND", "dir": "B>P", "capability": "triggers",
                       "payload": {"required": [], "optional": []}, "result": ["triggers"]},
    "deck.fire":      {"type": "COMMAND", "dir": "B>P", "capability": "triggers",
                       "payload": {"required": ["button"], "optional": ["vars"]}, "result": []},
    "deck.changed":   {"type": "EVENT", "dir": "P>B", "capability": "decks",
                       "payload": {"required": [], "optional": []}},
    "deck.key":       {"type": "EVENT", "dir": "P>B", "capability": "decks",
                       "payload": {"required": ["button", "running"], "optional": []}},
    "deck.notice":    {"type": "EVENT", "dir": "P>B", "capability": "decks",
                       "payload": {"required": ["text"], "optional": []}},
    "chat.say":       {"type": "EVENT", "dir": "P>B", "capability": "chat",
                       "payload": {"required": ["text"], "optional": []}}
  }
}
```

## 7. Change rules

1. **Contract first.** No code may build, read or change a payload field, an action or a message type that this file
   does not define. Change this file first, then the ACLs, then the code behind them.
2. **Versioning.** Adding an action, an optional field or a capability bumps `MINOR`. Removing or renaming anything,
   or making a field required, bumps `MAJOR`.
3. **Both copies, one change set.** Update the file in both repositories together (same bytes) and add a line to the
   changelog below. The tests fail when the two copies differ or the code's version constant does not match.
4. Only the ACL folders (`app/adapters/outbound/obs_bridge/`, `src/bridge/`) touch envelopes.

## 8. Migration from today's link (contract "0")

Up to plugin 0.5.x the link carries the plugin's obs-websocket-style messages: `{type: "request", id, request, data}`
answered by `{type: "response", …}`, raw events (`switch_scene`, `stop_output`, `button_state`, `say`, …) and
`get_status` polling. The bot's ACL keeps a `legacy_v0` translator that maps those to the same domain calls and events
until plugin 0.6.0 (the first to send HELLO) is installed. The bot decides on the plugin's first frame: a HELLO
envelope means contract 1.x; a contract-0 frame (contract-0 plugins open with `{type: "hello", plugin, version}`)
means contract 0; and 5 seconds of silence also means contract 0. Support for contract 0 is removed in a later
release and noted in both changelogs.

| Contract 0 | Contract 1.0 |
|---|---|
| `switch_scene` / `set_main_scene` request | `scene.switch` (with / without `canvas`) |
| `switch_transition` request | `transition.set` |
| `transition` request | `transition.run` |
| `start_main` / `start_output` / `start_group` / `start_all_outputs` | `output.start` |
| `stop_main` / `stop_output` / `stop_group` / `stop_all_outputs` | `output.stop` |
| `get_deck_state` | the SYNC_STATE `running` list plus `deck.key` events |
| `get_decks`, `press_button`, `release_button`, `stop_button`, `save_deck`, `delete_deck` | `deck.list`, `deck.press`, `deck.release`, `deck.stop`, `deck.save`, `deck.delete` |
| `get_web_file`, `get_look` | `deck.file`, `look.get` |
| `get_triggers`, `fire_trigger` | `deck.triggers`, `deck.fire` |
| `get_status` polling | SYNC_STATE + `state.sync` |
| events `switch_scene`, `start_output`/`stop_output`/`output_reconnecting`/`output_reconnected`, `decks_changed`, `button_state`, `say`, `deck_notification` | `scene.program`, `output.state`, `deck.changed`, `deck.key`, `chat.say`, `deck.notice` |

## Changelog

| Version | Date | Change |
|---|---|---|
| 1.0 | 2026-11-23 | First contract: envelope, HELLO with capabilities, SYNC_STATE, catalogue, contract-0 migration table. |
| 1.1 | 2026-11-23 | `transition.run`; `scene.switch` without `canvas` means the main canvas; `main` and `uptime` in SYNC_STATE; `all` as an output target; `output.dropped` cadence; how the bot tells contract 0 from 1.x. |
