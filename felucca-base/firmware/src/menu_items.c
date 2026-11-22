/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* MENU's rows and their settings: what ui_menu.c draws and steps, and what the editor reads and sets (editor_menu.c
 * MENU_DESC / MENU_SET). One apply path for both: menu_put. Grouped in four tabs (MI_TAB): the screen (COLOR, LEDS, ANIMATION,
 * SCREEN OFF, HOME OPENS), the controls (BUTTON HOLD: the layer threshold, KNOB ACCEL, SCALE LEDS, the keys, STEP PREVIEW, STEP KEYS),
 * the sound (CLICK, CLICK LEVEL, COUNT-IN, TUNE), USB SERIAL, RESTORE LAST, then BUTTON MAP (the setup screen: HARDWARE CALIBRATION),
 * TEST (the CPU test screen; its editor id is the former INFO's), GUIDE (the first-start guide's pages, ui_guide.c; no editor id)
 * and ABOUT, the rows with no value (menu_valued).
 * MENU holds no MIDI setting: MIDI IN (OWN CH / SELECTED, the project's G_ROUTE) and LEARN CLEAR are on GLO's MIDI pages (MIDI,
 * SIDECHAIN, MIDI LEARN); there is no MIDI tab. HOME OPENS is the page the HOME
 * button and leaving the menu open (CONTROL tab).
 * TUNE is not a setting: it edits the project's G_TUNE, the value the project holds and saves (no format change;
 * a project load brings its own); nothing of its is in the settings record (MI_PROJECT: menu_get / menu_put read and write song.g) */
/* Row names: ANIMATION, BUTTON HOLD, MULTI LOAD (KEEPS MODE / SETS MODE), STEP KEYS (REPLACE / ADD), BUTTON MAP; SCALE LEDS is in DISPLAY.
 * The editor ids 18 and 23 (once SCOPE and OVERVIEW) are reserved, never reused.
 * MI order = the order the device shows (a tab's rows follow each other); the editor's ids are editor_menu.c ED_MENU's, by row, so
 * the order is no id. MI_NAME is the editor's name (at most 12 characters, ed_str) and the device's label unless MI_LABEL has one */
enum { MI_COLOR, MI_LEDS, MI_SCLLED, MI_ANIM, MI_SCROFF, MI_NOTESLEFT, MI_CPU,                    /* DISPLAY */
       MI_MODERECALL, MI_HOMEPG, MI_ACCEL, MI_HOLD, MI_CHADD, MI_PREVIEW, MI_PERFLEAVE,  /* CONTROL (MULTI LOAD .. STEP KEYS .., PERF LEAVE) */
#if DUELL_VELO
       MI_KEYVEL,                                      /* (KEY VELOCITY OFF / ON: builds with KEY SENSE built in only, DUELL_VELO; no row, no editor id otherwise) */
       MI_AFTERT,                                      /* (AFTERTOUCH OFF / ON, its own line, the same builds) */
#endif
       MI_CLICK, MI_CLKLVL, MI_COUNTIN, MI_TUNE,                 /* AUDIO (CLICK .. COUNT-IN, TUNE) */
       MI_RESTORE, MI_PANEL,                                                        /* SYSTEM (RESTORE LAST, BUTTON MAP) */
       MI_USBAUDIO,                                    /* (USB AUDIO OFF / ON, the audio input over USB; off by default; a build without FELUCCA_UAC has the row and it does nothing) */
#if FELUCCA_CDC
       MI_SERIAL,                                      /* (USB SERIAL: developer builds only, FELUCCA_CDC: the product has no serial console, so no row that would do nothing) */
#endif
#if DUELL_TEST_MODE
       MI_TEST,                                        /* (developer builds only: DUELL_TEST_MODE; the release has no TEST row) */
#endif
       MI_GUIDE,                                       /* (the first-start GUIDE; no value, no editor id) */
       MI_ABOUT, MI_ERASE, MI_COUNT };
                                                       /* (the editor ids 1, 2, 24 (STYLE, LARGE, HELP), 7 (FX LATCH), 18 (SCOPE) and 23 (OVERVIEW) are reserved, never reused) */
/* the rows that open a screen or a dialog and hold no value (BUTTON MAP, TEST, GUIDE, ABOUT, ERASE DATA; they are not all last: BUTTON MAP sits between two valued rows) */
static int menu_valued(uint32_t row);
/* the rows that open a screen with no value (TEST, GUIDE, ABOUT): OCT+ or a knob turned right opens them, a knob turned left or OCT- closes
 * them again (ui_menu.c menu_input; the GUIDE: its own keys, ui_guide.c). BUTTON MAP is a setup, not read-only: OCT+ only */
