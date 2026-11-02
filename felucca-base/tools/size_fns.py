#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Build the main-loop code for size (build.py): the functions defined in SIZE_FILES get LLVM's
minsize (about -Oz) while everything else (the sound, render and ISR code, the flash and OTA code,
the main loop) keeps -Os. The JieLi clang 4 has no '#pragma clang attribute' and its minsize
attribute is an error on variables, so build.py compiles felucca.c to LLVM IR without the
optimizer, this script adds 'minsize' to those definitions, and the IR is then compiled at -Os
(without the edit, that round trip is byte-identical to compiling felucca.c directly).
  size_fns.py IN.ll OUT.ll"""
import re
import sys
from pathlib import Path

_ROOT = Path(__file__).resolve().parents[1]
SRC = _ROOT / "firmware" / "src" if (_ROOT / "firmware" / "src").is_dir() else _ROOT / "src"   # either layout
# the UI, the stores, the editor and the console: main loop only, never called by sound code; and (1.1) every
# other file without DSP: USB, MIDI, the sequencer, the drawing, the LCD, OTA, ... Of those, what an interrupt
# reaches (ISR_ROOTS and everything they call or name, from the IR) keeps -Os (isr_reach)
SIZE_FILES = ["ui.c", "favorites.c", "menu_items.c", "icons.c", "ui_graph.c", "ui_draw.c", "ui_menu.c", "ui_input.c",
              "storage.c", "upreset.c", "project.c", "settings_persist.c",
              "editor.c", "editor_preferences.c", "editor_backup.c", "editor_menu.c", "console.c",
              "usb.c", "midi_uart.c", "midi_control.c", "midi_clock.c", "seq.c", "chord.c", "motion.c",
              "panel.c", "lcd.c", "gfx.c", "ota.c", "ota_hw.c", "up_fm6.c", "params.c", "editor_fm6.c",
              "ui_layer.c", "ui_perform.c", "ui_name.c", "ui_events.c", "main.c", "libc.c", "storage_hw.c", "engines.c"]
# (DUELL 0.2.9: slice_store.c and ui_slice.c are gone with the SLICE engine)
# Apply the existing size policy to TIDE as well. isr_reach below still excludes
# every function reachable from either interrupt before applying minsize.
SIZE_FILES += ["ui_boxes.c", "ui_fm6op.c", "ui_guide.c", "ui_kb.c", "ui_tools.c"]
SIZE_FILES += ["morph_tide.c", "morph_music.c", "morph_fm_shape.c", "morph_complement.c",
               "morph_glide.c", "seq_morph.c", "seq_rhythm.c", "seq_phrase.c",
               "seq_evolve.c", "seq_voicelead.c", "seq_dialogue.c", "tide_seq.c",
               "tide_ui.c", "tide_store.c"]
ISR_ROOTS = ("fm1_alnk0_irq", "fm1_timer5_irq")      # hal/fm1_isr.S: the audio ISR, the key / LED scan (+ USB poll)
DEF = re.compile(r"^(?:static|void|int|uint\w*|int\w*|const)\b[^;=(]*?\b([A-Za-z_]\w*)\s*\(", re.M)
IR_DEF = re.compile(r"^(define [^\n]*?@\"?([\w.]+)\"?\([^\n]*\)(?: unnamed_addr| local_unnamed_addr)?)( #\d+[^\n]*\{)$",
                    re.M)


def definitions(text):
    """names of the functions text defines (a signature at column 0 whose ')' is followed by '{')"""
    names = []
    text = re.sub(r"/\*.*?\*/|//[^\n]*", " ", text, flags=re.S)        # comments (none hold code)
    for m in DEF.finditer(text):
        i, depth = m.end() - 1, 0
        while i < len(text):
            depth += {"(": 1, ")": -1}.get(text[i], 0)
            i += 1
            if depth == 0:
                break
        rest = re.sub(r"^(__attribute__\s*\(\(.*?\)\)\s*)+", "", text[i:i + 200].lstrip())
        if rest.startswith("{"):
            names.append(m.group(1))
    return names


def size_names():
    # morph.c also contains block-rate audio code: select only its main-loop
    # preset-roll entry here, rather than applying the policy to the whole file.
    names = {"morph_roll_from", "morph_end_pure", "morph_roll_step", "morph_fm_roll", "morph_xplan", "morph_xesig", "morph_xpoll", "fm6_cross_macros"}
    for f in SIZE_FILES:
        names.update(definitions((SRC / f).read_text()))
    return names


IR_BODY = re.compile(r"^define [^\n]*?@\"?([\w.]+)\"?\((.*?)^\}", re.M | re.S)
IR_REF = re.compile(r"@\"?([\w.]+)\"?")
# LLVM may wrap aggregate initializers; follow their indented continuation lines too.
IR_GLOBAL = re.compile(r'^@"?([\w.]+)"? = [^\n]*(?:\n[ \t]+[^\n]*)*', re.M)


def isr_reach(ir):
    """Functions an interrupt can run, including hooks reached through global tables.
    Follow the unoptimised IR before inlining; a named function pointer counts as
    called. Global initializers/aliases are traversal nodes, not returned functions."""
    body = {m.group(1): m.group(2) for m in IR_BODY.finditer(ir)}
    functions = set(body)
    for m in IR_GLOBAL.finditer(ir):
        body.setdefault(m.group(1), m.group(0))
    seen, todo = set(), [r for r in ISR_ROOTS if r in body]
    while todo:
        f = todo.pop()
        if f in seen:
            continue
        seen.add(f)
        todo += [g for g in IR_REF.findall(body[f]) if g in body and g not in seen]
    return seen & functions


def main(src, dst):
    ir = Path(src).read_text()
    isr = isr_reach(ir)
    names, hit = size_names() - isr, set()

    def mark(m):
        if m.group(2) not in names:
            return m.group(0)
        hit.add(m.group(2))
        # One shared cold constructor instead of copies inside roll/save paths.
        # This is selected only after the ISR-reachability exclusion above.
        attr = " minsize noinline" if m.group(2) == "morph_end_pure" else " minsize"
        return m.group(1) + attr + m.group(3)
    Path(dst).write_text(IR_DEF.sub(mark, ir))
    print(f"size: {len(hit)} main-loop functions built for size ({len(names) - len(hit)} not in this build; "
          f"{len(size_names() & isr)} reached from an interrupt kept -Os)")
    return 0


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:3]))
