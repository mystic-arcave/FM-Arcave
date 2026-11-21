# ARCAVE LINK: several FM-1s on one clock

Each FM-1 keeps its own scene and its own tune. LINK keeps tempo, beat and bar the same on all of them.

## What every FM-1 has

One clock (`firmware/src/arcave/ac_synth.h`): ticks, 96 to a quarter note, counted in the audio interrupt from
power-on and never stopped. Every tune and every machine reads bar and step from it, so two FM-1s whose clocks agree
are in time whatever each is playing. On its own each jukebox tune has its own key; with LINK on, every tune plays in
the one shared key (C major pentatonic, A minor pentatonic for the minor ones), so they also agree in pitch, and the
keyboard follows.

## The messages

Plain control changes on MIDI channel 16 (status `0xBF`), because they pass everywhere: USB, the TRS jack, and
Bluetooth MIDI stacks that drop SysEx.

| CC | Value | |
| --- | --- | --- |
| 102, 103 | high 7 bits, low 7 bits | the tempo: BPM x 32 |
| 104, 105, 106 | high to low, 7 bits each | the clock: quarter ticks (384 to a beat), modulo 2^21 |
| 107 | bits 0-2: sender (1-4 an FM-1, 0 the hub); bit 3: "follow this"; bit 4: "you lead" | ends a message: the clock is that of the moment this byte arrives |
| 108 | n | a ping from the hub |
| 109 | n | the FM-1's answer, sent at once |

- An FM-1 with LINK on (GLO > LINK > 1-4) says where its clock is twice a second (102-107, no "follow" bit).
- The hub hears them, takes one as the leader, and sends each of the others the leader's clock **moved ahead by that
  FM-1's own delay** (half the shortest ping round trip), with the "follow" bit.
- A follower takes the tempo and slides its clock toward the leader's: a twenty-fourth of what is owed every 0.7 ms,
  never a jump unless it is more than two steps out. A message read late can only look early, so of four in a row it
  trusts the one asking for the most.
- No word from the hub for 4 s: the FM-1 runs on by itself at the tempo it has.
- A plain MIDI clock (`F8`, and `FA` for bar one) is followed too, when no hub speaks.

## The hub

`web/link/` (the Link page): Web MIDI, no SysEx. It keys FM-1s by port and number, shows each one's delay and how far
it is from the leader, and can lead from its own clock when no FM-1 should. `tests/emu_test.mjs` plays a hub against
the browser build: the follower lands within two ticks (about 12 ms at 100 BPM) in that test.

## Bluetooth: what was found, and what is not done

FM-ARCAVE 0.1 has **no Bluetooth**: Dinghy's platform has none. The messages above were chosen so that the day it has,
nothing else changes. What reading the open FM-1 Bluetooth code (Optimist's stack, as JIANT and fm1-base carry it)
showed:

- It is a BLE MIDI **peripheral** with **one link** at a time. A central role exists in the sources but is not built
  or proven on the device. So four FM-1s cannot connect to each other, and one FM-1 cannot be the hub for three.
- A hub is therefore needed in any case: a computer or phone that holds four BLE MIDI links (macOS and iOS do), running
  the Link page or the same logic.
- Incoming BLE MIDI drops SysEx and song position; only channel messages and `F8 FA FB FC` pass. Hence control changes.
- The 13-bit BLE timestamps are decoded and then ignored on the way in; using them would take the connection
  interval's jitter (7.5 to 11.25 ms) out of the hub's messages.
- The radio's start takes about 160 ms with interrupts off, and its interrupts rank below audio.

To bring it over: the stack's sources (`bt/ble/`, about 5,300 lines without the central role), the radio headers
(`fm1_ble.h`, `fm1_ble_rf.h`, `fm1_rf_cal.h`, the RF tables), a glue file after JIANT's `bt_seg.c`, the BLE interrupt
numbers in `fm1_irq.h`, and a path from the stack's receive callback into `midi_get`'s queue. About 33 KB of flash and
6 KB of RAM; both are there. It has to be tried on a device, with the boot guard JIANT uses for a radio that fails to
start. A connectionless variant (the leader advertises its clock, followers only scan) would need no hub at all and
is worth a look after that.

## UNDERSONG: the game for two

Down the well in the Arcave's floor (`firmware/src/arcave/ac_duo.h`) two FM-1s play one game: a story of four
chapters (ECHO STEPS, TWIN ROAD, PIPE WHISPER, TWIN DRUM), the same four again for the best score once the story is
told, and ROCK-OFF against each other. Each player sees the other's rock, keys and dial. LINK does not have to be on.

An FM-1 only sends MIDI over USB, so the two need something between them that passes each one's messages to the
other. Nothing else: the relay knows nothing of the game.

- **Two FM-1s:** both by USB on one computer, the Link page open and its hub started.
- **No FM-1:** the emulator page open in two windows of one browser. They find each other by themselves.
- An emulator with MIDI on and an FM-1 on the same computer meet through the Link page too (a virtual MIDI port).

The messages, control changes on channel 16 like LINK's own:

| CC | Value | |
| --- | --- | --- |
| 110 | n, 1-126 | "I am here", every 300 ms. n is drawn at the door; the lower number is seat 0 and leads. The same number twice: draw again |
| 112, 113 | key 0-26 | a key pressed, a key let go (113 with 127: all) |
| 114 | 0-11 | the pipe's dial (PIPE WHISPER) |
| 118, 115, 116 | c, a, b | the three numbers of a message |
| 117 | op | ends a message and names it |

| op | a, b, c | |
| --- | --- | --- |
| 1 hello | chapters cleared, state | with every 110 |
| 2 epoch | the leader's clock / 32, 21 bits (c high, a, b low) | LINK's clock is sent modulo 2^19 ticks; this puts the two in the same bar |
| 3 pick | the line of the list | the list is shared: either player turns it |
| 4 open | game, + 8 as the story's chapter; 127: closed | |
| 5 ready | 1 | |
| 6 go | game (+ 8), the beat it starts on (14 bits: b low, c high) | from seat 0, once both are ready |
| 7 hit, 8 miss | the note (a low, c high), 1 PERFECT or 2 GOOD | each FM-1 judges its own player and tells the other |
| 9 quit | 0: out of this game; 1: back up to the Arcave | |
| 10 stray | | ROCK-OFF: a key where no note was |

Seat 0 also says where its clock is four times a second: CC 102-106 as above, then CC 107 with value 13 (sender 5,
"follow"). Seat 1 follows it; the relay passes it on unchanged, with no delay taken off, so seat 1 runs behind by
the relay's delay. Each FM-1 judges its own player against its own clock, so that delay never costs a note; it only
shows as the friend's notes sounding that much late.

`tests/duo_test.mjs` plays the whole of it between two browser builds wired to each other. What has not been tried:
two real FM-1s on the Link page (its relay pairs each input with the output of the same name, in order).