#if DUELL_TEST_MODE
#define MI_READONLY(row) ((row) == MI_TEST || (row) == MI_GUIDE || (row) == MI_ABOUT)
#else
#define MI_READONLY(row) ((row) == MI_GUIDE || (row) == MI_ABOUT)
#endif
static int menu_valued(uint32_t row) { return row < MI_COUNT && row != MI_PANEL && row != MI_ERASE && !MI_READONLY(row); }   /* (a row with a value) */
#define MI_PROJECT(row) ((row) == MI_TUNE)   /* the project's value (song.g), not a setting */
static const char *const MI_NAME[MI_COUNT] = {"COLOR", "LEDS", "SCALE LEDS", "ANIMATION", "SCREEN OFF", "NOTES LEFT", "CPU METER",
                                              "MULTI LOAD", "HOME OPENS", "KNOB ACCEL", "BUTTON HOLD", "STEP KEYS", "STEP PREVIEW", "PERF LEAVE",
#if DUELL_VELO
                                              "KEY VELOCITY", "AFTERTOUCH",
#endif
                                              "CLICK", "CLICK LEVEL", "COUNT-IN", "TUNE",
                                              "RESTORE LAST", "BUTTON MAP",
                                              "USB AUDIO",
#if FELUCCA_CDC
                                              "USB SERIAL",
#endif
#if DUELL_TEST_MODE
                                              "TEST",
#endif
#if DUELL_BENCH
                                              "BENCH",         /* (bench builds: the GUIDE row opens the BENCH screen, src/bench.c bench_input; the row and its
                                                                * code stay as they are, so the menu's code keeps its size: the first-start GUIDE still opens) */
#else
                                              "GUIDE",
#endif
                                              "ABOUT", "ERASE DATA"};
/* the device's label where it is not MI_NAME (0: MI_NAME). One '\n' at most, a designed break (nothing wraps by itself):
 * the line after it is drawn small (S, MID: a hint, or the rest of a long name) and the row takes two units (ui_menu.c) */
static const char *const MI_LABEL[MI_COUNT] = {
    [MI_LEDS] = "LIGHTS", [MI_SCLLED] = "SCALE LIGHTS", [MI_PERFLEAVE] = "LEAVING PERFORM",   /* (MI_NAME stays the editor's key, the editor says it in MENU_LONG) */
    [MI_ACCEL] = "KNOB ACCELERATION",                 /* (no cryptic ACCEL; MI_NAME stays the editor's key, the editor says it in MENU_LONG) */
#if FELUCCA_CDC
    [MI_SERIAL] = "USB SERIAL\nDEVELOPER CONSOLE"      /* (USB SERIAL: its hint) */
#endif
};
static const char *menu_label(uint32_t row)
{
    return row >= MI_COUNT ? "" : MI_LABEL[row] ? MI_LABEL[row] : MI_NAME[row];
}
/* the MENU's tabs (ui_menu.c: SELECT moves through the rows and over the tabs, ALGORITHM and PRESETS do nothing in the menu; the editor gets a
 * row's tab after its MENU_DESC reply). A tab's rows follow each other in MI order (tests/ui_test.c checks it).
 * A row is one unit (21 px and a 1 px rule) or two (a two-line label); a page holds MPAGE_UNITS units; a tab with more goes on
 * on a page 2 (menu_place), its pill then reads "CONTROL 1/2". Tabs are in the order the device shows them; a tab's index is what
 * the editor is told, not an id */
