# Changelog

Versions follow [semantic versioning](https://semver.org); the README says what each part means here.

## 0.2.0

- Decks: boards of buttons that run timed steps on OBS (scenes, sources, filters, text, mute, sounds), with
  variables, waits and conditions, a hotkey per button, and triggers from the Fiji-chan bot.
- The decks open in a browser, an OBS browser dock, or a phone on the local network, served from inside OBS on
  a secret address; the same pages are available through the bot.
- An editor for decks and an importer for SAMMI decks, which reports anything it could not convert.

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
