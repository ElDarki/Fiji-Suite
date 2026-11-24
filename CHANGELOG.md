# Changelog

Versions follow [semantic versioning](https://semver.org); the README says what each part means here.

## Unreleased

- **Windows comes as an installer** (`.msi`): run it and the plugin is in place for every user of the PC; a newer one
  replaces the older version, and it uninstalls from Settings → Apps.
- **Needs OBS 33 or newer**: the Windows build now targets OBS 33.0.3, like the Linux and Flatpak builds.
- The recording and backtrack output dialog: a new output saves the filename it shows (the pattern followed by the
  output's name, not the bare pattern), and editing an output with no filename or format fills in the defaults
  instead of leaving them empty.
- The Stats dock's rates no longer start with a spike (an output that was already running showed its whole total as
  its first second), and its graphs are drawn from each rate's last minutes, scaled to what is shown.
- Each Fiji Suite output reports its real state: starting, live, reconnecting, stopping, stopped or failed. The bot and
  obs-websocket clients now hear about a start when the output is actually live, not when it was asked for, and two new
  events say when a start or stop was asked for (`output_starting`, `output_stopping`; docs/websocket.md).
- Deck sounds follow OBS 33's audio monitoring switch (on or off), and the deprecated call is gone when built for
  OBS 33.
- Inside: starting and stopping outputs together (all, streams, recordings, groups, one after another) is one
  outputs controller, tested without OBS.
- Inside: the decks follow the MVC + State layout: a deck model saved locally (the same decks.json), the deck engine as
  the state of each key's run, one deck controller for presses from the deck pages, hotkeys, obs-websocket and the bot,
  and the OBS steps and hotkeys behind ports, with tests for the model, the store and the controller.
- Inside: each profile's settings (fiji-suite.json: canvases, outputs, hotkeys, which OBS outputs the dock shows, dock
  layouts) live in one profile model instead of an object every dock changed in place. The file keeps its format, and
  keys the plugin does not know are kept. Tested without OBS.
- Inside: the docks and dialogs start moving into the view layer, tested with Qt and no OBS: the name dialog and the
  Capture dock (over a capture controller and an OBS port for the capture sources), and the Live Scenes dock (a model
  saved with the scene collection as before, a controller, and an OBS port), and the recording / backtrack output dialog
  (one dialog for new and edited outputs, its rules in a model), and the Stats dock (a table model fed once a second
  by a controller over an OBS port). Views get their text through `view::text`.

## 0.6.0

- The link to the Fiji-chan bot speaks the new bridge contract (`BRIDGE_CONTRACT.md` 1.2): a handshake that agrees
  on what both sides can do, answers within 10 seconds, and full snapshots of OBS instead of the bot asking every
  15 seconds. **Needs Fiji-chan 1.4 or later**; older bots never finish the handshake.
- The bot address shows the link's state as it changes (off, connecting with the last error, connected), says why
  it stopped (a bad address, a bot speaking another link version), and explains what waits while OBS is not linked.
- The deck editor says whether triggers can fire right now.
- Inside: the plugin follows Model-View-Controller plus the State pattern (`CLAUDE.md`, `docs/architecture.md`).
  Every bot command runs through one thread-safe dispatcher on OBS's own thread, and the link is an explicit
  STANDALONE / CONNECTING / LINKED state machine, with tests for both.

## 0.5.0

- Fiji Sun and Fiji Moon are now themes of their own: pick them in **Settings → Appearance → Theme** instead of
  as a style of Yami. If you had picked one in 0.4.0, it stays picked.
- They follow Fiji-chan's style guide, not just her colours: pill buttons in Nunito Sans, a teal primary look with
  a cel edge for anything switched on or live, her rounded checkboxes, chip tabs and selected rows, cards for
  group boxes, dock titles in her display face (Zen Maru Gothic by day, Shippori Mincho by night) with an ink line
  by day and a gold hairline by night, and fields that turn teal when you type in them.
- The first time Fiji Suite adds the themes it reminds you to restart OBS once, since OBS only looks for new
  themes when it starts.
- The encoder tabs in Fiji Suite Settings take their look from the OBS theme.
- The deck pages follow the style guide's deck: 96 px keys with a cel edge that share the width in a narrow dock,
  a teal ring and dot on a key that is running, **Edit deck** as the board's main button, an editor with dashed
  slots to add keys, labels above every field and a save bar, her icons instead of text symbols, and her face on
  empty pages.

## 0.4.0

- **Fiji Sun and Fiji Moon**: two OBS themes in Fiji-chan's colours, with her fonts (Nunito Sans, Zen Maru Gothic).
  Fiji Suite copies them into your OBS themes folder; restart OBS once, then pick one in **Settings → Appearance**.
  They work on OBS 31, 32 and 33.
- The Fiji Suite docks follow whichever OBS theme you use: active output buttons take the theme's colour, like
  OBS's own controls, and themes can colour the multiview's live border and warnings.
- The deck pages have Fiji-chan's Sun and Moon looks. They start in Moon when OBS uses a dark theme; the ☀/☾
  button or `?mood=sun|moon` in the address changes it. Bigger touch targets on phones.
- New WebSocket request `get_look`.

## 0.3.0

- Update check: when OBS has finished starting, Fiji Suite asks GitHub once whether a newer release exists and,
  if so, offers to open the download page, skip that version, or ask again next time. It makes no other
  requests. Turn it off with **Check for Fiji Suite updates when OBS starts** in the settings.

## 0.2.0

- Decks: boards of buttons that run timed steps on OBS (scenes, sources, filters, text, mute, sounds), with
  variables, waits and conditions, a hotkey per button, and triggers from the Fiji-chan bot.
- The decks open in a browser, an OBS browser dock, or a phone on the local network, served from inside OBS on
  a secret address; the same pages are available through the bot.
- An editor for decks and an importer for SAMMI decks, which reports anything it could not convert.
- A source can be named by its scene item number (`# 21`), as SAMMI decks do.

## 0.1.1

- Fixed a crash when OBS starts: the multiview asked for OBS's own outputs before OBS had created them.
- The bot link now connects only after OBS has finished loading.
- New: the multiview can be shown in place of the main preview (Tools menu, the multiview's right-click
  menu, or a hotkey).

## 0.1.0

The first version of Fiji Suite.

- Extra canvases and clone canvases in their own docks, with stream, record, replay and virtual camera outputs
  per canvas.
- Per-canvas studio mode with Preview and Program, Transition and toggle hotkeys, and an optional swap after a
  transition.
- Multiview dock showing every canvas with the outputs it feeds, their uptime, bitrate and dropped frames.
- Output groups that start and stop together from the Outputs dock or a hotkey.
- Copy and paste of properties, transform and filters between sources on any canvas.
- Remote control through the OBS WebSocket server, and an optional link to a Fiji-chan bot.