enum { MTAB_DISPLAY, MTAB_CONTROL, MTAB_AUDIO, MTAB_SYSTEM, MTAB_COUNT };   /* (DUELL: MIDI is GLO's, no tab here) */
#define MPAGE_UNITS 7u
static const char *const MTAB_NAME[MTAB_COUNT] = {"DISPLAY", "CONTROL", "AUDIO", "SYSTEM"};
static const uint8_t MI_TAB[MI_COUNT] = {
    MTAB_DISPLAY, MTAB_DISPLAY, MTAB_DISPLAY, MTAB_DISPLAY, MTAB_DISPLAY, 0xFFu, 0xFFu,   /* COLOR LEDS SCALE LEDS ANIMATION SCREEN OFF (NOTES LEFT: no row, the capsules show no voices; its bit stays reserved) (CPU METER, last of its group: no tab, no row: there is no CPU bar; the row, its stored bit and its editor id 26 stay reserved) */
    MTAB_CONTROL, MTAB_CONTROL, MTAB_CONTROL, MTAB_CONTROL, MTAB_CONTROL, MTAB_CONTROL,   /* MULTI LOAD HOME OPENS KNOB ACCEL BUTTON HOLD STEP KEYS
                                                                                          * STEP PREVIEW */
    MTAB_CONTROL,                                                                         /* PERF LEAVE (FADE / KEEP; the 7th unit: one page is full) */
#if DUELL_VELO
    MTAB_CONTROL, MTAB_CONTROL,                                                           /* KEY VELOCITY, AFTERTOUCH (OFF / ON; CONTROL page 2) */
#endif
    MTAB_AUDIO, MTAB_AUDIO, MTAB_AUDIO, MTAB_AUDIO,                                       /* CLICK CLICK LEVEL COUNT-IN TUNE */
    MTAB_SYSTEM, MTAB_SYSTEM,                                                             /* RESTORE LAST BUTTON MAP */
    MTAB_SYSTEM,                                                                          /* USB AUDIO */
#if FELUCCA_CDC
    MTAB_SYSTEM,                                                                          /* USB SERIAL */
#endif
#if DUELL_TEST_MODE
    MTAB_SYSTEM,                                                            /* TEST */
#endif
    MTAB_SYSTEM,                                                            /* GUIDE */
    MTAB_SYSTEM,                                                            /* ABOUT */
    MTAB_SYSTEM,                                                            /* ERASE DATA (OCT+ opens its dialog, ui_menu.c) */
};
/* TUNE (MASTER TUNE like a Roland keyboard's): the project's G_TUNE, the frequency of A4 in 0.1 Hz, 415.3 .. 466.2 Hz (core.h TUNE_MIN .. TUNE_MAX); as a row
 * its values 0..TUNE_N - 1 (value - TUNE_MIN, 510 steps, 247 = 440.0 Hz). The editor gets it as a number in tenths (EDM_TENTHS: min..max, its unit "Hz") */
#define TUNE_N ((uint32_t)(TUNE_MAX - TUNE_MIN + 1))
typedef char mtab_fits_ui[sizeof ui.menu_row >= MTAB_COUNT ? 1 : -1];   /* (ui.c: the row last picked per tab) */
static uint32_t mtab_first(uint32_t t)                 /* a tab's first row */
{
    uint32_t i = 0;
    while (i < MI_COUNT && MI_TAB[i] != t)
        i++;
    return i;
}
static uint32_t mtab_rows(uint32_t t)                  /* .. and how many it has */
{
    uint32_t i, n = 0;
    for (i = 0; i < MI_COUNT; i++)
        n += MI_TAB[i] == t;
    return n;
}
/* a row's units (1, or 2 for a two-line label) and its place: the page of its tab and the unit it starts at. Rows fill a
 * page in MI order until the next one does not fit; the rest go on the next page (a few loops over ~26 rows: no table to keep) */
static uint32_t menu_units(uint32_t row)
{
    const char *l = menu_label(row);
    while (*l && *l != '\n')
        l++;
    return *l ? 2u : 1u;
}
static uint32_t menu_place(uint32_t row, uint32_t *unit)   /* the page (0 ..) of row in its tab; *unit its first unit there */
{
    uint32_t i = mtab_first(MI_TAB[row % MI_COUNT]), pg = 0, u = 0, n;
    for (;; i++) {
        n = menu_units(i);
        if (u + n > MPAGE_UNITS) {
            pg++;
            u = 0;
        }
        if (i >= row % MI_COUNT)
            break;
        u += n;
    }
    if (unit)
        *unit = u;
    return pg;
}
static uint32_t mtab_pages(uint32_t t)                 /* a tab's pages (1 ..) */
{
    return menu_place(mtab_first(t) + mtab_rows(t) - 1u, 0) + 1u;
}
/* the two-valued rows: a bit of ui_prefs (PREF_REC: of ui_rec_prefs, ui.c PREF_SCALE_LEDS_OFF) and its names (bit clear =
 * the default, bit set). A step up (a knob right,
 * OCT+) = ON on the ON / OFF rows (the switch's knob to the right; ANIM, USB SERIAL: their bit clears), else the
 * second name; a step down the other (menu_step) */
