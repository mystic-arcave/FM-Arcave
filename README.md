# FM ARCAVE

**The Mystic Arcave on the M-VAVE FM-1: a music game you play with the knobs and the keys.**

<p align="center"><img src="docs/screen.png" width="240" alt="FM ARCAVE's title: a wizard's hat with one eye over the name"></p>

Wake up Rock, roll down to the cave, and mend the jukebox with a song of your own. Then the arcade is yours: three
machines, free play, and a jukebox on the ALGORITHM knob.

- **The page:** <https://mystic-arcave.github.io/FM-Arcave/>
- **Play it in the browser:** [the emulator](https://mystic-arcave.github.io/FM-Arcave/emu/), the firmware's own code built for the web.
- **Install it on an FM-1:** [the installer](https://mystic-arcave.github.io/FM-Arcave/install/), over USB from Chrome or Edge.
- **Several FM-1s in time:** [the Link page](https://mystic-arcave.github.io/FM-Arcave/link/), see [docs/LINK.md](docs/LINK.md).

> **Status: 0.2.** 0.1 was installed from the installer page and played on a newer FM-1 (the one with M-VAVE printed
> under the screen) on 17.11.2026. 0.2 adds the text sizes, PLAY's green light and NIGHT BLOOM's rings arriving one by
> one; it is tested in the browser build and in scripted runs, not yet on the device. Installing it is at your own
> risk; the way back is any other firmware's installer, or OCT- held at power-on (the USB rescue).

## The game

| | |
| --- | --- |
| **The intro** | The title, then "press PLAY to wake up Rock". Any of the seven knobs rolls the rock; the keys jump, far left a hop, far right the biggest. Launch pads, jelly caps, star rings, and a secret behind the start. |
| **The cave** | A platforming hub with no music, only a low hum and the sounds Rock makes. Both ends of the floor are ramps with a booster; springy caps and the boosters reach three things hidden high up. Someone calls from off the screen: the Hat, a wizard's hat with no body. The jukebox that powered the three arcade machines has gone out. |
| **The mending** | Three rings of glass, each with a crack: KNOB 1, 2, 3 turn them until the light gets out. Then the dish: sweep the keys (or turn a knob) to spin it, hold a speed for eight beats and that is your tempo. REC, and two bars of keys are your first song. The power comes back with it. |
| **Free play** | ALGORITHM changes the tune; a banner slides in from the top with its name. The machines: **MYSTIC RUN** (hop crystals on the beat), **THE LIGHT TABLE** (three rings under a needle, an instrument), **NIGHT BLOOM** (new: four knobs turn four petals, a click a twenty-fourth of a turn; catch each spore on its own colour). |

Everywhere: **GLO** opens the settings over the paused game (language, text size, look, LINK, about, replay the intro, start
over); held for a second it warns, and held three seconds more it wipes the whole save. **OCT-** and **OCT+** step through English, German, Danish, French and Spanish. **HOME** leaves a machine.

## Build it

```
python3 tools/gen_tables.py && python3 tools/gen_course.py   # the tables (already in firmware/gen)
tests/run_arcave.sh intro            # the game on this computer: build/host/intro.png (screens), intro.wav
web/emu/build.sh 0.1                 # the browser build (needs Emscripten's emcc)
./build.sh                           # the FM-1 package build/fm-arcave.fwsc (toolchain: see docs/MAKE-IT-YOURS.md)
python3 tools/make_site.py 0.1 build/fm-arcave.fwsc    # the whole site in build/site
sh tests/run_arcave_tests.sh         # every test; none needs an FM-1
```

The chip has no floating point and no 64-bit division, so the game is whole numbers throughout
([docs/RULES.md](docs/RULES.md)). The picture is drawn into RAM and only the rows that changed go to the screen.

```
firmware/src/arcave/     the game: ac_base.h (drawing, font, input), ac_synth.h (voices, echo, the clock),
                         ac_music.h (tunes), ac_text.h (five languages), ac_intro.h, ac_cave.h, ac_juke.h (rings, dish),
                         ac_mach.h (three machines), ac_sync.h (LINK), ac_menu.h, arcave.c
firmware/src/fm_arcave.c the FM-1 app: Dinghy's platform under the game
firmware/src/, hal/      Dinghy's platform (updates, USB rescue, MIDI, audio, keys, screen, storage); dinghy.c is its sample app
web/emu/                 the browser FM-1 (web_host.c, worklet.js, panel.js)
web/site, install, link  the pages; tools/make_site.py puts them together
tools/gen_course.py      the intro's land and everything on it, worked out from the prototype's formulas
tests/scripts/           scripted playthroughs; tests/run_arcave_tests.sh runs them all
```

## Releasing

A tag `vX.Y` builds the package, runs the tests and publishes a release with a build attestation
(`.github/workflows/release.yml`). Every push to `main` publishes the site with the emulator
(`.github/workflows/pages.yml`; set Settings > Pages > Source to "GitHub Actions" once). The package's identity is
`FM-1_96N`; pick another in `tools/build.py` if the [Firmware Hub](https://thefirmwarehub.com) lists that one.

## Credits

FM ARCAVE is by benjamuels (Mystic Arcave), from the Mystic Arcave installation. It is built on [Dinghy](https://github.com/zvenson/dinghy)
by zvenson, which carries the platform of [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (Hügelton
Instruments) as [SLOOP](https://github.com/isod89/sloop-fm1) (isod89) has it. The rings and the dish are after UMBRA
OBSCURA. Everything is GPL-3.0-only; [LICENSING.md](LICENSING.md) says who made what.

FM ARCAVE is not made or endorsed by M-VAVE.
