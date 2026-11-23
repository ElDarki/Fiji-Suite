#!/usr/bin/env python3
"""Builds the Fiji Sun and Fiji Moon OBS themes (data/themes) from Fiji-chan's style guide.

Run: python3 tools/build-themes.py

The guide is the Fiji-chan bot's DESIGN.md and docs/style-guide/tokens.css (version 1.1). The two moods share one set of
component rules below; only the token values differ, the same way the bot's pages work. Qt style sheets cannot draw
shadows or transforms, so depth follows the guide's other cues: ink outlines by Sun, gold hairlines by Moon, the cel
edge under primary buttons.

Both themes are base themes built on Yami (OBS 31 to 33), so every control keeps OBS's sizes and behaviour. Fiji Sun
also carries Yami Light's rules (tools/themes/yami-light-*.txt/.qss, copied from OBS Studio 33, GPL-2.0) for its dark
icons, because a base theme cannot build on a variant.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "data" / "themes"
SRC = ROOT / "tools" / "themes"


def mix(fg, bg, alpha):
    """fg over bg at alpha, as #rrggbb (OBS drops the alpha of colour variables)."""
    f = [int(fg[i:i + 2], 16) for i in (1, 3, 5)]
    b = [int(bg[i:i + 2], 16) for i in (1, 3, 5)]
    return "#" + "".join(f"{round(alpha * x + (1 - alpha) * y):02x}" for x, y in zip(f, b))


SUN = dict(
    id="com.fijisuite.Fiji.Sun", name="Fiji Sun", dark="false", folder="FijiSun",
    blurb="Fiji-chan by day, a manga page on a late-afternoon beach: warm paper, ink outlines, flat colour, sea-glass\n"
          "   teal for every action.",
    ground="#f7f1eb", surface="#fffcf8", surface2="#f3ebe3", surface3="#e9ddd1",
    ink="#2b2230", ink2="#574a59", muted="#6b5e6c", disabled="#8a7d86",
    border="#e6d9cc", control="#98837b",
    accent="#167a71", accent_hover="#11655e", accent_edge="#0c534d", on_accent="#ffffff", accent_ink="#10675f",
    second="#d0802f", live="#c8243f", warning="#8f5000", danger="#bd2748", success="#18733f",
    line="#2b2230", line_w=2, float_line="#2b2230",  # ink by day
    field_w=2, r_btn=12, r_field=9, r_item=8, r_card=16, r_tab=12,
    display="'Zen Maru Gothic'",
    # Yami's slots: grey1 lines, grey2-4 controls, grey5 dock titles, grey6 panels, grey7 window, grey8 preview
    greys=["#98837b", "#ffffff", "#fffcf8", "#fffcf8", "#f3ebe3", "#fbf6f0", "#efe5db", "#e2d4c6"],
    # soft teal for text selection and Yami's other highlights, under ink text
    primary=["#a3cfc7", "#c4e1dc", "#167a71", "#7fb9b0", "#5ea79d"],
    blues=["#167a71", "#167a71", "#11655e", "#0c534d", "#0a3f3a", "#d8ece8"],
    link="#10675f",
)
SUN["accent_soft"] = mix(SUN["accent"], SUN["surface"], 0.12)

MOON = dict(
    id="com.fijisuite.Fiji.Moon", name="Fiji Moon", dark="true", folder="FijiMoon",
    blurb="Fiji-chan by night, a VIP lounge on the beach: midnight indigo, thin gold hairlines, Mincho headings,\n"
          "   moonlit teal for every action.",
    ground="#110f29", surface="#1a1738", surface2="#231f46", surface3="#2e2955",
    ink="#f1eefc", ink2="#cbc6ea", muted="#b9b3de", disabled="#8d87b8",
    border="#322c5e", control="#6f68a2",
    accent="#62d9c9", accent_hover="#8ee6da", accent_edge="#2a9a8e", on_accent="#04211e", accent_ink="#7fe3d7",
    second="#e3bd76", live="#ff7a93", warning="#ffd27a", danger="#ff8fa1", success="#7fe0a0",
    line="#322c5e", line_w=1, float_line=None,  # gold hairline, set below
    field_w=1, r_btn=12, r_field=7, r_item=7, r_card=14, r_tab=12,
    display="'Shippori Mincho B1'",
    greys=["#6f68a2", "#4a437e", "#3d3770", "#2e2955", "#231f46", "#1a1738", "#110f29", "#0a081f"],
    primary=["#175a69", "#1d6878", "#62d9c9", "#124a57", "#0e3b45"],
    blues=["#8ee6da", "#62d9c9", "#2a9a8e", "#1d6878", "#124a57", "#0e3b45"],
    link="#7fe3d7",
)
MOON["accent_soft"] = mix(MOON["accent"], MOON["surface"], 0.16)
MOON["float_line"] = mix(MOON["second"], MOON["surface"], 0.55)  # the gold hairline on floating things