typedef struct { uint8_t row; uint32_t bit; const char *name[2]; } menu_flag_t;
static const menu_flag_t MENU_FLAGS[] = {
    {MI_ACCEL, PREF_ACCEL, {"OFF", "ON"}},            /* ui_input.c accel */
#if FELUCCA_CDC
    {MI_SERIAL, PREF_SERIAL_OFF, {"ON", "OFF"}},       /* OFF, no serial console (usb_serial_apply) */
#endif
    {MI_RESTORE, PREF_RESTORE_OFF, {"ON", "OFF"}},     /* OFF, no autosave, power-on as new (project.c) */
    {MI_SCLLED, PREF_SCALE_LEDS_OFF, {"AUTO", "OFF"}}, /* AUTO (default), the keys show the scale when it is not CHROMATIC; OFF, they do not (ui_input.c) */
                                                       /* (ui_prefs2 bit 0, once SCOPE OUT / MIX: ignored) */
    {MI_PREVIEW, PREF_PREVIEW, {"OFF", "ON"}},         /* ON, the cursor's step sounds (ui_input.c) */
    {MI_CPU, PREF_CPU_OFF, {"ON", "OFF"}},             /* OFF, no CPU bar under the header's battery (ui_draw.c); bit clear = ON */
    {MI_MODERECALL, PREF_MODE_RECALL, TIDE_MLOAD_NAMES},   /* TIDE hook */   /* MULTI LOAD: KEEPS MODE (default, bit clear) a multi load
                                                       * leaves MODE, XFADE and SPLIT (the MODE page; TIDE: their TIDE SETTINGS rows) as they are; SETS MODE, it sets the ones saved in the multi (multi.c) */
    {MI_CHADD, PREF_CHORD_ADD, {"REPLACE", "ADD"}},      /* STEP KEYS REPLACE / ADD: ADD, keys add to the cursor step (ui_input.c) */
};
static const char *const HOLD_N[4] = {"0.3 s", "0.4 s", "0.5 s", "0.6 s"};   /* (the editor's names; the menu draws its own) */
/* the metronome (click.c) and the count-in (seq.c); ui.c ui_rec_prefs holds them */
static const char *const CLICK_N[3] = {"OFF", "REC", "ON"};          /* REC: while a track is armed and playing */
static const char *const CLKLVL_N[3] = {"LOW", "MID", "HIGH"};
static const char *const COUNTIN_N[3] = {"OFF", "1 BAR", "2 BARS"};
/* SCREEN OFF (ui.c scr_*: the screen dark after this long without panel input; NEVER is the default) */
static const char *const SCROFF_N[5] = {"NEVER", "5 MIN", "15 MIN", "30 MIN", "60 MIN"};
/* ANIM (OFF, values snap: ui_draw.c roll_note, ui_graph.c pr_follow; no power-on sweep, main.c boot_leds). IDLE: as ON and the LEDs' idle animation after a minute without input (ui_input.c idle_leds; ui.c
 * ui_idle). Its values: 0 ON, 1 OFF (the editor's), 2 IDLE; the menu steps them OFF, ON, IDLE (ANIM_ORD) */
static const char *const ANIM_N[3] = {"ON", "OFF", "IDLE"};
static const uint8_t ANIM_ORD[3] = {1, 0, 2};          /* (a value's place in the menu's order, and back: its own inverse) */
/* (the settings byte [15][26], once OVERVIEW, is kept as stored) */
/* HOME OPENS (DUELL): the page the HOME button opens from another page and leaving the menu (ui.c go_main): LAST (the
 * default, stored 0: the HOME page shown last), MODE, MORPH (stored 2, the old MIXER's code); ui_home_pg (ui.c), an unknown stored code reads as LAST (OVERVIEW's 3 too) */
static const char *const HOMEPG_N[HP_COUNT] = {"LAST", TIDE_HOME_MODE_NAME, TIDE_HOME_NAME};   /* TIDE hooks */

