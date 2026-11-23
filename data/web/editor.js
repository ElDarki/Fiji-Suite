// The deck editor. It edits a copy of one deck in memory and sends the whole deck with
// "Save deck"; nothing changes in OBS before that.
(() => {
  const { el } = Fiji;
  let decks = [];
  let deck = null;      // the working copy being edited
  let selected = null;  // the selected button object inside `deck`, or null
  let dirty = false;
  let linked = null;     // the bot link: true, false, or null when this page can't tell (opened through the bot)

  const clone = (value) => JSON.parse(JSON.stringify(value));
  const touch = () => { dirty = true; drawSaveBar(); };

  // ---------------------------------------------------------------- small form pieces
  // A visible label above every field; a checkbox has its label beside it.
  let fieldIds = 0;
  function row(label, control) {
    if (control.type === "checkbox") return el("label", { class: "check" }, control, label);
    const id = control.id || (control.id = `f${++fieldIds}`);
    return el("div", { class: "field" }, el("label", { for: id }, label), control);
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
          try { changed(JSON.parse(e.target.value || "{}")); area.classList.remove("is-invalid"); }
          catch { area.classList.add("is-invalid"); }
        },
      }, JSON.stringify(object[key] ?? {}, null, 1));
      return area;
    }
    if (kind === "number") {
      // a key's width and height count from 1 when the deck never set them
      const fallback = key === "w" || key === "h" ? 1 : 0;
      return el("input", { type: "number", value: object[key] ?? fallback, oninput: (e) => changed(Number(e.target.value)) });
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
          el("button", { type: "button", class: "btn btn-ghost btn-icon btn-sm", title: "Move up", "aria-label": "Move up", onclick: () => move(index, -1) }, Fiji.icon("up")),
          el("button", { type: "button", class: "btn btn-ghost btn-icon btn-sm", title: "Move down", "aria-label": "Move down", onclick: () => move(index, 1) }, Fiji.icon("down")),
          el("button", { type: "button", class: "btn btn-ghost btn-icon btn-sm", title: "Remove", "aria-label": "Remove", onclick: () => { list.splice(index, 1); touch(); redraw(); } }, Fiji.icon("close"))),
        type.fields.map(([key, label, kind, options]) => row(label, field(item, key, kind, options))));
    });
    const first = Object.keys(types)[0];
    nodes.push(el("button", { type: "button", class: "btn btn-sm", onclick: () => { list.push(withTime ? { type: first, at: 0 } : { type: first }); touch(); redraw(); } }, Fiji.icon("plus"), "Add"));
    return nodes;
  }

  // ---------------------------------------------------------------- the tray
  function drawGrid() {
    const tray = document.getElementById("deck");
    if (!deck) {
      tray.hidden = true;
      let empty = document.getElementById("empty");
      if (!empty) { empty = el("div", { class: "empty", id: "empty" }); tray.after(empty); }
      empty.replaceChildren(el("div", {}, "No decks yet. Make one, or bring your SAMMI decks over."),
        el("div", { class: "panel-actions" },
          el("button", { type: "button", class: "btn btn-primary", onclick: newDeck }, Fiji.icon("plus"), "Make a deck"),
          el("button", { type: "button", class: "btn", onclick: () => document.getElementById("sammi-file").click() }, "Import from SAMMI")));
      return;
    }
    document.getElementById("empty")?.remove();
    tray.hidden = false;
    const columns = Math.max(1, deck.columns || 5);
    const rows = Math.max(1, deck.rows || 3);
    Fiji.fitKeys(tray, columns, rows);
    tray.style.backgroundColor = deck.color || "";
    const covered = new Set();
    for (const b of deck.buttons) {
      for (let x = b.x; x < b.x + (b.w || 1); x++) for (let y = b.y; y < b.y + (b.h || 1); y++) covered.add(`${x},${y}`);
    }
    const nodes = [];
    for (let y = 0; y < rows; y++) {
      for (let x = 0; x < columns; x++) {
        if (covered.has(`${x},${y}`)) continue;
        nodes.push(Fiji.place(el("button", { type: "button", class: "key is-empty", "aria-label": `Add a key in column ${x + 1}, row ${y + 1}`,
                                             onclick: () => addButton(x, y) }, Fiji.icon("plus")), x, y, 1, 1));
      }
    }
    for (const b of deck.buttons) {
      const node = Fiji.key(b, b === selected ? "is-selected" : "");
      node.setAttribute("aria-pressed", b === selected ? "true" : "false");
      node.addEventListener("click", () => { selected = b; draw(); });
      nodes.push(Fiji.place(node, b.x, b.y, b.w, b.h));
    }
    tray.replaceChildren(...nodes);
  }

  function addButton(x, y) {
    selected = { id: "", text: "New key", color: "", x, y, w: 1, h: 1, steps: [], release: [], triggers: [] };
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
      el("div", { class: "field-row" },
        row("Columns", field(deck, "columns", "number", null, drawGrid)),
        row("Rows", field(deck, "rows", "number", null, drawGrid)),
        row("Tray colour", field(deck, "color", "color", null, drawGrid))),
    ];
    if (selected) {
      const b = selected;
      b.steps ||= []; b.release ||= []; b.triggers ||= [];
      parts.push(
        el("h3", {}, "Key"),
        row("Label", field(b, "text", "long", null, drawGrid)),
        el("div", { class: "field-row" },
          row("Colour", field(b, "color", "color", null, drawGrid)),
          row("Column", field(b, "x", "number", null, drawGrid)),
          row("Row", field(b, "y", "number", null, drawGrid))),
        el("div", { class: "field-row" },
          row("Width", field(b, "w", "number", null, drawGrid)),
          row("Height", field(b, "h", "number", null, drawGrid))),
        row("Queue presses", field(b, "queueable", "bool")),
        row("Allow overlap", field(b, "overlappable", "bool")),
        el("div", { class: "hint" }, b.id ? `Id: ${b.id}. A hotkey for this key is in OBS: Settings → Hotkeys.` : "Its id and hotkey are made when the deck is saved."),
        el("div", { class: "panel-actions" },
          el("button", { type: "button", class: "btn", onclick: test }, Fiji.icon("play"), "Test"),
          el("button", { type: "button", class: "btn btn-danger", onclick: () => { deck.buttons = deck.buttons.filter((x) => x !== b); selected = null; touch(); draw(); } }, Fiji.icon("trash"), "Delete key")),
        el("h3", {}, "Steps"),
        el("div", { class: "hint" }, "The number is when the step runs, in milliseconds after the press. Use /$name$/ for a variable."),
        listEditor(b.steps, STEP_TYPES, true, drawSide),
        el("h3", {}, "On release"),
        listEditor(b.release, STEP_TYPES, true, drawSide),
        el("h3", {}, "Triggers"),
        el("div", { class: "hint" }, linked === true ? "Linked to Fiji-chan: these triggers fire now."
          : linked === false ? "Not linked to Fiji-chan right now. Triggers and Say in chat steps are saved and work once OBS is linked (Tools → Fiji Suite Settings → General)."
          : "Triggers come from the bot, so they fire only while OBS is connected to it."),
        listEditor(b.triggers, TRIGGER_TYPES, false, drawSide));
    } else {
      parts.push(el("div", { class: "hint" }, "Pick a key to change it, or a dashed slot to add one."));
    }
    side.replaceChildren(...parts.flat());
  }

  async function test() {
    if (dirty) { Fiji.toast("Save the deck first: the test runs what is saved.", true); return; }
    const reply = await Fiji.call("press_button", { button: selected.id });
    if (!reply.success) Fiji.toast(reply.error || "It did not start.", true);
  }

  // ---------------------------------------------------------------- head and save bar
  function drawTabs() {
    document.getElementById("tabs").replaceChildren(...[
      ...decks.map((d) => el("button", {
        type: "button",
        "aria-selected": deck && d.id === deck.id ? "true" : "false",
        onclick: () => open(d.id),
      }, (deck && d.id === deck.id ? deck.name : d.name) || "Deck")),
      deck && !deck.id ? el("button", { type: "button", "aria-selected": "true" }, deck.name || "New deck") : null,
      el("button", { type: "button", class: "add", onclick: newDeck }, Fiji.icon("plus"), "Deck")].filter(Boolean));
    document.getElementById("actions").replaceChildren(...[
      Fiji.moodButton(),
      el("button", { type: "button", class: "btn btn-ghost", onclick: () => document.getElementById("sammi-file").click() }, "Import from SAMMI"),
      deck ? el("button", { type: "button", class: "btn btn-danger", onclick: removeDeck }, Fiji.icon("trash"), "Delete deck") : null,
      el("a", { class: "btn", href: "./", onclick: (e) => { if (!leaveOk()) e.preventDefault(); } }, Fiji.icon("back"), "Done")].filter(Boolean));
    drawSaveBar();
  }

  function drawSaveBar() {
    const bar = document.getElementById("savebar");
    bar.classList.toggle("is-dirty", dirty);
    bar.hidden = !deck;
    bar.replaceChildren(
      el("span", { class: "note", role: "status" }, dirty ? "This deck has changes that are not saved." : "All changes saved."),
      el("button", { type: "button", class: "btn btn-primary", disabled: !dirty, onclick: save }, Fiji.icon("check"), "Save deck"));
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
    deck = { id: "", name: "New deck", columns: 5, rows: 3, color: "", buttons: [] };
    selected = null; dirty = true;
    draw();
  }

  async function save() {
    if (!deck) return;
    const reply = await Fiji.call("save_deck", { deck });
    if (!reply.success) { Fiji.toast(reply.error || "Saving didn't work. Try again.", true); return; }
    const selectedIndex = selected ? deck.buttons.indexOf(selected) : -1;
    deck = reply.deck;
    deck.buttons ||= [];
    selected = selectedIndex >= 0 ? deck.buttons[selectedIndex] : null;
    dirty = false;
    await load(deck.id);
    Fiji.toast("Saved.");
  }

  async function removeDeck() {
    if (!confirm(`Delete the deck "${deck.name}" and all its buttons?`)) return;
    if (deck.id) await Fiji.call("delete_deck", { id: deck.id });
    deck = null; selected = null; dirty = false;
    await load();
  }

  async function load(keep) {
    decks = (await Fiji.call("get_decks")).decks || [];
    if (keep === undefined && !deck && location.hash) keep = decodeURIComponent(location.hash.slice(1));
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
      Fiji.toast("That file isn't a SAMMI deck file: " + error.message, true);
      return;
    }
    const side = document.getElementById("side");
    side.replaceChildren(
      el("h3", {}, "Import from SAMMI"),
      el("div", { class: "hint" }, `${result.decks.length} decks, ${result.buttons} buttons. Steps marked "not converted" are kept in the button, do nothing, and are listed below. Your SAMMI file is not changed.`),
      el("div", { class: "report" }, result.report.join("\n")),
      el("div", { class: "panel-actions" },
        el("button", { type: "button", class: "btn btn-primary", onclick: async () => {
          for (const imported of result.decks) await Fiji.call("save_deck", { deck: imported });
          dirty = false; deck = null;
          await load();
          Fiji.toast("Imported.");
        } }, Fiji.icon("plus"), "Add these decks"),
        el("button", { type: "button", class: "btn btn-ghost", onclick: drawSide }, "Cancel")));
  });

  window.addEventListener("beforeunload", (event) => { if (dirty) event.preventDefault(); });
  // Only the local deck server knows get_link; through the bot the request fails and the plain hint stays.
  Fiji.call("get_link").then((r) => { linked = r.success ? !!r.linked : null; drawSide(); }).catch(() => {});
  load().catch((error) => {
    const tray = document.getElementById("deck");
    tray.hidden = true;
    tray.after(el("div", { class: "empty", id: "empty" }, "OBS isn't answering right now. Check that OBS is open, then reload this page.",
      el("div", { class: "hint" }, error.message)));
  });
})();