def variables(t):
    g, p, b = t["greys"], t["primary"], t["blues"]
    return f"""    /* ---- {t['name']}: colours from the Fiji-chan style guide v1.1 ---- */
{chr(10).join(f'    --grey{i + 1}: {c};' for i, c in enumerate(g))}

    --primary: {p[0]};
    --primary_light: {p[1]};
    --primary_lighter: {p[2]};
    --primary_dark: {p[3]};
    --primary_darker: {p[4]};

    /* OBS blue (sliders, progress, mixer labels) becomes her teal */
{chr(10).join(f'    --blue{i + 1}: {c};' for i, c in enumerate(b))}

    /* OBS 31-32 read these; 33 colours the labels directly (rules below) */
    --warning: {t['warning']};
    --danger: {t['danger']};

    --text: {t['ink']};
    --text_light: {t['ink2']};
    --text_muted: {t['muted']};
    --text_disabled: {t['disabled']};
    --text_inactive: {t['ink2']};

    --border_color: {t['border']};
    --border_radius: {t['r_item']}px;
    --border_radius_small: 4px;
    --border_radius_large: {t['r_card']}px;

    --input_bg: {t['surface']};
    --input_bg_hover: {t['surface']};
    --input_bg_focus: {t['surface']};
    --input_border: {t['control']};
    --input_border_hover: {t['ink2']};
    --input_border_focus: {t['accent']};

    --button_bg: {t['surface']};
    --button_bg_hover: {t['surface2']};
    --button_bg_down: {t['surface3']};
    --button_bg_disabled: {t['surface2']};
    --button_border: {t['control']};
    --button_border_hover: {t['accent']};
    --button_border_focus: {t['accent']};

    --tab_border_selected: {t['accent']};
    --highlight_color: {t['accent']};
    --separator_hover: {t['accent']};

    --scrollbar_handle: {t['surface3']};
    --scrollbar_bg: {t['surface']};
    --scrollbar_hover: {t['control']};
    --scrollbar_down: {t['ink2']};
    --scrollbar_border: {t['border']};

    --palette_light: {g[1]};
    --palette_link: {t['link']};
    --palette_linkVisited: {t['link']};
    --palette_highlightedText: {t['ink']};"""


def checkbox_svgs(t):
    """Her checkbox: a rounded square, teal when ticked (the guide's .check)."""
    def box(fill, stroke, tick=None):
        mark = (f'<path d="M5.5 10.4l3 3 6-6.6" fill="none" stroke="{tick}" stroke-width="2.4" '
                f'stroke-linecap="round" stroke-linejoin="round"/>') if tick else ""
        return (f'<svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 20 20">'
                f'<rect x="1.5" y="1.5" width="17" height="17" rx="5" fill="{fill}" stroke="{stroke}" '
                f'stroke-width="{2 if t["field_w"] > 1 else 1.5}"/>{mark}</svg>\n')
    return {
        "checkbox_unchecked.svg": box(t["surface"], t["control"]),
        "checkbox_unchecked_focus.svg": box(t["surface"], t["accent"]),
        "checkbox_checked.svg": box(t["accent"], t["accent_edge"], t["on_accent"]),
        "checkbox_checked_focus.svg": box(t["accent_hover"], t["accent_edge"], t["on_accent"]),
        "checkbox_unchecked_disabled.svg": box(t["surface2"], t["border"]),
        "checkbox_checked_disabled.svg": box(t["border"], t["border"], t["disabled"]),
    }