/* MENU > USB SERIAL. The serial console is a developer tool (README: FELUCCA_CDC): only developer builds (DUELL_BENCH / DUELL_TEST_MODE) have it, and the row. The product never has it: usb_serial_apply is empty there, and the saved PREF_SERIAL_OFF bit is read by nobody. OFF is the default (a new unit,
 * ERASE DATA, and once on a unit saved with default ON: settings_persist.c settings_defaults). ON presents it,
 * the descriptors byte for byte as before; OFF re-enumerates as audio + MIDI only, device class 0 (the bytes of a
 * FELUCCA_CDC=0 build): macOS 13-15 then attach their USB audio driver (with the console Apple's CDC composite driver
 * takes the device and the audio input never appears). USB-MIDI stays, and with it the editor, the update installer
 * and the soft key (SysEx on EP1). Applied at boot before USB starts (main.c: enumerated with it from the start, never
 * on then off) and while the menu is closed (ui_input: one re-enumeration when it closes, not one per KNOB 1 step).
 * A change from the editor (MENU_SET) waits 200 ms (usb.set_at) so its reply leaves before the device drops off
 * the bus: the whole device re-enumerates, USB-MIDI and audio too */
/* MENU > SYSTEM > USB AUDIO (the product's one USB row): the same machinery. OFF (the default, ui.c usb_audio_get) presents MIDI only (usb.c: descriptors without the audio
 * streaming interface, no isochronous traffic, the resampler never runs), ON the audio input as before. Applied at boot before usb_start (enumerated with it from the start),
 * and when the menu has closed or 200 ms after an editor change; one re-enumeration (the device drops off the bus for a moment, the editor reconnects). Bench builds always present it */
static void usb_serial_apply(void)
{
#if FELUCCA_CDC || FELUCCA_UAC
    if (usb.set_at && (uint16_t)((uint16_t)fm1_ms - (usb.set_at & ~1u)) < 200u)   /* (& ~1: at an even fm1_ms the | 1 was 1 ms ahead; 16 bits wrap, fine for 200 ms) */
        return;
    usb.set_at = 0;
#endif
#if FELUCCA_CDC
#if DUELL_BENCH
    usb_cdc_switch(FELUCCA_CDC_DEFAULT);                /* (bench builds: the console is always presented, whatever USB SERIAL says: the bench suite's runner needs it) */
#else
    usb_cdc_switch(FELUCCA_CDC_DEFAULT && !(ui_prefs & PREF_SERIAL_OFF));   /* (a FELUCCA_CDC_DEFAULT=0 build: off) */
#endif
#endif
#if FELUCCA_UAC
#if DUELL_BENCH
    usb_audio_switch(1);
#else
    usb_audio_switch(usb_audio_get());
#endif
#endif
}
static const menu_flag_t *menu_flag(uint32_t row)
{
    uint32_t i;
    for (i = 0; i < NELEM(MENU_FLAGS); i++)
        if (MENU_FLAGS[i].row == row)
            return &MENU_FLAGS[i];
    return 0;
}

