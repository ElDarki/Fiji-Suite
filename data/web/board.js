// The deck board: decks as tabs above a tray of keys; a key presses on the way down.
// "?view" in the address hides editing (used by the bot's press-only link).
(() => {
  const { el } = Fiji;
  const viewOnly = new URLSearchParams(location.search).has("view");
  let decks = [];
  let running = new Set();
  let current = "";
  try { current = localStorage.getItem("fiji-deck") || ""; } catch (error) { /* ignore */ }

  function keyNode(button) {
    const node = Fiji.key(button, running.has(button.id) ? "is-active" : "");
    // press on the way down and release on the way up, so buttons with a release list work
    node.addEventListener("pointerdown", (event) => {
      node.setPointerCapture(event.pointerId);
      node.classList.add("pressed");
      Fiji.call("press_button", { button: button.id })
        .then((reply) => { if (reply && reply.success === false && reply.error) Fiji.toast(reply.error, true); })
        .catch((error) => Fiji.toast(error.message, true));
    });
    const up = () => {
      node.classList.remove("pressed");
      if ((button.release || []).length) Fiji.call("release_button", { button: button.id }).catch(() => {});
    };
    node.addEventListener("pointerup", up);
    node.addEventListener("pointercancel", () => node.classList.remove("pressed"));
    // the keyboard presses too
    node.addEventListener("keydown", (event) => {
      if ((event.key === "Enter" || event.key === " ") && !event.repeat) {
        event.preventDefault();
        Fiji.call("press_button", { button: button.id }).catch((error) => Fiji.toast(error.message, true));
      }
    });
    return Fiji.place(node, button.x, button.y, button.w, button.h);
  }

  function render() {
    const deck = decks.find((d) => d.id === current) || decks[0];
    document.getElementById("tabs").replaceChildren(...decks.map((d) => el("button", {
      type: "button",
      role: "tab",
      "aria-selected": deck && d.id === deck.id ? "true" : "false",
      onclick: () => { current = d.id; try { localStorage.setItem("fiji-deck", d.id); } catch (error) { /* ignore */ } render(); },
    }, d.name || "Deck")));
    document.getElementById("actions").replaceChildren(...[
      Fiji.moodButton(),
      deck ? el("button", { type: "button", class: "btn", onclick: () => Fiji.call("stop_button", { button: "all" }) },
        Fiji.icon("stop"), "Stop all") : null,
      viewOnly ? null : el("a", { class: "btn btn-primary", href: "editor.html" + (deck ? "#" + encodeURIComponent(deck.id) : "") },
        Fiji.icon("edit"), "Edit deck"),
    ].filter(Boolean));
    const tray = document.getElementById("deck");
    if (!deck) {
      tray.hidden = true;
      let empty = document.getElementById("empty");
      if (!empty) {
        empty = el("div", { class: "empty", id: "empty" });
        tray.after(empty);
      }
      empty.replaceChildren(...[el("div", {}, "No decks yet. Make one, and your keys appear here."),
        viewOnly ? null : el("a", { class: "btn btn-primary", href: "editor.html" }, Fiji.icon("plus"), "Make a deck")].filter(Boolean));
      return;
    }
    document.getElementById("empty")?.remove();
    tray.hidden = false;
    tray.style.backgroundColor = deck.color || "";
    Fiji.fitKeys(tray, Math.max(1, deck.columns || 5), Math.max(1, deck.rows || 3));
    tray.replaceChildren(...(deck.buttons || []).map(keyNode));
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
      const node = document.querySelector(`.key[data-id="${CSS.escape(data.button)}"]`);
      if (node) node.classList.toggle("is-active", !!data.running);
    } else if (event === "deck_notification") {
      Fiji.toast(data.text);
    }
  });

  load().catch((error) => {
    const tray = document.getElementById("deck");
    tray.hidden = true;
    tray.after(el("div", { class: "empty", id: "empty" }, "OBS isn't answering right now. Check that OBS is open, then reload this page.",
      el("div", { class: "hint" }, error.message)));
  });
})();
