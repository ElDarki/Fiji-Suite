// The deck editor. It edits a copy of one deck in memory and sends the whole deck with
// "Save deck"; nothing changes in OBS before that.
(() => {
  const { el } = Fiji;
  let decks = [];
  let deck = null;      // the working copy being edited
  let selected = null;  // the selected button object inside `deck`, or null
  let dirty = false;

  const clone = (value) => JSON.parse(JSON.stringify(value));
  const touch = () => { dirty = true; drawTabs(); };

  // ---------------------------------------------------------------- small form pieces
  function row(label, control) {
    return el("div", { class: "row" }, el("label", {}, label), control);
  }

  // An input bound to object[key]. kind: text, long, number, bool, toggle, choice, json, color.
  function field(object, key, kind, options, redraw) {
    const changed = (value) => { object[key] = value; touch(); if (redraw) redraw(); };
    if (kind === "bool") {
      return el("input", { type: "checkbox", checked: !!object[key], onchange: (e) => changed(e.target.checked) });
    }
    if (kind === "toggle") {
      const now = object[key] === true ? "true" : object[key] === false ? "false" : "toggle";
      const select = el("select", { onchange: (e) => changed(e.target.value === "toggle" ? "toggle" : e.target.value === "true") },
        el("option", { value: "true" }, "On"), el("option", { value: "false" }, "Off"), el("option", { value: "toggle" }, "Toggle"));
      select.value = object[key] === undefined ? "true" : now;
      if (object[key] === undefined) object[key] = true;
      return select;
    }
    if (kind === "choice") {
      const select = el("select", { onchange: (e) => changed(e.target.value) }, options.map((o) => el("option", { value: o }, o)));
      if (object[key] === undefined) object[key] = options[0];
      select.value = object[key];
      return select;
    }
    if (kind === "long") {
      return el("textarea", { oninput: (e) => changed(e.target.value) }, object[key] ?? "");
    }
    if (kind === "json") {
      const area = el("textarea", {
        oninput: (e) => {
          try { changed(JSON.parse(e.target.value || "{}")); area.style.borderColor = ""; }
          catch { area.style.borderColor = "#d92c2c"; }
        },
      }, JSON.stringify(object[key] ?? {}, null, 1));
      return area;
    }
    if (kind === "number") {
      return el("input", { type: "number", value: object[key] ?? 0, oninput: (e) => changed(Number(e.target.value)) });
    }
    if (kind === "color") {
      return el("input", { type: "color", value: object[key] || "#5ea79d", oninput: (e) => changed(e.target.value) });
    }
    return el("input", { type: "text", value: object[key] ?? "", oninput: (e) => changed(e.target.value) });
  }

  // A list of steps or triggers: a type picker and that type's fields, with add / remove / move.
  function listEditor(list, types, withTime, redraw) {
    const move = (index, by) => {
      const target = index + by;
      if (target < 0 || target >= list.length) return;
      [list[index], list[target]] = [list[target], list[index]];
      touch(); redraw();
    };
    const nodes = list.map((item, index) => {
      const type = types[item.type] || types.unsupported || { label: item.type, fields: [] };
      const picker = el("select", { onchange: (e) => { list[index] = withTime ? { type: e.target.value, at: item.at || 0 } : { type: e.target.value }; touch(); redraw(); } },
        Object.entries(types).map(([id, t]) => el("option", { value: id }, t.label)));
      picker.value = types[item.type] ? item.type : "unsupported";
      return el("div", { class: "step" + (item.type === "unsupported" ? " unsupported" : "") },
        el("div", { class: "head" },
          withTime ? el("input", { type: "number", min: 0, title: "Milliseconds after the press", value: item.at || 0,
                                   oninput: (e) => { item.at = Number(e.target.value); touch(); } }) : null,
          picker,
          el("button", { class: "plain", title: "Move up", onclick: () => move(index, -1) }, "↑"),
          el("button", { class: "plain", title: "Move down", onclick: () => move(index, 1) }, "↓"),
          el("button", { class: "plain danger", title: "Remove", onclick: () => { list.splice(index, 1); touch(); redraw(); } }, "×")),
        type.fields.map(([key, label, kind, options]) => row(label, field(item, key, kind, options))));
    });
    const first = Object.keys(types)[0];
    nodes.push(el("button", { class: "plain", onclick: () => { list.push(withTime ? { type: first, at: 0 } : { type: first }); touch(); redraw(); } }, "+ Add"));
    return nodes;
  }

  // ---------------------------------------------------------------- the grid
  function place(node, x, y, w, h) {
    node.style.gridColumn = `${x + 1} / span ${Math.max(1, w || 1)}`;
    node.style.gridRow = `${y + 1} / span ${Math.max(1, h || 1)}`;
    return node;
  }

  function drawGrid() {
    const board = document.getElementById("deck");
    if (!deck) {
      board.style.gridTemplateColumns = board.style.gridTemplateRows = "";
      board.replaceChildren(el("div", { class: "empty" }, "Add a deck, or import your SAMMI decks, to start."));
      return;
    }
    const columns = Math.max(1, deck.columns || 8);
    const rows = Math.max(1, deck.rows || 4);
    board.style.gridTemplateColumns = `repeat(${columns}, 1fr)`;
    board.style.gridTemplateRows = `repeat(${rows}, 1fr)`;
    board.style.backgroundColor = deck.color || "";
    const covered = new Set();
    for (const b of deck.buttons) {
      for (let x = b.x; x < b.x + (b.w || 1); x++) for (let y = b.y; y < b.y + (b.h || 1); y++) covered.add(`${x},${y}`);
    }
    const nodes = [];
    for (let y = 0; y < rows; y++) {
      for (let x = 0; x < columns; x++) {
        if (covered.has(`${x},${y}`)) continue;
        nodes.push(place(el("div", { class: "cell", title: "Add a button here", onclick: () => addButton(x, y) }), x, y, 1, 1));
      }
    }
    for (const b of deck.buttons) {
      nodes.push(place(el("button", {
        class: "deck-button" + (b === selected ? " selected" : ""),
        style: Fiji.buttonStyle(b),
        onclick: () => { selected = b; draw(); },
      }, b.text || ""), b.x || 0, b.y || 0, b.w, b.h));
    }
    board.replaceChildren(...nodes);
  }

  function addButton(x, y) {
    selected = { id: "", text: "New", color: "", x, y, w: 1, h: 1, steps: [], release: [], triggers: [] };
    deck.buttons.push(selected);
    touch(); draw();
  }

  // ---------------------------------------------------------------- the side panel
  function drawSide() {
    const side = document.getElementById("side");
    if (!deck) { side.replaceChildren(); return; }
    const parts = [
      el("h3", {}, "Deck"),
      row("Name", field(deck, "name", "text", null, drawTabs)),
      row("Columns", field(deck, "columns", "number", null, drawGrid)),
      row("Rows", field(deck, "rows", "number", null, drawGrid)),
      row("Background", field(deck, "color", "color", null, drawGrid)),
    ];
    if (selected) {
      const b = selected;
      b.steps ||= []; b.release ||= []; b.triggers ||= [];
      parts.push(
        el("h3", {}, "Button"),
        row("Text", field(b, "text", "long", null, drawGrid)),
        row("Colour", field(b, "color", "color", null, drawGrid)),
        row("Column", field(b, "x", "number", null, drawGrid)),
        row("Row", field(b, "y", "number", null, drawGrid)),
        row("Width", field(b, "w", "number", null, drawGrid)),
        row("Height", field(b, "h", "number", null, drawGrid)),
        row("Queue presses", field(b, "queueable", "bool")),
        row("Allow overlap", field(b, "overlappable", "bool")),
        el("div", { class: "hint" }, b.id ? `Id: ${b.id}. A hotkey for this button is in OBS: Settings → Hotkeys.` : "Its id and hotkey are created when the deck is saved."),
        el("div", { class: "row" },
          el("button", { class: "plain", onclick: test }, "Test"),
          el("button", { class: "plain danger", onclick: () => { deck.buttons = deck.buttons.filter((x) => x !== b); selected = null; touch(); draw(); } }, "Delete button")),
        el("h3", {}, "Steps"),
        el("div", { class: "hint" }, "The number is when the step runs, in milliseconds after the press. Use /$name$/ for a variable."),
        listEditor(b.steps, STEP_TYPES, true, drawSide),
        el("h3", {}, "On release"),
        listEditor(b.release, STEP_TYPES, true, drawSide),
        el("h3", {}, "Triggers"),
        el("div", { class: "hint" }, "Triggers come from the bot, so they fire only while OBS is connected to it."),
        listEditor(b.triggers, TRIGGER_TYPES, false, drawSide));
    } else {
      parts.push(el("div", { class: "hint" }, "Click an empty cell to add a button, or a button to edit it."));
    }
    side.replaceChildren(...parts.flat());
  }

  async function test() {
    if (dirty) { Fiji.toast("Save the deck first: the test runs what is saved."); return; }
    const reply = await Fiji.call("press_button", { button: selected.id });
    if (!reply.success) Fiji.toast(reply.error || "It did not start.");
  }

  // ---------------------------------------------------------------- top bar
  function drawTabs() {
    document.getElementById("tabs").replaceChildren(
      ...decks.map((d) => el("button", {
        class: "tab" + (deck && d.id === deck.id ? " current" : ""),
        onclick: () => open(d.id),
      }, (deck && d.id === deck.id ? deck.name : d.name) || "Deck")),
      el("button", { class: "plain", onclick: newDeck }, "+ Deck"),
      el("button", { class: "plain", onclick: () => document.getElementById("sammi-file").click() }, "Import from SAMMI"),
      el("span", { class: "spacer" }),
      deck ? el("button", { class: "plain danger", onclick: removeDeck }, "Delete deck") : null,
      deck ? el("button", { class: "plain" + (dirty ? " primary" : ""), onclick: save }, dirty ? "Save deck *" : "Saved") : null,
      el("a", { class: "plain", href: "./" }, "Back to decks"),
      Fiji.moodButton());
  }

  function draw() { drawTabs(); drawGrid(); drawSide(); }

  function leaveOk() {
    return !dirty || confirm("This deck has changes that are not saved. Discard them?");
  }

  function open(id) {
    if (deck && deck.id === id) return;
    if (!leaveOk()) return;
    const found = decks.find((d) => d.id === id);
    deck = found ? clone(found) : null;
    if (deck) deck.buttons ||= [];
    selected = null; dirty = false;
    draw();
  }

  function newDeck() {
    if (!leaveOk()) return;
    deck = { id: "", name: "New deck", columns: 8, rows: 4, color: "#17171c", buttons: [] };
    selected = null; dirty = true;
    draw();
  }

  async function save() {
    if (!deck) return;
    const reply = await Fiji.call("save_deck", { deck });
    if (!reply.success) { Fiji.toast(reply.error || "Saving failed."); return; }
    const selectedIndex = selected ? deck.buttons.indexOf(selected) : -1;
    deck = reply.deck;
    deck.buttons ||= [];
    selected = selectedIndex >= 0 ? deck.buttons[selectedIndex] : null;
    dirty = false;
    await load(deck.id);
  }

  async function removeDeck() {
    if (!confirm(`Delete the deck "${deck.name}" and all its buttons?`)) return;
    if (deck.id) await Fiji.call("delete_deck", { id: deck.id });
    deck = null; selected = null; dirty = false;
    await load();
  }

  async function load(keep) {
    decks = (await Fiji.call("get_decks")).decks || [];
    if (keep && deck && deck.id === keep) { draw(); return; }
    if (!dirty) {
      const found = decks.find((d) => d.id === (keep || (deck && deck.id))) || decks[0];
      deck = found ? clone(found) : null;
      if (deck) deck.buttons ||= [];
      selected = null;
    }
    draw();
  }

  // ---------------------------------------------------------------- SAMMI import
  document.getElementById("sammi-file").addEventListener("change", async (event) => {
    const file = event.target.files[0];
    event.target.value = "";
    if (!file) return;
    let result;
    try {
      result = SammiImport.convert(JSON.parse(await file.text()));
    } catch (error) {
      Fiji.toast("That file could not be read as SAMMI decks: " + error.message);
      return;
    }
    const side = document.getElementById("side");
    side.replaceChildren(
      el("h3", {}, "Import from SAMMI"),
      el("div", { class: "hint" }, `${result.decks.length} decks, ${result.buttons} buttons. Steps marked "not converted" are kept in the button, do nothing, and are listed below. Your SAMMI file is not changed.`),
      el("div", { class: "report" }, result.report.join("\n")),
      el("div", { class: "row" },
        el("button", { class: "tab current", onclick: async () => {
          for (const imported of result.decks) await Fiji.call("save_deck", { deck: imported });
          dirty = false; deck = null;
          await load();
          Fiji.toast("Imported.");
        } }, "Add these decks"),
        el("button", { class: "plain", onclick: drawSide }, "Cancel")));
  });

  window.addEventListener("beforeunload", (event) => { if (dirty) event.preventDefault(); });
  load().catch((error) => {
    document.getElementById("deck").replaceChildren(el("div", { class: "empty" }, "Could not reach OBS: " + error.message));
  });
})();
