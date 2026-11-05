# DUELL factory sounds: PATCHES numbers (DUELL 1.1 with TIDE)

The 163 factory sounds in the order the device lists them in PATCHES (TYPE ALL): by category (BASS, LEAD, PAD, PLUCK, KEYS, PERC, FX, OTHER), then engine, then name. **A number up to 128 is a patch slot**: slot #NN holds the factory sound of this list at position NN until you save a sound there (RESTORE brings it back); MIDI program change n loads slot n, #n + 1. Generated from `felucca-base/tools/duell_sounds.py` with the sort in `felucca-base/tools/gen_duell_sounds.py` (`gen_duell_sounds.py --check` prints the same list).

**1.0: the factory bank is OSCILL/R's bank** (`felucca-base/tools/factory_bank.duell-patches.json`, imported by `tools/import_bank.py`; each patch exactly as exported, only its TRIM balanced). YO YA is new and CARNIVAL is not in the bank, so #108 to #125 moved by one against 0.2.31.

**Past the 128 (1.1 with TIDE):** #129 to #163 are 35 more factory sounds (the 8 of the dub pack, 24 for TIDE's JRNY: pads, chords, stabs, plucks, textures, and 3 FM6 dub voices: FM HAZE, DUB DRAWBAR, BELL STAB), listed after the 128 so no number of the 128 moves. They are read-only places in PATCHES, not slots: a number past #128 holds no saved sound and SAVE offers USER 1 for it. They can be marked FAV (the star belongs to the sound, not to a slot) and MIDI program change reaches them in bank 2 (bank select CC0 = 0, CC32 = 2): program 0 is #129, program 34 is #163. The same holds for the displaced #125 to #128 for FAV; program change cannot call those four. Their order is the same one: category, engine, name. (With the USER slots, U1 to U4 stand at the places of #125 to #128, which stay listed after the slots.) Adding sounds can move the numbers past #128 (they keep the category order); no number of the 128 moves.