/* a row's value as 0..menu_n(row) - 1 in the order the menu steps it (LEDS: OFF LOW MEDIUM MAX, LEDS_MENU) */
static uint32_t menu_n(uint32_t row)
{
#if DUELL_VELO
    if (row == MI_KEYVEL || row == MI_AFTERT)
        return SC_TYPE_N + 1u;                         /* OFF + 5 types */
#endif
    return row == MI_COLOR ? NPALETTES : (row >= MI_CLICK && row <= MI_COUNTIN) ? 3u :
           row == MI_HOLD ? 4u : row == MI_LEDS ? LEDS_COUNT : row == MI_SCROFF ? NELEM(SCROFF_N) :
           row == MI_TUNE ? TUNE_N : row == MI_ANIM ? 3u : row == MI_HOMEPG ? (uint32_t)HP_COUNT : 2u;
}
static uint32_t menu_get(uint32_t row)
{
    const menu_flag_t *f = menu_flag(row);
    uint32_t i = 0;
    if (f)
        return ((ui_prefs | (uint32_t)ui_rec_prefs << 8 | (uint32_t)ui_prefs2 << 16) & f->bit) != 0;
    switch (row) {
    case MI_COLOR: return settings.palette % NPALETTES;
    case MI_HOLD: return settings_hold % 4u;
    case MI_SCROFF: return scr_get();
    case MI_ANIM: return ui_prefs & PREF_ANIM_OFF ? 1u : (ui_idle & 1u) ? 2u : 0u;
    case MI_HOMEPG: return ui_home_pg < HP_COUNT ? ui_home_pg : HP_LAST;
    case MI_PERFLEAVE: return perf_keep();
    case MI_NOTESLEFT: return notes_show_on();         /* 1 = ON (the stored bit is OFF: set = OFF, clear = ON, the default) */
    case MI_USBAUDIO: return usb_audio_get();          /* 1 = ON (tag + bit; anything else OFF, the default) */
#if DUELL_VELO
    case MI_KEYVEL: return keyvel_on() ? 1u + keyvel_type() : 0u;   /* 0 OFF, 1..5 SOFTER .. HARDER (the ON bit: set = ON, clear = OFF, the default; the type in ui_sense_type) */
    case MI_AFTERT: return aftert_on() ? 1u + aftert_type() : 0u;   /* the same (ON = bit 7 clear) */
#endif
    case MI_CLICK: case MI_CLKLVL: case MI_COUNTIN: return rp_get(row - MI_CLICK);
    case MI_TUNE: return (uint32_t)((song.g[G_TUNE] >= TUNE_MIN && song.g[G_TUNE] <= TUNE_MAX ? song.g[G_TUNE] : TUNE_DEF) - TUNE_MIN);   /* (out of range: 440.0) */
    case MI_LEDS:
        while (i + 1u < LEDS_COUNT && LEDS_MENU[i] != settings_leds)
            i++;
        return i;
    }
    return 0;
}
static const char *menu_vname(uint32_t row, uint32_t v)
{
    const menu_flag_t *f = menu_flag(row);
    if (f)
        return f->name[v & 1u];
    switch (row) {
    case MI_COLOR: return UI_PALETTES[v % NPALETTES].name;
    case MI_HOLD: return HOLD_N[v & 3u];
    case MI_CLICK: return CLICK_N[v % 3u];
    case MI_CLKLVL: return CLKLVL_N[v % 3u];
    case MI_COUNTIN: return COUNTIN_N[v % 3u];
    case MI_SCROFF: return SCROFF_N[v % NELEM(SCROFF_N)];
    case MI_ANIM: return ANIM_N[v % 3u];
    case MI_HOMEPG: return HOMEPG_N[v % HP_COUNT];
#if DUELL_VELO
    case MI_KEYVEL: case MI_AFTERT: {                  /* OFF + 5 types, the same words on the unit and in the editor (list names are PV_LEN 32: spelled out) */
        static const char *const TYPE_N[6] = {"OFF", "SOFTER", "SOFT", "NORMAL", "HARD", "HARDER"};
        return TYPE_N[v % 6u];
    }
#endif
    case MI_NOTESLEFT: return v & 1u ? "ON" : "OFF";
    case MI_USBAUDIO: return v & 1u ? "ON" : "OFF";
    case MI_PERFLEAVE: return v & 1u ? "KEEP" : "FADE";   /* leaving PERFORM / loading a sound: SPACE and KILL fade to neutral (default) / stay */
    case MI_LEDS: return LEDS_NAME[LEDS_MENU[v % LEDS_COUNT]];
    case MI_TUNE: {                                    /* A4 in Hz with one decimal ("440.0"; the menu adds "Hz") */
        static char b[8];
        fmt_fix(b, (int32_t)(v % TUNE_N) + TUNE_MIN, 1);
        return b;
    }
    }
    return "";
}
/* set a row (v below menu_n(row)) and do what the menu does on a change: the palette and the EQ at once;
 * ANIM, LEDS, HOLD, the flags are read where they are used, every frame;
 * USB SERIAL when the menu has closed (usb_serial_apply). Saved by the caller (menu_close, the editor). TUNE: the project's
 * value (voice.c reads TUNE); kept with the project (the autosave), not by settings_save */
