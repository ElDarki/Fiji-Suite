// What a deck button can do, and what can set it off. The editor builds its forms from these
// tables and the SAMMI importer writes steps of these types, so a type added here shows up in both.
//
// field kinds: text, long (multi-line), number, bool, toggle (on / off / toggle), choice (options)
const STEP_TYPES = {
  // carried out by OBS
  scene: { label: "Change scene", fields: [["canvas", "Canvas (empty = main)", "text"], ["scene", "Scene", "text"]] },
  source_visibility: {
    label: "Show / hide source",
    fields: [["canvas", "Canvas (empty = main)", "text"], ["scene", "Scene (empty = current)", "text"],
             ["source", "Source", "text"], ["visible", "Visible", "toggle"]],
  },
  source_position: {
    label: "Move source",
    fields: [["canvas", "Canvas (empty = main)", "text"], ["scene", "Scene (empty = current)", "text"],
             ["source", "Source", "text"], ["x", "X", "text"], ["y", "Y", "text"]],
  },
  source_scale: {
    label: "Scale source",
    fields: [["canvas", "Canvas (empty = main)", "text"], ["scene", "Scene (empty = current)", "text"],
             ["source", "Source", "text"], ["x", "Scale X", "text"], ["y", "Scale Y", "text"]],
  },
  source_rotation: {
    label: "Rotate source",
    fields: [["canvas", "Canvas (empty = main)", "text"], ["scene", "Scene (empty = current)", "text"],
             ["source", "Source", "text"], ["rotation", "Degrees", "text"]],
  },
  filter_visibility: {
    label: "Turn filter on / off",
    fields: [["canvas", "Canvas (empty = main)", "text"], ["source", "Source or scene", "text"],
             ["filter", "Filter", "text"], ["visible", "Enabled", "toggle"]],
  },
  source_text: {
    label: "Set text",
    fields: [["canvas", "Canvas (empty = main)", "text"], ["source", "Text source", "text"], ["text", "Text", "long"]],
  },
  mute: {
    label: "Mute / unmute",
    fields: [["canvas", "Canvas (empty = main)", "text"], ["source", "Audio source", "text"], ["muted", "Muted", "toggle"]],
  },
  sound: {
    label: "Play sound",
    fields: [["file", "File on this computer", "text"], ["volume", "Volume (0 to 1)", "text"],
             ["name", "Name (to stop it later)", "text"]],
  },
  stop_sounds: { label: "Stop sounds", fields: [["name", "Name (empty = all)", "text"]] },
  chat: { label: "Say in chat (needs the bot)", fields: [["text", "Message", "long"]] },
  notify: { label: "Show a note on the deck", fields: [["text", "Text", "long"]] },
  request: {
    label: "Plugin request",
    fields: [["request", "Request (see docs/websocket.md)", "text"], ["data", "Data (JSON)", "json"]],
  },
  // handled by the engine
  wait: { label: "Wait", fields: [["ms", "Milliseconds", "text"]] },
  wait_until: {
    label: "Wait until",
    fields: [["variable", "Variable", "text"], ["op", "Is", "choice", ["==", "!=", "<", ">", "<=", ">=", "contains", "starts_with"]],
             ["value", "Value", "text"], ["timeout", "Give up after (ms, 0 = never)", "text"]],
  },
  set: { label: "Set variable", fields: [["variable", "Variable (global.name is shared)", "text"], ["value", "Value", "text"]] },
  random: { label: "Random number", fields: [["variable", "Variable", "text"], ["min", "From", "text"], ["max", "To", "text"]] },
  math: {
    label: "Calculate",
    fields: [["variable", "Variable", "text"], ["left", "Left", "text"], ["op", "Operation", "choice", ["+", "-", "*", "/"]],
             ["right", "Right", "text"]],
  },
  if: {
    label: "If",
    fields: [["left", "Left", "text"], ["op", "Is", "choice", ["==", "!=", "<", ">", "<=", ">=", "contains", "starts_with"]],
             ["right", "Right", "text"]],
  },
  else: { label: "Else", fields: [] },
  end: { label: "End if", fields: [] },
  stop: { label: "Stop this button", fields: [] },
  press: { label: "Press another button", fields: [["button", "Button id", "text"]] },
  stop_button: { label: "Stop another button", fields: [["button", "Button id", "text"]] },
  comment: { label: "Comment (does nothing)", fields: [["text", "Note", "long"]] },
  // left by the importer for commands it could not convert; never runs
  unsupported: { label: "Not converted (does nothing)", fields: [["note", "What it was", "long"]] },
};

// Triggers are matched by the bot, so they only fire while OBS is connected to it. The variables
// each one hands to the button are listed in docs/decks.md.
const TRIGGER_TYPES = {
  chat: {
    label: "Chat message",
    fields: [["message", "Text (* matches anything)", "text"],
             ["match", "Match", "choice", ["exact", "starts_with", "contains"]],
             ["permission", "Who", "choice", ["everyone", "subscribers", "vips", "moderators", "broadcaster"]],
             ["cooldown", "Cooldown (seconds)", "text"]],
  },
  reward: { label: "Channel point reward", fields: [["title", "Reward title (* = any)", "text"]] },
  raid: { label: "Raid", fields: [["min", "At least this many raiders", "text"]] },
  sub: { label: "Subscription", fields: [] },
  bits: { label: "Bits", fields: [["min", "At least", "text"]] },
};

if (typeof module !== "undefined") module.exports = { STEP_TYPES, TRIGGER_TYPES };
