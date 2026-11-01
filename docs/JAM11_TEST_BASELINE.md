# jam11 Windows test baseline

Step 0, commit `aa55e22` (1.1 `974d875` + Windows build, fonts 8-slot, hot room, `jam/` tools), 2026-11-01, `sh jam/check.sh` with a 600 s limit per group, Git Bash + llvm-mingw. Every later step is compared with this list, not with "all green".

Summary: 47 passed, 63 failed, 11 skipped.

Why so many fail: most of upstream 1.1's own host tests don't compile against 1.1's firmware (they use names 1.1 removed, e.g. `CF_ERASE_PSLOT`, `ui.plk_src`, old argument counts) or check 1.0 behaviour. That is upstream's state at `974d875`, not a Windows problem. `jam_dev` fails on 1.0-format fixtures (End B block, the old music object) in its mock; the tool itself backed up and flashed the real 1.1 unit today. SKIP = needs `fork()`, which Windows lacks. `fm6` exceeds 600 s here.

## PASS

chord strum performcap dlymod hang atgt release menuproduct dx7compat splash hold reset resetwalk tcstage1 
bootguard tcbench poolrule polylimits deadline velomono sat eq32 ovlguard matrixpack engswitch guide 
guidesetup wtab wavetable fun10 audio erase flashled usb elfcheck plate chorusskip tune bench benchaudio 
rollbrowser sliders xb midimap midimapmut ldrscreen lcdboot 

## FAIL

velaccent mod perform perflevelmut ui select step3 engine morphui dlyev patslot multi patch patchraw bank 
dx7import mrender loadcost cpuscen uf pan twocore tcmeas mxstep modinert eqon level modules uitest project 
motion persist backup editor editorjson midictl reverb halltail locut shimmer panic insert bassmono comp 
master stereochorus sidechain benchsim benchrun ownerimport xbmut fuzz xvalues twin subcyc pages scope2 editb 
velo emu converttwin jam_dev 

## TIMEOUT

fm6 

## SKIP

regress perflevel morph mode sounds fmbass wave mixsplit partindep duckpart budget 

