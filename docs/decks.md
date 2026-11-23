# Decks

A deck is a board of buttons. A button runs a list of timed steps when it is pressed, by a click, a hotkey,
or a trigger from the Fiji-chan bot. Decks live in the plugin and work with no bot and no internet.

## Opening the decks

- **Tools → Decks** opens the board in your browser. The address is also in **Tools → Fiji Suite Settings →
  General → Decks address**.
- **As an OBS dock**: Docks → Custom Browser Docks, and paste that address.
- **On a phone or tablet**: tick "Let phones and tablets on my network open the decks" in the settings and
  open the address shown there on the device. It must be on the same network.
- The address contains a secret. **Reset** makes a new one and the old address stops working.

**Edit deck** on the board opens the editor on that deck. Pick a key to change it in the panel on the right, or a
dashed slot to add one. A deck is changed only when you press **Save deck** in the bar at the bottom of the panel,
which wakes up as soon as something is not saved.

The pages come in Fiji-chan's two looks: **Sun** and **Moon**. They start in Moon when OBS uses a dark theme
and in Sun otherwise. The ☀/☾ button switches and remembers the choice on that device, and `?mood=sun` or
`?mood=moon` at the end of the address fixes the look (handy for a browser dock). A button without its own
colour takes the look's colours.

## Buttons and steps

Each step has a time in milliseconds after the press, and steps run in list order, each once its time has
come. Text may contain `/$name$/`, which is replaced by a variable: one the button set, one that came with
the trigger, or `/$global.name$/` for a variable shared by all buttons.

| Step | What it does |
|---|---|
| Change scene | On the main canvas, or a Fiji Suite canvas by name. |
| Show / hide source, Move, Scale, Rotate | A source inside a scene. An empty scene means the one the canvas is showing. `# 21` as the source means the scene's item number 21. |
| Turn filter on / off | A filter of a source. An empty source means the current scene. |
| Set text | Changes the text of a text source. |
| Mute / unmute | An audio source, including OBS's global audio devices. |
| Play sound, Stop sounds | A sound file on this computer, played inside OBS. |
| Say in chat | Sent through the bot; does nothing without it. |
| Show a note on the deck | A short message on open deck pages. |
| Plugin request | Any request from `docs/websocket.md`, for example starting an output group. |
| Wait, Wait until | Push the later steps back, or hold until a variable has a value. |
| Set variable, Random number, Calculate | Variables. A name starting with `global.` is shared. |
| If / Else / End if | Run steps only when a comparison is true. |
| Stop this button, Press / Stop another button | Control of buttons. |

"Queue presses" makes a second press wait for the first to finish; "Allow overlap" runs both at once. With
neither, a press while the button is running is ignored.

Sounds are mixed into the stream like any audio source. They are also sent to your monitoring device; if you
hear them twice on stream, set `"sound_monitoring": false` in `decks.json`.

## Triggers

Triggers are matched by the Fiji-chan bot, so they fire only while OBS is connected to it.

| Trigger | Variables the button receives |
|---|---|
| Chat message | `user`, `message` |
| Channel point reward | `user`, `reward`, `input` |
| Raid | `user`, `raiders` |
| Subscription | `user`, `message` |
| Bits | `user`, `bits`, `message` |

## Importing from SAMMI

In the editor, **Import from SAMMI** reads a `decks_data.json` (in SAMMI's `json` folder) and shows a report
before anything is added. Your SAMMI files are never changed. Commands the importer does not know are kept
in the button as "Not converted" steps that do nothing, with the original data, so nothing is silently lost.
Lines marked CHECK in the report need a look.

## Where things are kept

`decks.json` in OBS's plugin config folder (`%APPDATA%\obs-studio\plugin_config\fiji-suite\`), with the
previous version beside it as `decks.json.bak`. Hotkeys set for buttons in OBS's settings are saved in it too.
