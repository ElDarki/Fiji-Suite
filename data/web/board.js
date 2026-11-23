// The deck board: tabs for the decks, a grid of buttons, a press on tap.
// "?view" in the address hides the links to the editor (used by the bot's press-only link).
(() => {
  const { el } = Fiji;
  const viewOnly = new URLSearchParams(location.search).has("view");
  let decks = [];
  let running = new Set();
  let current = localStorage.getItem("fiji-deck") || "";

  // Places a button (or an empty cell) on the CSS grid. Cells count from 0.
  function place(node, x, y, w, h) {
    node.style.gridColumn = `${x + 1} / span ${Math.max(1, w || 1)}`;
    node.style.gridRow = `${y + 1} / span ${Math.max(1, h || 1)}`;
    return node;
  }

  function buttonNode(button) {
    const node = el("button", {
      class: "deck-button" + (running.has(button.id) ? " running" : ""),
      "data-id": button.id,
      style: Fiji.buttonStyle(button),
    }, button.text || "");
    // press on the way down and release on the way up, so buttons with a release list work
    node.addEventListener("pointerdown", (event) => {
      node.setPointerCapture(event.pointerId);
      Fiji.call("press_button", { button: button.id }).catch((error) => Fiji.toast(error.message));
    });
    node.addEventListener("pointerup", () => {
      if ((button.release || []).length) Fiji.call("release_button", { button: button.id }).catch(() => {});
    });
    return place(node, button.x || 0, button.y || 0, button.w, button.h);
  }

  function render() {
    const deck = decks.find((d) => d.id === current) || decks[0];
    const tabs = document.getElementById("tabs");
    tabs.replaceChildren(
      ...decks.map((d) => el("button", {
        class: "tab" + (deck && d.id === deck.id ? " current" : ""),
        onclick: () => { current = d.id; localStorage.setItem("fiji-deck", d.id); render(); },
      }, d.name || "Deck")),
      el("span", { class: "spacer" }),
      el("button", { class: "plain danger", onclick: () => Fiji.call("stop_button", { button: "all" }) }, "Stop all"),
      viewOnly ? null : el("a", { class: "plain", href: "editor.html" }, "Edit"),
      Fiji.moodButton(),
    );
    const board = document.getElementById("deck");
    if (!deck) {
      board.style.gridTemplateColumns = board.style.gridTemplateRows = "";
      board.replaceChildren(el("div", { class: "empty" },
        "No decks yet. ", viewOnly ? "" : el("a", { href: "editor.html" }, "Create one in the editor.")));
      return;
    }
    board.style.gridTemplateColumns = `repeat(${deck.columns || 8}, 1fr)`;
    board.style.gridTemplateRows = `repeat(${deck.rows || 4}, 1fr)`;
    board.style.backgroundColor = deck.color || "";
    board.replaceChildren(...(deck.buttons || []).map(buttonNode));
  }

  async function load() {
    const [all, state] = await Promise.all([Fiji.call("get_decks"), Fiji.call("get_deck_state")]);
    decks = all.decks || [];
    running = new Set((state.running || []).map((r) => r.button));
    render();
  }

  Fiji.listen(({ event, data }) => {
    if (event === "decks_changed") {
      load();
    } else if (event === "button_state") {
      data.running ? running.add(data.button) : running.delete(data.button);
      const node = document.querySelector(`.deck-button[data-id="${CSS.escape(data.button)}"]`);
      if (node) node.classList.toggle("running", !!data.running);
    } else if (event === "deck_notification") {
      Fiji.toast(data.text);
    }
  });

  load().catch((error) => {
    document.getElementById("deck").replaceChildren(el("div", { class: "empty" }, "Could not reach OBS: " + error.message));
  });
})();
