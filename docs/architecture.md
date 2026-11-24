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
calls. `fiji-suite.cpp` builds them once OBS has finished loading.

Moved in 0.7.0: the decks. `model::DeckModel` keeps them (decks.json, through `obs::ObsDataStore`); the deck engine
(`src/state/deck-engine.*`) is the life cycle of key runs; `controller::DeckController` presses, releases and stops
them, runs the engine's steps and serves the bridge's deck actions; the steps that act on OBS and the hotkeys are the
`obs::DeckSteps` and `obs::DeckHotkeys` ports (`obs-deck-steps.cpp`, `obs-deck-hotkeys.cpp`). Left in `src/decks/`: the
local deck server.

Also moved in 0.7.0: each output's life cycle (`state::OutputLifecycle`, fed by the output widget), starting and
stopping outputs together (`controller::OutputsController` over the `obs::Outputs` port the output dock implements),
and the themes (`model::themes` copies them, `obs::InstallLook` puts the fonts and themes in place,
`view::ShowRestartNotice` asks for a restart). Still legacy: the docks and dialogs themselves, and the per-profile
settings they share (`current_profile_config` in `fiji-suite.cpp`).

The per-profile settings move onto `model::ProfileModel` one part at a time. Its first-run defaults
(`model::withFirstRunDefaults`) and the file (`obs::LoadProfileFile`, `obs::SaveProfileFile`, fiji-suite.json in the OBS
profile folder) are in the layers. It owns which of OBS's own outputs the output dock shows, the hotkey bindings
(start/stop all, groups, multiview over preview), Fiji Suite's output list, the canvas list, and the docks' view state (layouts, preview lock, the
settings dialog's size; `ProfileModel::isViewKey`). The output widgets and
canvas docks keep a settings object of their own and hand their changes back (`OutputWidget::Publish`,
`fiji_publish_canvas`). The parts it owns are
mirrored into `current_profile_config` for the code that has not moved, and written over it on save; each dock that
moves takes its part off the old object.
