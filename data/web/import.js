// Converts SAMMI's decks_data.json into Fiji Suite decks. It only reads what it is given.
//
// SAMMI stores each command as a number ("cmd") with parameters b0, b1... (text) and v0, v1...
// (numbers), and does not publish what the numbers mean. The table below covers the commands
// whose meaning is clear from how they are used in real decks. Anything else becomes an
// "unsupported" step that keeps the original data and does nothing, and is listed in the report.
const SammiImport = (() => {
  // SAMMI accepts a bare variable name where a number goes ("randomX"); Fiji wants /$randomX$/.
  const value = (text) => {
    const t = String(text ?? "").trim();
    return /^[A-Za-z_][A-Za-z0-9_.]*$/.test(t) ? `/$${t}$/` : t;
  };
  // The scene SAMMI calls /$global.Main.current_scene$/ is "the current scene": an empty name here.
  const scene = (text) => (/^\/\$global\.[^.$]+\.current_scene\$\/$/.test(String(text ?? "").trim()) ? "" : String(text ?? ""));
  // What SAMMI's "Trigger Pull" names map to among the variables the bot sends with a trigger.
  const PULL = { display_name: "user", user_name: "user", name: "user", amount: "raiders", message: "message", redeem_name: "reward", user_input: "input" };

  const COMMANDS = {
    2: (c) => ({ type: "sound", file: c.b0 || "", volume: String(c.v0 ?? 1), name: c.b1 || "" }),
    6: (c) => ({ type: "comment", text: c.b0 || "" }),
    22: (c) => ({ type: "source_position", scene: scene(c.b0), source: c.b1 || "", x: value(c.b2), y: value(c.b3) }),
    29: (c) => ({ type: "filter_visibility", source: scene(c.b0), filter: c.b1 || "", visible: !!c.v0 }),
    30: (c) => ({ type: "source_visibility", scene: scene(c.b0), source: c.b1 || "", visible: !!c.v0 }),
    32: (c) => ({ type: "source_scale", scene: scene(c.b0), source: c.b1 || "", x: value(c.b2), y: value(c.b3) }),
    33: (c) => ({ type: "source_rotation", scene: scene(c.b0), source: c.b1 || "", rotation: value(c.b2) }),
    35: (c) => ({ type: "source_text", source: c.b0 || "", text: c.b1 || "" }),
    43: (c) => ({ type: "scene", scene: c.b0 || "" }),
    44: (c) => ({ type: "mute", source: c.b0 || "", muted: !!c.v0 }),
    45: (c) => ({ type: "mute", source: c.b0 || "", muted: "toggle" }),
    58: () => ({ type: "request", request: "start_main", data: { output: "stream" } }),
    59: () => ({ type: "request", request: "stop_main", data: { output: "stream" } }),
    66: (c, notes) => {
      const from = PULL[c.b1];
      if (!from) notes.push(`pulls "${c.b1}" from the trigger, which has no match here: check the variable "${c.b0}"`);
      return { type: "set", variable: c.b0 || "", value: `/$${from || c.b1}$/` };
    },
    77: (c) => ({ type: "random", variable: c.b0 || "", min: value(c.b1), max: value(c.b2) }),
    146: (c) => ({ type: "stop_sounds", name: c.b0 === "all" ? "" : c.b0 || "" }),
    148: (c) => ({ type: "chat", text: c.b0 || "" }),
    296: (c) => ({ type: "source_visibility", scene: scene(c.b0), source: c.b1 || "", visible: "toggle" }),
  };
  // Commands whose meaning is a reasoned guess: converted, but called out for checking.
  const CHECK = { 58: "start streaming", 59: "stop streaming", 296: "toggle source visibility", 66: "read a value from the trigger" };

  // GameMaker colours are blue-green-red.
  const colour = (n) => {
    n = Math.max(0, Math.round(Number(n) || 0));
    const hex = (v) => v.toString(16).padStart(2, "0");
    return `#${hex(n & 255)}${hex((n >> 8) & 255)}${hex((n >> 16) & 255)}`;
  };

  function trigger(t, notes) {
    if (t.trg === 0) {
      let message = String(t.message ?? "");
      const starts = message.startsWith("*"), ends = message.endsWith("*");
      const core = message.replace(/^\*+|\*+$/g, "");
      if (core.includes("*")) notes.push(`chat trigger "${message}" has a * in the middle; it was imported as "contains ${core.split("*")[0]}"`);
      const match = !core ? "contains" : starts ? "contains" : ends ? "starts_with" : "exact";
      const permission = t.moderator ? "moderators" : (t.sub || t.founder || t.tier2 || t.tier3 || t.prime) ? "subscribers" : t.vip ? "vips" : "everyone";
      return { type: "chat", message: core.split("*")[0] || "*", match, permission, cooldown: "0" };
    }
    if (t.trg === 3) return { type: "reward", title: t.redeemname || "*" };
    if (t.trg === 4) return { type: "raid", min: String(t.minimum ?? 0) };
    notes.push(`trigger type ${t.trg} is not supported and was left out`);
    return null;
  }

  function button(b, deck, report) {
    const notes = [];
    const columns = Math.max(1, Math.round(deck.grid_x || 8));
    const rows = Math.max(1, Math.round(deck.grid_y || 4));
    // SAMMI runs commands by their time, whatever their order in the list; Fiji runs them in
    // order, each at its time. A stable sort by time keeps both readings the same.
    const commands = (b.command_list || []).map((c, index) => ({ c, index }))
      .sort((a, z) => (a.c.ms || 0) - (z.c.ms || 0) || a.index - z.index).map((x) => x.c);
    const steps = commands.map((c) => {
      const convert = COMMANDS[c.cmd];
      let step;
      if (convert) {
        step = convert(c, notes);
        if (CHECK[c.cmd]) notes.push(`command ${c.cmd} was read as "${CHECK[c.cmd]}": please confirm`);
      } else {
        const kept = Object.fromEntries(Object.entries(c).filter(([k, v]) => !["obsid", "pos", "vis", "sel", "dis", "xpan"].includes(k) && v !== "" && v !== 0));
        step = { type: "unsupported", note: `SAMMI command ${c.cmd}: ${JSON.stringify(kept)}` };
        notes.push(`command ${c.cmd} was not converted`);
      }
      step.at = Math.max(0, Math.round(c.ms || 0));
      return step;
    });
    const triggers = (b.triggers || []).map((t) => trigger(t, notes)).filter(Boolean);
    if ((b.release_list || []).length) notes.push(`${b.release_list.length} release command(s) were left out`);
    const unique = [...new Set(notes)];
    const name = String(b.text || "").replace(/\s+/g, " ").trim() || b.button_id;
    report.push(`${unique.length ? "CHECK" : "ok   "}  ${deck.deck_name} / ${name}: ${steps.length} step(s), ${triggers.length} trigger(s)` +
                unique.map((n) => `\n         - ${n}`).join(""));
    return {
      id: "",
      text: b.text || "",
      color: colour(b.color),
      x: Math.min(columns - 1, Math.round((b.x || 0) * columns)),
      y: Math.min(rows - 1, Math.round((b.y || 0) * rows)),
      w: Math.max(1, Math.round((b.width || 0) * columns)),
      h: Math.max(1, Math.round((b.height || 0) * rows)),
      queueable: !!b.queueable,
      overlappable: !!b.overlappable,
      steps,
      release: [],
      triggers,
    };
  }

  // decks_data.json -> {decks, buttons, report}. Throws when the file is not SAMMI deck data.
  function convert(data) {
    const list = Array.isArray(data) ? data : data && (data.default || data.deck_data);
    if (!Array.isArray(list)) throw new Error("no deck list found");
    const report = [];
    let buttons = 0;
    const decks = list.filter((d) => d && typeof d === "object").map((d) => {
      const converted = (d.button_list || []).map((b) => button(b, d, report));
      buttons += converted.length;
      return {
        id: "",
        name: d.deck_name || "Deck",
        columns: Math.max(1, Math.round(d.grid_x || 8)),
        rows: Math.max(1, Math.round(d.grid_y || 4)),
        color: colour(d.background_color),
        buttons: converted,
      };
    });
    return { decks, buttons, report };
  }

  return { convert };
})();

if (typeof module !== "undefined") module.exports = { SammiImport };