static void menu_put(uint32_t row, uint32_t v)
{
    const menu_flag_t *f = menu_flag(row);
    if (f) {
        if (f->bit & PREF_X)
            ui_prefs2 = (uint8_t)(v ? ui_prefs2 | f->bit >> 16 : ui_prefs2 & ~(f->bit >> 16));
        else if (f->bit & PREF_REC)
            ui_rec_prefs = (uint8_t)(v ? ui_rec_prefs | f->bit >> 8 : ui_rec_prefs & ~(f->bit >> 8));
        else
            ui_prefs = (uint8_t)(v ? ui_prefs | f->bit : ui_prefs & ~f->bit);
        return;
    }
    switch (row) {
    case MI_COLOR:
        settings.palette = v;
        pal_q.due = 1;                                 /* (applied by ui_draw.c ui_draw, at most every 100 ms: ui.c pal_q) */
        break;
    case MI_HOLD: settings_hold = (uint8_t)v; break;
    case MI_SCROFF: scr_put(v); break;                 /* (read every frame: ui.c scr_frame) */
    case MI_ANIM:                                      /* (read where they are used, every frame) */
        ui_prefs = (uint8_t)(v == 1u ? ui_prefs | PREF_ANIM_OFF : ui_prefs & ~PREF_ANIM_OFF);
        ui_idle = (uint8_t)(v == 2u ? ui_idle | 1u : ui_idle & ~1u);
        break;
    case MI_HOMEPG: ui_home_pg = (uint8_t)(v < HP_COUNT ? v : HP_LAST); break;   /* (read where HOME opens: go_main) */
    case MI_LEDS: settings_leds = LEDS_MENU[v]; break;
#if DUELL_VELO
    case MI_KEYVEL:                                    /* (v 0 OFF, 1..5 a type; ON = the bit set; sense.c takes it at once, ui_input.c again every frame) */
        ui_idle = (uint8_t)(v ? ui_idle | KEYVEL_ON_BIT : ui_idle & ~KEYVEL_ON_BIT);
        if (v)
            ui_sense_type = (uint8_t)((ui_sense_type & ~7u) | (v <= SC_TYPE_N ? v : SC_TYPE_NORMAL + 1u));   /* (stored type + 1 = the menu value) */
        sense_enable(v != 0u);
        sense_vel_type_set(keyvel_type());
        break;
    case MI_AFTERT:                                    /* (its own bit, inverted: the bit set = OFF, so the default is ON; the keys' channel pressure) */
        ui_idle = (uint8_t)(v ? ui_idle & ~AFTERT_OFF_BIT : ui_idle | AFTERT_OFF_BIT);
        if (v)
            ui_sense_type = (uint8_t)((ui_sense_type & ~(7u << 3)) | ((v <= SC_TYPE_N ? v : SC_TYPE_NORMAL + 1u) << 3));
        sense_at_enable(v != 0u);
        sense_at_type_set(aftert_type());
        break;
#endif
    case MI_NOTESLEFT: ui_notes_off = (uint8_t)(v ? ui_notes_off & ~NOTES_OFF_BIT : ui_notes_off | NOTES_OFF_BIT); break;   /* (read where the header is drawn: ui_draw.c) */
    case MI_USBAUDIO: ui_runkeep = (uint8_t)((ui_runkeep & 7u) | UAUD_TAG | (v ? UAUD_ON : 0u)); break;   /* (applied when the menu closes: usb_serial_apply) */
    case MI_PERFLEAVE: ui_idle = (uint8_t)(v ? ui_idle | PERF_KEEP_BIT : ui_idle & ~PERF_KEEP_BIT); break;   /* (read where PERFORM is left: ui_perform.c) */
    case MI_CLICK: case MI_CLKLVL: case MI_COUNTIN: rp_put(row - MI_CLICK, v); break;   /* (at once: click.c, seq.c) */
    case MI_TUNE: song.g[G_TUNE] = (int16_t)((int32_t)v + TUNE_MIN); break;
    }
}
/* the menu's step, any of KNOB 1..4 or OCT+ (s > 0) / OCT- (s < 0): the next / previous value, stopping at the ends;
 * COLOR wraps round its palettes. On an ON / OFF row up is ON (MENU_FLAGS). TUNE: s steps of 0.1 Hz at a time (510 values; ui_menu.c
 * gives a fast turn KNOB ACCEL's multiple over that range, up to x8) */
static uint32_t menu_step(uint32_t row, int32_t s)
{
    const menu_flag_t *f = menu_flag(row);
    uint32_t v = menu_get(row), n = menu_n(row);
    if (row == MI_TUNE)
        return (uint32_t)clamp((int32_t)v + s, 0, (int32_t)n - 1);
    if (row == MI_ANIM)                                /* OFF ON IDLE, stopping at the ends */
        return ANIM_ORD[clamp((int32_t)ANIM_ORD[v % 3u] + (s > 0) - (s < 0), 0, 2)];
    if (f && str_eq(f->name[0], "ON"))                 /* (USB SERIAL, RESTORE LAST, CPU METER: ON is their value 0) */
        s = -s;
    if (s > 0)
        return v + 1u < n ? v + 1u : row == MI_COLOR ? 0u : v;
    if (s < 0)
        return v ? v - 1u : row == MI_COLOR ? n - 1u : 0u;
    return v;
}
