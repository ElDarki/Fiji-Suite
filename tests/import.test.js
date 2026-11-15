// Tests for the SAMMI importer. Run: node tests/import.test.js   (exit code 0 = all passed)
const assert = require("node:assert/strict");
const { SammiImport } = require("../data/web/import.js");
const { STEP_TYPES, TRIGGER_TYPES } = require("../data/web/steps.js");

// A made-up deck in SAMMI's format, covering every command and trigger the importer knows,
// one it does not, and commands listed out of time order.
const sammi = {
  default: [{
    deck_name: "Rewards", grid_x: 10, grid_y: 6, background_color: 4210752,
    button_list: [
      {
        text: "Freeze", button_id: "ID1", color: 51200, x: 0.2, y: 0.5, width: 0.1, height: 1 / 6, queueable: 1, overlappable: 0,
        command_list: [
          { cmd: 30, ms: 0, b0: "overlays", b1: "clip", v0: 1 },
          { cmd: 29, ms: 800, b0: "/$global.Main.current_scene$/", b1: "Freeze", v0: 1 },
          { cmd: 44, ms: 0, b0: "Mic/Aux", v0: 1 },
          { cmd: 30, ms: 8000, b0: "overlays", b1: "clip", v0: 0 },
        ],
        triggers: [{ trg: 3, redeemname: "Freeze!", message: "*" }],
      },
      {
        text: "Everything", button_id: "ID2", color: 255, x: 0, y: 0, width: 0.2, height: 2 / 6,
        command_list: [
          { cmd: 2, ms: 0, b0: "C:\\sounds\\a.ogg", b1: "s1", v0: 0.5 },
          { cmd: 6, ms: 0, b0: "a note" },
          { cmd: 77, ms: 0, b0: "rx", b1: "10", b2: "20" },
          { cmd: 22, ms: 0, b0: "overlays", b1: "clip", b2: "rx", b3: "40" },
          { cmd: 32, ms: 0, b0: "overlays", b1: "clip", b2: ".5", b3: ".5" },
          { cmd: 33, ms: 0, b0: "overlays", b1: "clip", b2: "90" },
          { cmd: 35, ms: 0, b0: "Label", b1: "Hi /$user$/" },
          { cmd: 43, ms: 0, b0: "Break" },
          { cmd: 45, ms: 0, b0: "Mic/Aux" },
          { cmd: 58, ms: 0 }, { cmd: 59, ms: 0 },
          { cmd: 66, ms: 0, b0: "leader", b1: "display_name" },
          { cmd: 66, ms: 0, b0: "odd", b1: "Value 2" },
          { cmd: 146, ms: 0, b0: "all" },
          { cmd: 148, ms: 0, b0: "hello chat" },
          { cmd: 296, ms: 0, b0: "overlays", b1: "clip" },
          { cmd: 999, ms: 5, b0: "mystery", v0: 3 },
        ],
        triggers: [
          { trg: 0, message: "*hola*" }, { trg: 0, message: "!go*", moderator: 1 }, { trg: 0, message: "exact words" },
          { trg: 4, minimum: 21 }, { trg: 12, message: "timer" },
        ],
        release_list: [{ cmd: 43, b0: "x" }],
      },
    ],
  }],
};

const { decks, buttons, report } = SammiImport.convert(sammi);
assert.equal(decks.length, 1);
assert.equal(buttons, 2);
const [deck] = decks;
assert.deepEqual([deck.name, deck.columns, deck.rows, deck.color], ["Rewards", 10, 6, "#404040"]);

const [freeze, all] = deck.buttons;
// placed on the grid from SAMMI's fractions; GameMaker's blue-green-red colour becomes #rrggbb
assert.deepEqual([freeze.x, freeze.y, freeze.w, freeze.h, freeze.color, freeze.queueable], [2, 3, 1, 1, "#00c800", true]);
assert.deepEqual([all.w, all.h, all.color], [2, 2, "#ff0000"]);
// commands come out in time order, and "the current scene" becomes an empty name
assert.deepEqual(freeze.steps.map((s) => [s.type, s.at]), [["source_visibility", 0], ["mute", 0], ["filter_visibility", 800], ["source_visibility", 8000]]);
assert.deepEqual(freeze.steps[2], { type: "filter_visibility", source: "", filter: "Freeze", visible: true, at: 800 });
assert.deepEqual(freeze.triggers, [{ type: "reward", title: "Freeze!" }]);

const byType = Object.fromEntries(all.steps.map((s) => [s.type + (s.request ? ":" + s.request : ""), s]));
assert.deepEqual(byType.sound, { type: "sound", file: "C:\\sounds\\a.ogg", volume: "0.5", name: "s1", at: 0 });
assert.equal(byType.source_position.x, "/$rx$/");   // a bare variable name gets wrapped
assert.equal(byType.source_position.y, "40");
assert.equal(byType.source_scale.x, ".5");
assert.equal(byType.source_rotation.rotation, "90");
assert.equal(byType.source_text.text, "Hi /$user$/");
assert.equal(byType.scene.scene, "Break");
assert.deepEqual(byType["request:stop_main"].data, { output: "stream" });
assert.equal(byType.stop_sounds.name, "");
assert.equal(byType.chat.text, "hello chat");
assert.equal(byType.comment.text, "a note");
assert.deepEqual(all.steps.filter((s) => s.type === "set").map((s) => s.value), ["/$user$/", "/$Value 2$/"]);
assert.equal(all.steps.filter((s) => s.type === "mute").at(-1).muted, "toggle");
assert.equal(all.steps.filter((s) => s.type === "source_visibility").at(-1).visible, "toggle");
// the unknown command is kept, inert, with its data, and comes last because it is at 5 ms
assert.equal(all.steps.at(-1).type, "unsupported");
assert.match(all.steps.at(-1).note, /999.*mystery/);
assert.deepEqual(all.triggers, [
  { type: "chat", message: "hola", match: "contains", permission: "everyone", cooldown: "0" },
  { type: "chat", message: "!go", match: "starts_with", permission: "moderators", cooldown: "0" },
  { type: "chat", message: "exact words", match: "exact", permission: "everyone", cooldown: "0" },
  { type: "raid", min: "21" },
]);

// every step and trigger the importer writes is a type the editor knows
for (const b of deck.buttons) {
  for (const s of b.steps) assert.ok(STEP_TYPES[s.type], `step type ${s.type}`);
  for (const t of b.triggers) assert.ok(TRIGGER_TYPES[t.type], `trigger type ${t.type}`);
}

assert.match(report[0], /^ok {3}/);
assert.match(report[1], /^CHECK/);
for (const expected of ["command 999 was not converted", "trigger type 12 is not supported", "1 release command",
                        'pulls "Value 2"', 'read as "start streaming"']) {
  assert.ok(report[1].includes(expected), expected);
}
assert.throws(() => SammiImport.convert({ something: "else" }), /no deck list/);
console.log("all importer checks passed");
