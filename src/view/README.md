# View layer

Docks, dialogs and widgets (Qt Widgets), plus the deck web pages in `data/web/`.

- Views **observe models** (Qt signals) and **send intents to controllers** (`CommandDispatcher::post` or a controller
  method). They never call `obs_*`, never include `obs.h`, and never build or read bridge envelopes.
- Twitch-dependent controls bind their enabled state and tooltip to
  `fiji::state::LinkStateMachine::twitchFeaturesAvailable()`: disabled with an explanation in `STANDALONE` and
  `CONNECTING`, never hidden, never an error.
- Translated text comes in from the caller (views do not include `obs-module.h`).
- Every view source is listed in `FIJI_VIEW_SOURCES` and built into `fiji-view-test` (`tests/view-test.cpp`, Qt Widgets,
  offscreen, no OBS): a view that reaches libobs or legacy code does not link, and its behaviour is tested there.
- Visual rules come from the style guide (Fiji Sun / Fiji Moon themes, `tools/build-themes.py`).

The docks and dialogs from before this layout still live in `src/docks/`, `src/dialogs/` and `src/utils/widgets/`;
they move here one at a time (see the legacy list in `tools/check-architecture.py`).
