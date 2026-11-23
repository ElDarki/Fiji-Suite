# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Fiji Suite: an OBS Studio plugin (C++17, Qt 6, libobs, OBS 31+) with extra docks (canvases, outputs, scenes, sources,
stats…), stream decks served as web pages, the Fiji Sun / Fiji Moon themes, and an optional link to the Fiji-chan
Twitch bot (`../twitchbot`). `README.md` is the user guide; `docs/websocket.md`, `docs/decks.md` and
`docs/architecture.md` hold the details.

## Commands

```bash
# Build (Linux, out of tree; needs OBS, Qt 6 and the obs-studio dev packages, see .github/workflows/release.yaml)
cmake --preset ubuntu-x86_64
cmake --build build_x86_64 --parallel

# Tests
python3 tools/check-architecture.py                       # layer rules + bridge contract (no build needed)
node tests/import.test.js                                 # deck importers
cmake --build build_x86_64 --target fiji-arch-test fiji-core-test decks-engine-test
ctest --test-dir build_x86_64 --output-on-failure         # every C++ test plus the architecture rules

# Themes: edit tools/build-themes.py (never the generated data/themes files), then
python3 tools/build-themes.py
```

Releases: the version lives in `buildspec.json`, `data/manifest.json`, the Flatpak `metainfo.xml` and `CHANGELOG.md`
(CMake refuses to configure when the first two differ). Pushing a `vX.Y.Z` tag makes CI build Windows, Linux and
Flatpak and publish the release. Ask before committing, tagging or pushing.

## Architecture: MVC + State pattern. Read this before writing code.

New code goes into the layers below; `tools/check-architecture.py` enforces the rules (CI and CTest run it).
The code from before this layout (`src/docks`, `src/dialogs`, `src/utils`, `src/decks`, `src/fiji-suite.cpp`) is the
legacy list in that script. It moves into the layers one piece at a time, and the list may only shrink.

| Layer | Folder | What it is | May include |
|---|---|---|---|
| Model | `src/model/` | Plain data + rules (decks, settings, canvases, outputs), Qt Core only, signals on change | model |
| State | `src/state/` | State-pattern classes: the link, outputs, deck keys | state |
| Controller | `src/controller/` | `CommandDispatcher` + handlers: intents → model changes and libobs calls | model, state, obs port |
| View | `src/view/`, `data/web/` | Docks, dialogs, widgets, the deck web pages | model, state, controller |
| Bridge | `src/bridge/` | The plugin's Anti-Corruption Layer to the bot (`BRIDGE_CONTRACT.md`) | controller, model, state |
| OBS port | `src/obs/` | `ObsApi` (the libobs calls controllers need) and its real implementation; `ObsDataStore` | model |

### Model: local-first
- Every model persists through `model::LocalStore`; the real one is `obs::ObsDataStore`, which writes `obs_data_t` JSON
  into the plugin's config folder (`obs_module_config_path`) with safe writes.
- **The plugin works completely without the bot.** Nothing a streamer sets up (decks, outputs, canvases, settings) may
  live only on the bot or wait for it.
- Models are Qt Core only: no `obs.h`, no widgets, no envelopes. They emit signals when they change.

### State: explicit state machines
- The bot link is `state::LinkStateMachine` with exactly three states, one class each (`StandaloneState`,
  `ConnectingState`, `LinkedState`, each with `enter()`/`exit()`):
  - `STANDALONE` → `CONNECTING` when an address is set;
  - `CONNECTING` → `LINKED` when the HELLO handshake completes;
  - `CONNECTING` → `STANDALONE` on failure, with a `LinkReason` (bad address, refused, version mismatch);
  - `LINKED` → `CONNECTING` when the connection drops (the transport retries);
  - any state → `STANDALONE` when the address is cleared.
- Twitch-dependent controls (chat lines from deck buttons, Twitch triggers, "through the bot" deck links, bot status)
  bind their enabled state and tooltip to `twitchFeaturesAvailable()` and the accepted capabilities. They are
  **disabled with an explanation, never hidden**. Nothing throws, asserts or logs errors because the bot is missing;
  `STANDALONE` is a normal mode.
- Other life cycles follow the same pattern: an output (`IDLE`, `STARTING`, `ACTIVE`, `RECONNECTING`, `STOPPING`,
  `FAILED`), a deck key (idle, pressed, running, held). Transitions live in the state classes, not in `if` chains in
  views.

### Controller: one thread-safe dispatcher
- Every request to change OBS or a model becomes a `controller::Command` posted to the `CommandDispatcher`, whatever its
  source: a bridge COMMAND (link thread), a hotkey, a dock button, the local deck server or obs-websocket.
- `post()` is thread-safe. One consumer runs the commands in order on the OBS/UI thread
  (`QMetaObject::invokeMethod(…, Qt::QueuedConnection)`), so handlers never race with OBS or each other.
- Handlers call libobs **only through `obs::ObsApi`** and return a `CommandResult` instead of throwing. They are the
  only code outside `src/obs/` that drives libobs.
- Results go back as ACKs (bridge) or signals (UI).

### View
- Views observe models and send intents to controllers. **No `obs_*` calls, no `obs.h`, no envelopes** in views.
- Visual rules come from the Fiji-chan style guide (bot repo `DESIGN.md`); the OBS look comes from the Fiji Sun / Moon
  themes. Deck pages follow the guide's deck section.

### Bridge: contract-first Anti-Corruption Layer
- `BRIDGE_CONTRACT.md` (the same bytes as `../twitchbot/BRIDGE_CONTRACT.md`) defines the envelope (`version`, `id`,
  `type` COMMAND | EVENT | ACK | HELLO | SYNC_STATE, `action`, `payload`), the HELLO capability negotiation,
  SYNC_STATE and every action.
- **Never build, read or change a payload field the contract does not define. Update `BRIDGE_CONTRACT.md` first**
  (bump its version, change both repos' copies together, add a changelog line), then `src/bridge/`, then the code
  behind it. `tools/check-architecture.py` checks the version constant, the message types and the copies.
- Only `src/bridge/` knows the wire format. It turns COMMANDs into `Command`s and model/state signals into EVENTs, and
  sends EVENTs only for capabilities both sides accepted.

### Adding something
- A new feature: model (+ its tests), state machine if it has a life cycle, a command handler behind the dispatcher,
  then the view. Test models, states and handlers without OBS (a fake `ObsApi`, an in-memory `LocalStore`).
- A new bridge action: the contract first, in both repos.
- Legacy code you touch for a feature: move that piece into the layers in the same change, or leave it as it is; do not
  add new legacy.
