# Fiji Suite

Extra canvases, multistreaming and button decks for [OBS Studio](https://obsproject.com).

[Leer en español](README.es.md)

Fiji Suite lets one OBS run several canvases side by side, for example a horizontal and a vertical version of
the same stream. Each canvas has its own scenes, its own studio mode and its own stream, recording, replay
and virtual camera outputs. It also adds decks: boards of buttons that run timed steps on OBS, which you can
press from OBS, a browser or your phone.

> **Beta.** Fiji Suite is at version 0.x and may still have bugs. Export your scene collection before you try
> it (**Scene Collection → Export**), and please report anything that goes wrong.

## Features

**Canvases**
- Extra canvases, each in its own dock, with their own scenes, sources and transitions.
- Studio mode per canvas: the **Studio Mode** button on a canvas dock splits it into Preview and Program.
  Picking a scene only changes the preview until you press **Transition**.
- Hotkeys per canvas for Transition and for turning studio mode on and off, and an option to swap preview
  and program after a transition.

**Outputs and multistreaming**
- Stream, record, replay buffer and virtual camera outputs per canvas, managed from the **Outputs** dock.
  Stream to Twitch, YouTube, TikTok, Kick and others at the same time.
- Output groups: right-click an output to put it in a group, then start or stop the whole group at once from
  the Outputs dock or with a hotkey.

**Multiview**
- A dock that shows every canvas live, side by side, with the outputs each one feeds and their uptime,
  bitrate and dropped frames.
- Right-click it to open a fullscreen copy on another display, or to show it in place of the main preview.

**Decks**
- Boards of buttons. Each button runs timed steps: switch scenes, show, hide or move sources, toggle filters,
  change text, mute audio, play sounds, with variables, waits and conditions.
- Press buttons from an OBS dock, any browser, a phone on the same network, a hotkey or a bot.
- An editor for decks, and an importer for SAMMI decks that lists anything it could not convert.
- Details: [docs/decks.md](docs/decks.md).

**Other tools**
- Copy and paste between sources on any canvas from the source's right-click menu: properties (between
  sources of the same type), transform and filters.
- Remote control through the OBS WebSocket server, for bots and stream decks:
  [docs/websocket.md](docs/websocket.md).
- Optional link to the Fiji-chan Twitch bot. It is off while no bot address is set, and everything else works
  without it.

Fiji Suite adds its own docks and menu items. It does not rearrange your OBS window.

## Requirements

- OBS Studio 31.1 or newer.
- Windows 10 or 11 (64-bit), or Linux (Flatpak OBS, or OBS from the official PPA on Ubuntu 24.04).
- macOS is not supported yet.

## Install

Download the file for your system from the
[latest release](https://github.com/ElDarki/Fiji-Suite/releases/latest) or from the Fiji Suite page on the
OBS forums. Close OBS before you install.

### Windows

1. Unzip `fiji-suite-VERSION-windows-x64.zip` into `C:\ProgramData\obs-studio\plugins\`.
2. Check that this file now exists: `C:\ProgramData\obs-studio\plugins\fiji-suite\bin\64bit\fiji-suite.dll`.
3. Start OBS.

Windows or your antivirus may warn that the file is not signed. Beta builds are not signed.

### Linux, Flatpak OBS

For the OBS from Flathub (`com.obsproject.Studio`), on any distribution:

```
flatpak install --user fiji-suite-VERSION-linux-flatpak.flatpak
```

If OBS is installed system-wide and that fails, run the same command with `sudo` and without `--user`.

### Linux, OBS without Flatpak

Built on Ubuntu 24.04 against the OBS from the official PPA. It may not load on other distributions; use
Flatpak OBS there.

```
mkdir -p ~/.config/obs-studio/plugins
tar -xzf fiji-suite-VERSION-linux-x86_64.tar.gz -C ~/.config/obs-studio/plugins
```

### Updating

Close OBS and install the new version the same way, over the old one. Your canvases, outputs and decks are
kept.

### Uninstalling

- Windows: delete `C:\ProgramData\obs-studio\plugins\fiji-suite`.
- Flatpak: `flatpak uninstall com.obsproject.Studio.Plugin.FijiSuite`.
- Linux without Flatpak: delete `~/.config/obs-studio/plugins/fiji-suite`.

## Getting started

Everything is in the OBS **Tools** menu:

- **Add Canvas** creates a new canvas and its dock.
- **Fiji Suite Settings** has the canvases, outputs, hotkeys and the bot link.
- **Multiview** opens the multiview dock; **Multiview Over Main Preview** shows it in place of the main
  preview.
- **Decks** opens the deck pages.

Docks can be shown or hidden from the **Docks** menu like any other OBS dock. The plugin's interface is in
English for now.

## Important

- **Do not load Fiji Suite together with Aitum Stream Suite.** Both manage the same kind of canvases and
  outputs and will conflict. Remove or disable one of them.
- Back up your scene collection before trying a new version while Fiji Suite is in beta.

## Reporting problems

Post a bug report in the discussion of the Fiji Suite page on the OBS forums, or open an
[issue on GitHub](https://github.com/ElDarki/Fiji-Suite/issues). Please include:

- Your OBS log: **Help → Log Files → Upload Current Log File**, then paste the link.
- If OBS crashed: the crash report from **Help → Crash Reports**.
- What you were doing when it happened, and your Fiji Suite and OBS versions.

## Build from source

### Windows

Needs Visual Studio 2022 Build Tools with the C++ workload and CMake 3.22 or newer. The first configure
downloads the OBS sources and prebuilt dependencies into `.deps`.

```
cmake --preset windows-x64
cmake --build build_x64 --config RelWithDebInfo --parallel
```

The plugin lands in `build_x64/rundir/RelWithDebInfo`.

### Flatpak

See [build-aux/flatpak/README.md](build-aux/flatpak/README.md).

## Versions

Fiji Suite follows [semantic versioning](https://semver.org): `MAJOR.MINOR.PATCH`. What changed in each
version is in [CHANGELOG.md](CHANGELOG.md).

- **PATCH**: fixes only.
- **MINOR**: new features; saved settings, hotkeys and the WebSocket requests and events keep working.
- **MAJOR**: something that needs action: settings that do not carry over, or a request or event in
  `docs/websocket.md` that is removed or changes meaning.

While the version starts with 0, any release may still change those things.

For maintainers: the number is the `version` in `buildspec.json`; `data/manifest.json` must match it and the
build stops if it does not. To release, change both, update the version and date in
`build-aux/flatpak/com.obsproject.Studio.Plugin.FijiSuite.metainfo.xml`, add a section to `CHANGELOG.md`,
commit, and tag the commit `vX.Y.Z`.

## Author

Made by darkzeromex.

- Twitch: [twitch.tv/darkzeromex](https://www.twitch.tv/darkzeromex)
- X: [x.com/darkzeromex](https://x.com/darkzeromex)
- Source: [github.com/ElDarki/Fiji-Suite](https://github.com/ElDarki/Fiji-Suite)

## License and origin

GNU General Public License v2, see [LICENSE](LICENSE).

Fiji Suite is a modified version of Aitum Stream Suite by Aitum and Exeldro, forked in November 2026 from
commit `8d39111`. The GPL requires this notice of origin and modification to stay with the source.