def components(t):
    f = t["folder"]
    w, fw = t["line_w"], t["field_w"]
    # by Sun a floating thing gets the manga "float": 2px ink with a flat offset (thicker right and bottom edge)
    float_border = (f"border: 2px solid {t['float_line']}; border-right-width: 4px; border-bottom-width: 5px;"
                    if w > 1 else f"border: 1px solid {t['float_line']};")
    return f"""
/* ==========================================================================
   {t['name']}: her components (the guide's Components section), on Yami's sizes
   ========================================================================== */

/* her type: Nunito Sans for everything people operate; the display face only for headings */
QWidget {{
    font-family: 'Nunito Sans', 'Open Sans', '.AppleSystemUIFont', Helvetica, Arial, 'MS Shell Dlg', sans-serif;
}}

.text-heading, .text-title {{
    font-family: {t['display']}, 'Nunito Sans', 'Open Sans', sans-serif;
}}

/* ---- buttons: pills, Nunito Sans 700 ---- */
QPushButton {{
    background-color: {t['surface']};
    border: {fw}px solid {t['control']};
    border-radius: {t['r_btn']}px;
    font-weight: bold;
}}
QPushButton:hover, QPushButton:focus {{
    background-color: {t['surface2']};
    border-color: {t['accent']};
}}
QPushButton:pressed, QPushButton:pressed:hover {{
    background-color: {t['surface3']};
    border-color: {t['accent']};
}}
QPushButton:disabled {{
    background-color: {t['surface2']};
    border-color: {t['border']};
    color: {t['disabled']};
}}

/* switched on, and the live outputs: the primary look, a teal fill over a hard cel edge */
QPushButton:checked,
#streamButton:!hover:!pressed.state-active,
#recordButton:!hover:!pressed.state-active,
#pauseRecordButton:!hover:!pressed.state-active,
#replayBufferButton:!hover:!pressed.state-active,
#virtualCamButton:!hover:!pressed.state-active,
#modeSwitch:!hover:!pressed.state-active,
#broadcastButton:!hover:!pressed.state-active {{
    background: {t['accent']};
    color: {t['on_accent']};
    border: 1px solid {t['accent']};
    border-bottom: 3px solid {t['accent_edge']};
}}
QPushButton:checked:hover, QPushButton:checked:focus,
#streamButton:hover:!pressed.state-active,
#recordButton:hover:!pressed.state-active,
#pauseRecordButton:hover:!pressed.state-active,
#replayBufferButton:!pressed.state-active,
#virtualCamButton:!pressed.state-active,
#modeSwitch:hover:!pressed.state-active,
#broadcastButton:hover:!pressed.state-active {{
    background: {t['accent_hover']};
    color: {t['on_accent']};
    border: 1px solid {t['accent_hover']};
    border-bottom: 3px solid {t['accent_edge']};
}}

/* icon buttons */
QToolButton, .btn-tool {{
    background-color: transparent;
    border: 1px solid transparent;
    border-radius: {t['r_item']}px;
}}
QToolButton:hover, QToolButton:focus, .btn-tool:hover, .btn-tool:focus {{
    background-color: {t['surface2']};
    border-color: {t['control']};
}}
QToolButton:pressed, QToolButton:pressed:hover, .btn-tool:pressed, .btn-tool:pressed:hover {{
    background-color: {t['surface3']};
    border-color: {t['accent']};
}}
QToolButton:checked, .btn-tool:checked {{
    background-color: {t['accent_soft']};
    border-color: {t['accent']};
}}
QToolButton:disabled, .btn-tool:disabled {{
    background-color: transparent;
    border-color: transparent;
}}

/* ---- fields: a visible line, teal on focus ---- */
QLineEdit, QTextEdit, QPlainTextEdit, QComboBox, QDateTimeEdit, QSpinBox, QDoubleSpinBox {{
    background-color: {t['surface']};
    border: {fw}px solid {t['control']};
    border-radius: {t['r_field']}px;
}}
QLineEdit:hover, QTextEdit:hover, QPlainTextEdit:hover, QComboBox:hover, QDateTimeEdit:hover,
QSpinBox:hover, QDoubleSpinBox:hover {{
    border-color: {t['ink2']};
}}
QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QComboBox:focus, QDateTimeEdit:focus,
QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:on {{
    border-color: {t['accent']};
}}
QLineEdit:read-only, QLineEdit:read-only:hover, QLineEdit:read-only:focus {{
    background-color: {t['surface2']};
    border-color: {t['border']};
}}
QLineEdit:disabled, QTextEdit:disabled, QPlainTextEdit:disabled, QComboBox:disabled, QSpinBox:disabled,
QDoubleSpinBox:disabled {{
    background-color: {t['surface2']};
    border-color: {t['border']};
}}
QComboBox QAbstractItemView {{
    background-color: {t['surface']};
    {float_border}
}}

/* ---- checkboxes: her rounded square, teal when ticked ---- */
QCheckBox::indicator:unchecked, QGroupBox::indicator:unchecked, QTableView::indicator:unchecked {{
    image: url(theme:{f}/checkbox_unchecked.svg);
}}
QCheckBox::indicator:unchecked:hover, QGroupBox::indicator:unchecked:hover, QTableView::indicator:unchecked:hover {{
    image: url(theme:{f}/checkbox_unchecked_focus.svg);
}}
QCheckBox::indicator:checked, QGroupBox::indicator:checked, QTableView::indicator:checked {{
    image: url(theme:{f}/checkbox_checked.svg);
}}
QCheckBox::indicator:checked:hover, QGroupBox::indicator:checked:hover, QTableView::indicator:checked:hover {{
    image: url(theme:{f}/checkbox_checked_focus.svg);
}}
QCheckBox::indicator:checked:disabled, QGroupBox::indicator:checked:disabled, QTableView::indicator:checked:disabled {{
    image: url(theme:{f}/checkbox_checked_disabled.svg);
}}
QCheckBox::indicator:unchecked:disabled, QGroupBox::indicator:unchecked:disabled,
QTableView::indicator:unchecked:disabled {{
    image: url(theme:{f}/checkbox_unchecked_disabled.svg);
}}

/* OBS 32 and later: settings switches are teal when on */
idian--ToggleSwitch {{
    qproperty-background_checked: {t['accent']};
    qproperty-background_checked_hover: {t['accent_hover']};
}}

/* ---- lists and menus: the selected row is a chip, a teal outline on soft teal ---- */
QListView::item:hover, QListWidget::item:hover, QMenu::item:hover {{
    background: {t['surface2']};
    border: 1px solid {t['border']};
}}
QMenu::item:selected, QMenu > QWidget:selected, QListView::item:selected, QListWidget::item:selected,
QMenu::item:selected:hover, QListView::item:selected:hover, QListWidget::item:selected:hover {{
    background-color: {t['accent_soft']};
    border: 1px solid {t['accent']};
    color: {t['accent_ink']};
}}
.list-grid SceneTree::item:selected {{
    background-color: {t['accent_soft']};
}}
.list-grid SceneTree::item:checked {{
    background-color: {t['accent']};
    color: {t['on_accent']};
}}

QMenu, QMenu > QMenu {{
    background-color: {t['surface']};
    {float_border}
}}
QMenuBar::item:selected {{
    background: {t['accent_soft']};
    color: {t['accent_ink']};
    border-radius: {t['r_item']}px;
}}
QToolTip {{
    background-color: {t['surface']};
    color: {t['ink']};
    {float_border}
    padding: 4px 8px;
}}

/* ---- tabs: chips ---- */
QTabWidget::pane {{
    border: none;
    border-top: 1px solid {t['border']};
}}
QTabBar::tab, QTabBar::tab:top, QTabBar::tab:bottom {{
    background: {t['surface']};
    color: {t['ink2']};
    border: {fw}px solid {t['control']};
    border-radius: {t['r_tab']}px;
    font-weight: bold;
    margin: 2px 4px 4px 0px;
}}
QTabBar::tab:hover {{
    background: {t['surface2']};
    border-color: {t['accent']};
    color: {t['ink']};
}}
QTabBar::tab:selected {{
    background: {t['accent_soft']};
    border-color: {t['accent']};
    color: {t['accent_ink']};
}}

/* ---- docks are her panels: ink by day, a gold hairline by night, the title in her display face ---- */
QDockWidget {{
    font-family: {t['display']}, 'Nunito Sans', 'Open Sans', sans-serif;
    color: {t['ink']};
}}
QDockWidget::title {{
    background: {t['dock_title']};
    border: none;
    border-bottom: {w}px solid {t['dock_line']};
}}

/* group boxes are cards */
QGroupBox {{
    background: {t['surface']};
    border: 1px solid {t['border']};
    border-radius: {t['r_card']}px;
    font-family: {t['display']}, 'Nunito Sans', 'Open Sans', sans-serif;
}}

/* ---- sliders, scroll bars, progress ---- */
QSlider::sub-page:horizontal, QSlider::add-page:vertical {{
    background-color: {t['accent']};
}}
QSlider::handle:horizontal, QSlider::handle:vertical {{
    background-color: {t['surface']};
    border: 2px solid {t['accent']};
    border-radius: 8px;
}}
QSlider::handle:horizontal {{
    width: 12px;
    height: 12px;
    margin: -6px 0;
}}
QSlider::handle:vertical {{
    width: 12px;
    height: 12px;
    margin: 0 -6px;
}}
QSlider::handle:hover {{
    background-color: {t['accent_soft']};
}}
QSlider::handle:pressed {{
    background-color: {t['accent']};
}}
QScrollBar::handle {{
    border-radius: 5px;
}}
QProgressBar {{
    border-radius: {t['r_item']}px;
}}
QProgressBar::chunk {{
    background: {t['accent']};
    border: none;
    border-bottom: 2px solid {t['accent_edge']};
    border-radius: {t['r_item']}px;
}}

/* ---- states: always with a word (OBS 33 colours these labels directly) ---- */
.text-bright {{
    color: {t['link']};
}}
.text-success {{
    color: {t['success']};
}}
.text-warning {{
    color: {t['warning']};
}}
.text-danger {{
    color: {t['danger']};
}}

/* Fiji Suite's multiview: the on-air border and the dropped-frames warning */
CanvasTile {{
    qproperty-liveColor: {t['live']};
    qproperty-warningColor: {t['warning']};
}}
"""