| PATCHES # | Name |
|---|---|
| 1 | BEHIVE |
| 2 | FOLD BASS |
| 3 | LADDER BASS |
| 4 | RUBBER BAND |
| 5 | SQUELCH |
| 6 | TIDAL SUB |
| 7 | FM BASS 2 |
| 8 | FRETLESS |
| 9 | SLAP DOCTOR |
| 10 | ELASTIC BASS |
| 11 | FUZZ BASS |
| 12 | ARP BASS |
| 13 | CARTRIDGE |
| 14 | BULLFROG |
| 15 | BREADBIN |
| 16 | SID BASS |
| 17 | SYNC BASS |
| 18 | PEDAL BASS |
| 19 | PLUCK BASS |
| 20 | GEARBOX |
| 21 | GROWL BASS |
| 22 | HARM BASS |
| 23 | KICK BASS |
| 24 | REESE BASS |
| 25 | DRIVEN SAW |
| 26 | FUNFARE |
| 27 | RAZORWIRE |
| 28 | SQUARE LEAD |
| 29 | TORCH SONG |
| 30 | WALL OF SAW |
| 31 | HAN SOLO |
| 32 | BRAZEN |
| 33 | PIXEL HERO |
| 34 | BANSHEE |
| 35 | OBOE-ISH |
| 36 | SMOKE RING |
| 37 | PAN WHISTLE |
| 38 | HORNET |
| 39 | HOOVER |
| 40 | LICORICE |
| 41 | PURPLE FLUTE |
| 42 | SCREAMER |
| 43 | SYNC LEAD |
| 44 | EMBER GLOW |
| 45 | SILK CHORUS |
| 46 | VELVET BOW |
| 47 | GLASS HALL |
| 48 | LAMPLIGHT |
| 49 | WOW FLUTTER |
| 50 | PLATE CHOIR |
| 51 | VESPERS |
| 52 | WHISPER PAD |
| 53 | CHIP CHAPEL |
| 54 | STRING MACH |
| 55 | CHOIR LOFT |
| 56 | SINGING BOWL |
| 57 | SEA ORGAN |
| 58 | BRASS PAD |
| 59 | BREATH PAD |
| 60 | DIGITAL PAD |
| 61 | EVOLVING PAD |
| 62 | GLASS PAD |
| 63 | PWM PAD |
| 64 | VOWEL PAD |
| 65 | ARP PLUCK |
| 66 | PIZZICATO |
| 67 | ISLAND STEEL |
| 68 | MALLET WOOD |
| 69 | FAKE KOTO |
| 70 | WIRE HARP |
| 71 | MARBLE RUN |
| 72 | DOO WOP |
| 73 | PEBBLE POP |
| 74 | NIGHT BOX |
| 75 | GHOST HARP |
| 76 | NURSERY KEYS |
| 77 | NYLON GUT |
| 78 | DIGI PLUCK |
| 79 | SFORZANDO |
| 80 | BIG STAB |
| 81 | FROST BELLS |
| 82 | SALOON KEYS |
| 83 | TINE ROOM |
| 84 | TUBE BELLS |
| 85 | CLAVICLE |
| 86 | TIN TOLL |
| 87 | JAZZ CLUB |
| 88 | FAUX HARPSI |
| 89 | MOTOR VIBES |
| 90 | ROSEWOOD |
| 91 | DRAWBARS |
| 92 | PLATE BELL |
| 93 | SYNTH BRASS |
| 94 | WAVE CHIMES |
| 95 | DEEP KICK |
| 96 | HARD KICK |
| 97 | FIELD CLANK |
| 98 | FLOOR THUD |
| 99 | OPEN SIZZLE |
| 100 | STATIC SNARE |
| 101 | TIZZ HAT |
| 102 | STORM FRONT |
| 103 | ALIEN CHIMES |
| 104 | CRYSTAL CAVE |
| 105 | SONAR PING |
| 106 | DIVE BOMB |
| 107 | GHOST VOICES |
| 108 | YO YA |
| 109 | CIRCUIT BEND |
| 110 | LESLIE STORM |
| 111 | TAPE DRONE |
| 112 | RAIN CHIMES |
| 113 | MOOR WIND |
| 114 | REV CYMBAL |
| 115 | WHITE OUT |
| 116 | FROZEN AIR |
| 117 | RESO SWEEP |
| 118 | GLASS SHARDS |
| 119 | TEMPLE BLOCK |
| 120 | DEAD BATTERY |
| 121 | DUSTY DRONE |
| 122 | PIXEL KICK |
| 123 | VOCAL BEATS |
| 124 | DIAL TONE |
| 125 | GRAVEL SNARE |
| 126 | TABLA TALK |
| 127 | GEIGER |
| 128 | MODEM |
| 129 | FOG BANK |
| 130 | NEON DRIFT |
| 131 | SLOW TIDE |
| 132 | WARM WASH |
| 133 | FM HAZE |
| 134 | AIR SHIMMER |
| 135 | DEEP BLUE |
| 136 | STAR FIELD |
| 137 | TAPE CLOUD |
| 138 | ORGAN WASH |
| 139 | ROTOR HAZE |
| 140 | DUB BLIP |
| 141 | HOLLOW POP |
| 142 | SUB PLUCK |
| 143 | TAPE PLUCK |
| 144 | AIR POP |
| 145 | BOTTLE TAP |
| 146 | COMB PLUCK |
| 147 | CHORD ECHO |
| 148 | DARK MIN7 |
| 149 | LOW STAB |
| 150 | SOUL STAB |
| 151 | SUNDAY KEYS |
| 152 | BELL STAB |
| 153 | DUB DRAWBAR |
| 154 | ICE STAB |
| 155 | MELLOW EP |
| 156 | SOFT EP |
| 157 | DEEP ORGAN |
| 158 | HOUSE ORGAN |
| 159 | ORGAN SKANK |
| 160 | SOFT ORGAN |
| 161 | NIGHT DRONE |
| 162 | DUST BED |
| 163 | TAPE HISS |
