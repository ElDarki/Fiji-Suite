// Shared by every deck page. The pages only ever use two addresses next to their own: "api" and
// "events". The plugin's local server and the bot both provide them, so the same files work in
// an OBS dock, on a phone on the local network, and through the bot.
const Fiji = {
  async call(request, data) {
    const response = await fetch("api", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ request, data: data || {} }),
    });
    if (!response.ok) throw new Error(`${request}: HTTP ${response.status}`);
    return response.json();
  },

  // Calls handler({event, data}) for every plugin event. Reconnects by itself.
  listen(handler) {
    const source = new EventSource("events");
    source.onmessage = (message) => {
      try {
        handler(JSON.parse(message.data));
      } catch (error) {
        console.warn("bad event", error);
      }
    };
    return source;
  },

  // el("button", {class: "x", onclick: fn}, "text", child, ...)
  el(tag, attributes, ...children) {
    const node = document.createElement(tag);
    for (const [key, value] of Object.entries(attributes || {})) {
      if (key.startsWith("on")) node.addEventListener(key.slice(2), value);
      else if (key === "class") node.className = value;
      else if (key === "style") node.style.cssText = value;
      else if (value === true) node.setAttribute(key, "");
      else if (value !== false && value != null) node.setAttribute(key, value);
    }
    for (const child of children.flat()) {
      if (child != null) node.append(child.nodeType ? child : document.createTextNode(String(child)));
    }
    return node;
  },

  toast(text, bad) {
    let box = document.getElementById("toasts");
    if (!box) {
      box = Fiji.el("div", { id: "toasts" });
      document.body.append(box);
    }
    const note = Fiji.el("div", { class: "toast" + (bad ? " bad" : ""), role: "status" }, text);
    box.append(note);
    setTimeout(() => note.remove(), 5000);
  },

  // Icons from the bot's set (24 px grid, 2 px round strokes, currentColor). Never Unicode glyphs as icons.
  ICONS: {
    sun: '<circle cx="12" cy="12" r="4.5"/><path d="M12 2v2M12 20v2M2 12h2M20 12h2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/>',
    moon: '<path d="M20 14.5A8.5 8.5 0 0 1 9.5 4a8.5 8.5 0 1 0 10.5 10.5Z"/><path d="M17 3.5v3M15.5 5h3"/>',
    plus: '<path d="M12 5v14M5 12h14"/>',
    edit: '<path d="M4 20h4L19 9l-4-4L4 16Z"/><path d="m13.5 6.5 4 4"/>',
    trash: '<path d="M4 7h16M9 7V4.5h6V7M6.5 7l1 13h9l1-13M10 11v5M14 11v5"/>',
    close: '<path d="M6 6l12 12M18 6 6 18"/>',
    up: '<path d="M12 19V5M6 11l6-6 6 6"/>',
    down: '<path d="M12 5v14M6 13l6 6 6-6"/>',
    play: '<path d="M7 4.5v15l12-7.5Z"/>',
    stop: '<rect x="6" y="6" width="12" height="12" rx="2"/>',
    back: '<path d="M19 12H5M11 6l-6 6 6 6"/>',
    check: '<path d="m5 12.5 4.5 4.5L19 7.5"/>',
  },
  icon(name) {
    const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
    svg.setAttribute("viewBox", "0 0 24 24");
    svg.setAttribute("class", "icon");
    svg.setAttribute("aria-hidden", "true");
    svg.innerHTML = Fiji.ICONS[name] || "";
    return svg;
  },

  // A deck key (the guide's .key). A key without its own colour keeps the page's (Sun or Moon) look.
  key(button, extra) {
    let style = "";
    let cls = "key";
    if (button.color) {
      style += `background-color:${button.color};color:${button.text_color || Fiji.readableOn(button.color)};`;
      cls += " has-color";
    } else if (button.text_color) {
      style += `color:${button.text_color};`;
    }
    if (button.image) {
      style += `background-image:url("${encodeURI(button.image)}");`;
      cls += " has-image";
    }
    const node = Fiji.el("button", { type: "button", class: cls + (extra ? " " + extra : ""), style, "data-id": button.id || "" },
      Fiji.el("span", { class: "kdot", "aria-hidden": "true" }),
      Fiji.el("span", { class: "kl" }, button.text || ""));
    return node;
  },

  // Places a key or a slot on the tray's grid. Cells count from 0.
  place(node, x, y, w, h) {
    node.style.gridColumn = `${(x || 0) + 1} / span ${Math.max(1, w || 1)}`;
    node.style.gridRow = `${(y || 0) + 1} / span ${Math.max(1, h || 1)}`;
    return node;
  },

  // Keys are 96 px squares; in a narrow dock or on a phone they share the width, down to 56 px.
  fitKeys(tray, columns, rows) {
    tray.style.setProperty("--cols", columns);
    tray.style.setProperty("--rows", rows);
    const fit = () => {
      const room = tray.parentElement.clientWidth - parseFloat(getComputedStyle(tray.parentElement).paddingLeft) * 2;
      const style = getComputedStyle(tray);
      const pad = parseFloat(style.paddingLeft) * 2;
      const gap = parseFloat(style.columnGap) || 12;
      const size = Math.floor((room - pad - gap * (columns - 1)) / columns);
      tray.style.setProperty("--key", `${Math.max(56, Math.min(96, size))}px`);
    };
    fit();
    if (!tray._fitObserver && window.ResizeObserver) {
      tray._fitObserver = new ResizeObserver(() => tray._fit && tray._fit());
      tray._fitObserver.observe(tray.parentElement);
    }
    tray._fit = fit;
  },

  // Sun or Moon. "?mood=sun|moon" in the address wins, then the choice made with the mood button, then the OBS
  // theme (dark → Moon). Until one of those is known, the device's light/dark setting decides (style.css).
  mood: null,
  setMood(mood, remember) {
    if (mood !== "sun" && mood !== "moon") return;
    Fiji.mood = mood;
    document.documentElement.dataset.mood = mood;
    if (remember) {
      try { localStorage.setItem("fiji-mood", mood); } catch (error) { /* private window: not remembered */ }
    }
    document.querySelectorAll(".mood-switch").forEach(Fiji.paintMoodButton);
  },
  startMood() {
    const asked = new URLSearchParams(location.search).get("mood");
    let saved = null;
    try { saved = localStorage.getItem("fiji-mood"); } catch (error) { /* ignore */ }
    if (asked) return Fiji.setMood(asked);
    if (saved) return Fiji.setMood(saved);
    Fiji.call("get_look").then((look) => {
      if (!Fiji.mood && look && look.success) Fiji.setMood(look.dark ? "moon" : "sun");
    }).catch(() => {});
  },
  isMoon() {
    return Fiji.mood ? Fiji.mood === "moon" : matchMedia("(prefers-color-scheme: dark)").matches;
  },
  // The sun and moon switch: shows the mood you are in and says what a press does.
  paintMoodButton(button) {
    const moon = Fiji.isMoon();
    button.replaceChildren(Fiji.icon(moon ? "moon" : "sun"));
    const label = moon ? "Moon look. Switch to Sun" : "Sun look. Switch to Moon";
    button.title = label;
    button.setAttribute("aria-label", label);
  },
  moodButton() {
    const button = Fiji.el("button", { class: "btn btn-ghost btn-icon mood-switch", type: "button",
                                       onclick: () => Fiji.setMood(Fiji.isMoon() ? "sun" : "moon", true) });
    Fiji.paintMoodButton(button);
    return button;
  },

  // Black or white, whichever reads better on the given #rrggbb background.
  readableOn(color) {
    const m = /^#?([0-9a-f]{6})$/i.exec(color || "");
    if (!m) return "#ffffff";
    const n = parseInt(m[1], 16);
    const luminance = 0.299 * (n >> 16) + 0.587 * ((n >> 8) & 255) + 0.114 * (n & 255);
    return luminance > 150 ? "#101014" : "#ffffff";
  },
};

Fiji.startMood();
