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

  toast(text) {
    let box = document.getElementById("toasts");
    if (!box) {
      box = Fiji.el("div", { id: "toasts" });
      document.body.append(box);
    }
    const note = Fiji.el("div", { class: "toast" }, text);
    box.append(note);
    setTimeout(() => note.remove(), 5000);
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