def theme(t, light=False):
    head = f"""@OBSThemeMeta {{
    name: '{t['name']}';
    id: '{t['id']}';
    extends: 'com.obsproject.Yami';
    author: 'darkzeromex';
    dark: '{t['dark']}';
}}

/* {t['blurb']}
   Generated by tools/build-themes.py from the Fiji-chan style guide v1.1; edit that script, not this file.
   A base theme (Settings → Appearance → Theme) built on Yami, so OBS's sizes and behaviour stay the same; the
   colours, shapes and type follow the guide. Works with OBS 31, 32 and 33. Installed by the Fiji Suite plugin. */

@OBSThemeVars {{
"""
    light_vars = (SRC / "yami-light-vars.txt").read_text() + "\n" if light else ""
    light_rules = "\n" + (SRC / "yami-light-rules.qss").read_text() if light else ""
    return head + light_vars + variables(t) + "\n}\n" + light_rules + components(t)


SUN.update(dock_title=SUN["surface"], dock_line=SUN["ink"])
# Moon's surfaces carry a faint tonal sheen; the dock title gets it, never a glow
MOON.update(dock_title=f"qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 {mix('#ffffff', MOON['surface'], 0.05)}, "
                       f"stop:0.45 {MOON['surface']})",
            dock_line=MOON["float_line"])

for t, light, filename in ((SUN, True, "Fiji_Sun.obt"), (MOON, False, "Fiji_Moon.obt")):
    (OUT / filename).write_text(theme(t, light))
    folder = OUT / t["folder"]
    folder.mkdir(exist_ok=True)
    for name, svg in checkbox_svgs(t).items():
        (folder / name).write_text(svg)
    print("wrote", filename, "and", folder.name + "/")
