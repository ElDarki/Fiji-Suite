# Changelog

Versions follow [semantic versioning](https://semver.org); the README says what each part means here.

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
