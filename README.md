# Fiji Suite

Extra canvases and multistream outputs for [OBS Studio](https://github.com/obsproject/obs-studio).

## Features

- Extra canvases, each in its own dock, with their own scenes, sources and transitions.
- Per-canvas studio mode: the **Studio Mode** button on a canvas dock splits it into Preview and Program.
  Picking a scene only changes the preview until **Transition** is pressed.
- Stream, record, replay and virtual camera outputs per canvas, managed from the Outputs dock.
- Output groups: right-click an output to put it in a group, then start or stop the whole group from the
  Outputs dock or with a hotkey.
- Multiview dock: every canvas live side by side, each with the outputs it feeds, their uptime, bitrate and
  dropped frames. Right-click it for a fullscreen copy on another display, or to show it in place of the
  main preview.
- Hotkeys per canvas for Transition and for toggling studio mode, and an option to swap preview and program
  after a transition.
- Decks: boards of buttons that run timed steps (scenes, sources, filters, sounds, chat), pressed from an OBS
  dock, a browser, a phone on your network, a hotkey or a bot trigger, with an importer for SAMMI decks. See
  `docs/decks.md`.
- Remote control through the OBS WebSocket server, for bots and stream decks: see `docs/websocket.md`.
- Optional link to a Fiji-chan bot: paste the bot's address in the settings and chat commands, rewards, alerts
  and Fiji-chan herself can see and control OBS. Off while the address is empty.
- Copy and paste between sources on any canvas from the source right-click menu: properties (between sources
  of the same type), transform and filters.
- Settings are under **Tools → Fiji Suite Settings**. The plugin does not rearrange the OBS window.

## Build (Windows)

Needs Visual Studio 2022 Build Tools with the C++ workload and CMake 3.22 or newer. The first configure
downloads the OBS sources and prebuilt dependencies into `.deps`.

```
cmake --preset windows-x64
cmake --build build_x64 --config RelWithDebInfo --parallel
```

The plugin lands in `build_x64/rundir/RelWithDebInfo`.

## Versions

Fiji Suite follows [semantic versioning](https://semver.org): `MAJOR.MINOR.PATCH`.

- **PATCH**: fixes only.
- **MINOR**: new features; saved settings, hotkeys and the WebSocket requests and events keep working.
- **MAJOR**: something that needs action: settings that do not carry over, or a request or event in
  `docs/websocket.md` that is removed or changes meaning.

While the version starts with 0, any release may still change those things.

The number is the `version` in `buildspec.json`; `data/manifest.json` must match it and the build stops if it
does not. To release: change both, add a section to `CHANGELOG.md`, commit, and tag the commit `vX.Y.Z`.

## License and origin

GNU General Public License v2, see `LICENSE`.

Fiji Suite is a modified version of Aitum Stream Suite by Aitum and Exeldro, forked in November 2026 from
commit `8d39111`. The GPL requires this notice of origin and modification to stay with the source. The two
plugins manage the same kind of canvases and outputs and should not be loaded together.
