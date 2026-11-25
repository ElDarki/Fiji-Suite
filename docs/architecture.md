# Fiji Suite architecture

Model-View-Controller plus the State pattern, with a contract-first bridge to the Fiji-chan bot. The rules are in
`CLAUDE.md`; `tools/check-architecture.py` enforces them.

## Layers

```
            ┌──────────────── View (src/view, data/web) ────────────────┐
            │ docks, dialogs, widgets, deck pages                        │
            └──────┬─────────────────────────────▲───────────────────────┘
          intents  │                             │ signals
                   ▼                             │
 hotkeys ──► ┌──────────── Controller (src/controller) ─────────┐
 local deck  │ CommandDispatcher (thread-safe queue)             │
 server  ──► │   └─ handlers ── ObsApi (src/obs) ──► libobs      │
 obs-ws  ──► └──────┬────────────────────────────────────────────┘
                    │ change                    ▲ Command / CommandResult
                    ▼                           │
            ┌── Model (src/model) ──┐   ┌── Bridge ACL (src/bridge) ──┐ ◄── WebSocket ──► Fiji-chan bot
            │ decks, settings, …    │   │ envelopes ⇄ Commands/events │     (BRIDGE_CONTRACT.md)
            │ LocalStore ─► ObsData │   └─────────────▲───────────────┘
            └───────────────────────┘                 │ state, capabilities
            ┌── State (src/state) ──────────────────────────────────────┐
            │ LinkStateMachine: STANDALONE / CONNECTING / LINKED; outputs; deck keys │
            └───────────────────────────────────────────────────────────┘
```

## Threads

| Thread | Runs |
|---|---|
| OBS/UI (Qt main) | views, models, state machines, the dispatcher's consumer, every libobs call made by handlers |
| Link transport | the bot WebSocket; `BridgeAcl::onFrame` parses and posts Commands; ACKs are queued back to it |
| Local deck server | HTTP for the deck pages; posts Commands |
| obs-websocket | vendor requests; posts Commands and waits for their CommandResult |

Only the dispatcher crosses threads: everything else stays on the thread that owns it.

## The link states

```
 STANDALONE ──address set──► CONNECTING ──HELLO done──► LINKED
     ▲                          │  ▲                       │
     └──failure (reason)────────┘  └────────dropped────────┘
 any ──address cleared──► STANDALONE
```

`STANDALONE` is a full working mode: decks, outputs and docks all work. Twitch-dependent controls are disabled with an
explanation until `LINKED`, and the accepted capabilities say which bot features are there.

## Moving the legacy code

`src/docks`, `src/dialogs`, `src/utils`, `src/decks` and `src/fiji-suite.cpp` predate the layers. Each piece moves in
its own change: its data into a model, its life cycle into a state, its libobs calls into a handler behind `ObsApi`,
its widgets into `src/view`, with tests for the model, state and handler. Then its entry leaves the legacy list.

Moved so far (0.6.0): the bot link. The curl transport, the ACL and `BotLink` are in `src/bridge/`; the link's life cycle
is `state::LinkStateMachine`; bot commands run through `controller::CommandDispatcher` and `ObsCommandHandler`; and
`obs::RequestObsApi` reaches OBS through the plugin's request table until those calls move into it as direct libobs
calls. `fiji-suite.cpp` builds them when the module loads.

Moved in 0.7.0: the decks. `model::DeckModel` (saved through `obs::ObsDataStore`, still `decks.json`) and the deck
engine (`model/deck-engine`), `state::DeckKeys` (idle, held, running, held and running),
`controller::DeckController` (every deck action, from the bridge, the local deck server, obs-websocket and the
hotkeys, goes through the dispatcher), and behind `obs/deck-port.hpp` the steps that act on OBS
(`obs::ObsDeckSteps`) and the hotkeys (`obs::ObsDeckHotkeys`). The deck pages are the view.

Moved in 0.8.0: starting and stopping outputs, and their life cycle. `model::planOutputs` says what a target means
(all, a group, OBS's own outputs, a name), `state::OutputStates` follows each output (IDLE, STARTING, ACTIVE,
RECONNECTING, STOPPING, FAILED) from OBS's signals, and `controller::OutputController` serves `output.start` and
`output.stop` through `obs::OutputBackend`. The bridge reports `output.state` from the state machine. The output
dock, its widgets and the encoder setup are still legacy views; they move next.

Moved in 0.9.0: the look. `model::planThemes` decides which theme files to write or remove, `obs::installLook`
registers the fonts and copies the themes, and `view::showRestartNotice` is the first view. The update check is
`model::isNewer`, `controller::UpdateChecker`, `obs::GitHubReleases` (behind `obs/update-port.hpp`) and
`view::showUpdateOffer`.

Moved (unreleased): the Multiview's rules. `model::TileOutputs` says which outputs a canvas tile shows, their status
lines, bitrate and dropped-frame warning, and whether the canvas is on air; `model::multiviewColumns` lays out the
grid. The tiles that draw the canvases stay in `src/docks/multiview-dock.cpp` until there is a layer for OBS's
display widget (the canvas dock needs the same).
