#!/usr/bin/env python3
"""MVC + State guardrails for Fiji Suite (CLAUDE.md, "Architecture"). Exit code 0 means every rule holds.

    python3 tools/check-architecture.py

Checks
- the include rules of each layer under src/ (model, state, controller, bridge, obs, view);
- that only src/obs/ includes libobs among the new layers, and views never call obs_* functions;
- that the legacy folders (code from before this layout) only shrink, and no unknown folder appears in src/;
- BRIDGE_CONTRACT.md: its catalogue block parses, src/bridge/envelope.hpp matches its version and message types,
  src/bridge/catalogue.cpp embeds the same block,
  and the bot repo's copy (../twitchbot) is byte-identical when it is checked out next to this one.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"

LAYERS = ("model", "state", "controller", "bridge", "obs", "view")

# Code from before the MVC + State layout. It moves into the layers one piece at a time; remove an entry when it
# is gone. Never add one: new code goes into a layer.
LEGACY = {
    "src/component-source.c",
    "src/fiji-suite.cpp",
    "src/version.h.in",
    "src/decks",
    "src/dialogs",
    "src/docks",
    "src/utils",
}

# Files the build writes into src/ (CMake's configure_file); never committed (.gitignore).
GENERATED = {"version.h"}

LIBOBS = re.compile(r'#\s*include\s*[<"](obs[^>"]*\.h|util/[^>"]+|graphics/[^>"]+|callback/[^>"]+|media-io/[^>"]+)[>"]')
WIDGETS = re.compile(r'#\s*include\s*[<"](QtWidgets/[^>"]*|Q(Widget|Dialog|DockWidget|MainWindow|Label|PushButton|'
                     r'Layout|HBoxLayout|VBoxLayout|GridLayout|Menu|Action|ToolButton|LineEdit|ComboBox|CheckBox|'
                     r'MessageBox|Application)\b[^>"]*)[>"]')
OBS_CALL = re.compile(r"\bobs_[a-z0-9_]+\s*\(")
LOCAL_INCLUDE = re.compile(r'#\s*include\s*"([^"]+)"')

# Which other layers each layer may include (by relative path). The layer itself is always allowed.
MAY_INCLUDE = {
    "model": set(),
    "state": set(),
    "obs": {"model"},                      # ObsDataStore implements model::LocalStore
    "controller": {"model", "state", "obs"},
    "bridge": {"controller", "model", "state"},
    "view": {"model", "state", "controller"},
}


def layer_of(path: Path) -> str | None:
    rel = path.relative_to(SRC).parts
    return rel[0] if rel and rel[0] in LAYERS else None


def check_layers(errors: list[str]) -> None:
    for path in sorted(SRC.rglob("*")):
        if path.suffix not in (".hpp", ".h", ".cpp", ".c") or not path.is_file():
            continue
        layer = layer_of(path)
        if layer is None:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        rel = path.relative_to(ROOT)
        lines = text.splitlines()

        def where(match) -> str:
            return f"{rel}:{text.count(chr(10), 0, match.start()) + 1}"

        for m in LIBOBS.finditer(text):
            # Within the layers only the obs adapter's implementation files may include libobs.
            if not (layer == "obs" and path.suffix in (".cpp", ".c")):
                errors.append(f"{where(m)} includes libobs ({m.group(1)}); only src/obs/*.cpp may")
        if layer != "view":
            for m in WIDGETS.finditer(text):
                errors.append(f"{where(m)} includes Qt Widgets ({m.group(1)}); only the view layer may")
        if layer in ("view", "model", "state", "bridge"):
            for m in OBS_CALL.finditer(text):
                line = lines[text.count("\n", 0, m.start())].strip()
                if not line.startswith("//"):
                    errors.append(f"{where(m)} calls {m.group(0).rstrip('(').strip()}; go through a controller/ObsApi")
        for m in LOCAL_INCLUDE.finditer(text):
            target = (path.parent / m.group(1)).resolve()
            try:
                target_rel = target.relative_to(SRC)
            except ValueError:
                continue  # outside src/ (e.g. generated version.h)
            other = target_rel.parts[0] if target_rel.parts else ""
            if other == layer:
                continue
            if other in LAYERS:
                if other not in MAY_INCLUDE[layer]:
                    errors.append(f"{where(m)} ({layer}) includes the {other} layer ({m.group(1)})")
            else:
                errors.append(f"{where(m)} ({layer}) includes legacy code ({m.group(1)}); wrap it behind a port")


def check_legacy(errors: list[str]) -> None:
    for entry in sorted(LEGACY):
        if not (ROOT / entry).exists():
            errors.append(f"{entry} no longer exists: remove it from LEGACY in tools/check-architecture.py")
    known = set(LAYERS) | {Path(e).name for e in LEGACY} | GENERATED
    for child in sorted(SRC.iterdir()):
        if child.name not in known:
            errors.append(f"src/{child.name} is neither a layer ({', '.join(LAYERS)}) nor legacy; put new code in a layer")


def check_contract(errors: list[str]) -> None:
    contract_path = ROOT / "BRIDGE_CONTRACT.md"
    text = contract_path.read_text(encoding="utf-8")
    blocks = re.findall(r"```json bridge-contract\n(.*?)\n```", text, re.S)
    if len(blocks) != 1:
        errors.append("BRIDGE_CONTRACT.md needs exactly one ```json bridge-contract block")
        return
    try:
        contract = json.loads(blocks[0])
    except json.JSONDecodeError as e:
        errors.append(f"BRIDGE_CONTRACT.md catalogue is not valid JSON: {e}")
        return
    envelope = (SRC / "bridge" / "envelope.hpp").read_text(encoding="utf-8")
    version = re.search(r'kContractVersion\s*=\s*"([^"]+)"', envelope)
    if not version or version.group(1) != contract["version"]:
        errors.append(f"src/bridge/envelope.hpp kContractVersion must be \"{contract['version']}\"")
    types = re.search(r"enum class MessageType\s*\{([^}]*)\}", envelope)
    if not types or [t.strip() for t in types.group(1).split(",") if t.strip()] != contract["types"]:
        errors.append(f"src/bridge/envelope.hpp MessageType must list {contract['types']} in that order")
    embedded = re.search(r'R"json\(\n(.*?)\n\)json"', (SRC / "bridge" / "catalogue.cpp").read_text(encoding="utf-8"), re.S)
    if not embedded or embedded.group(1) != blocks[0]:
        errors.append("src/bridge/catalogue.cpp must hold the contract's catalogue block exactly; paste it from BRIDGE_CONTRACT.md")
    bot_copy = ROOT.parent / "twitchbot" / "BRIDGE_CONTRACT.md"
    if bot_copy.exists() and bot_copy.read_bytes() != contract_path.read_bytes():
        errors.append("BRIDGE_CONTRACT.md differs from ../twitchbot/BRIDGE_CONTRACT.md; change both together")


def self_test() -> None:
    """The patterns catch what they should (guards the guard)."""
    ignored = (ROOT / ".gitignore").read_text(encoding="utf-8").split()
    assert all(name in ignored for name in GENERATED), "a GENERATED file must be in .gitignore"
    assert LIBOBS.search('#include <obs.h>') and LIBOBS.search('#include <obs-frontend-api.h>')
    assert LIBOBS.search('#include <util/config-file.h>') and not LIBOBS.search('#include <QObject>')
    assert WIDGETS.search('#include <QWidget>') and WIDGETS.search('#include <QtWidgets/QLabel>')
    assert not WIDGETS.search('#include <QJsonObject>')
    assert OBS_CALL.search("obs_source_get_name(src)") and not OBS_CALL.search("ObsApi api;")


def main() -> int:
    self_test()
    errors: list[str] = []
    check_layers(errors)
    check_legacy(errors)
    check_contract(errors)
    for e in errors:
        print("ARCHITECTURE:", e)
    if not errors:
        print("architecture: all rules hold")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
