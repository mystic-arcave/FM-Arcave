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

## Bluetooth

An FM-1 running FM-ARCAVE can be a **Bluetooth LE MIDI peripheral**: a computer or phone connects to it as to any
BLE-MIDI instrument, and to several FM-1s at once. Everything above and below (LINK's clock, UNDERSONG's messages)
then goes over Bluetooth as it goes over USB, both ways: `midi_send` sends to USB and to the Bluetooth link, and what
arrives over Bluetooth comes out of `midi_get` (as source 3). The messages were chosen for this: plain control
changes, which a BLE-MIDI link passes (it passes channel messages and `F8 FA FB FC`; no SysEx, so the installer
stays on USB).

- **Off until it is turned on.** The game keeps ON or OFF in its own save and tells the platform (`bt_set`,
  `firmware/src/bt.c`); with it off the radio is never touched and the FM-1 starts exactly as before.
- **The name** is `FM-ARCAVE XXXX`: XXXX are the last four hex digits of the FM-1's own address, made once at the
  first ON and kept, so two FM-1s side by side can be told apart. The service is the standard BLE-MIDI one
  (`03B80E5A-EDE8-4B33-A751-6CE34EC4C700`, its MIDI I/O characteristic `7772E5DB-3868-4112-A1A9-F2669D106BF3`:
  `firmware/bt/ble/ble_host.h`, `ble_att.c`).
- **One link per FM-1**, and the FM-1 never connects out: the hub is still a computer or phone (the Link page), which
  holds a link to each FM-1.
- **Pairing**: none is asked for. A computer that insists on pairing (Windows, some Macs and Androids) gets Just Works
  pairing, no PIN.
- **The first ON restarts the FM-1 once.** The radio needs the FM-1's supplies set the way the stock firmware sets
  them (`firmware/hal/fm1_power.h`), and that is done at power-on, before the sound and the screen start, only when
  Bluetooth was ON the last time. So the first time ON is chosen, the FM-1 restarts itself three seconds later and
  comes up with Bluetooth on; from then on it is simply on at power-on (the radio starts three seconds in, with about
  a third of a second of silence while it calibrates itself).
- **The guard.** If the radio's start hangs or crashes, the FM-1 resets once and comes up with Bluetooth off and
  FAILED in its place; it does not try again by itself (`firmware/src/bt.c`, the boot guard; `tests/bt_guard_test.c`
  plays every case on a computer).
- **Timing.** A BLE link passes MIDI once per connection interval (7.5 to 11.25 ms is what the FM-1 asks for), so a
  message is up to that much later than over USB, unevenly. LINK's hub already measures each FM-1's delay by its
  pings and takes it off; UNDERSONG judges each player on their own FM-1. Outgoing messages carry BLE-MIDI
  timestamps; incoming timestamps are not used.

What it is made of: Optimist's BLE stack (Henri David) with the WL82 radio driver, the copy JIANT FM ships on FM-1s
(`firmware/bt/ble/`, the peripheral's part only), JIANT's unit around it (`firmware/bt/bt_radio.c`) and the
platform's side after JIANT's (`firmware/src/bt.c`). It is linked into the app (about 33 KB of flash, under 5 KB of
RAM), compiled as a unit of its own (`tools/build.py`: `build/bt.o`). Where JIANT keeps the stack in a flash segment
of its own outside a full app slot, FM-ARCAVE's slot has the room. The SMK-37 build has no Bluetooth.

**Not yet tried on an FM-1.** It builds, and the parts with no hardware in them are tested on a computer
(`sh tests/run_bt_tests.sh`: the BLE-MIDI packets, the rings, the platform's rules). That the radio starts, advertises
and holds a link beside FM-ARCAVE's sound and screen, only an FM-1 can show. `DINGHY_DEFS=-DFELUCCA_BT_TEST=1
./build.sh` makes a build with Bluetooth on from power-on, to try the radio before anything else.

## Bluetooth on the Link page

Both tabs of the Link page take an FM-1 over Bluetooth as they take one over USB, in any mix. The shared code is
`web/link/ble.js`; `tests/ble_web_test.mjs` tries all of it without an FM-1.

### Two ways in

- **From the page (Web Bluetooth, Chrome or Edge):** ADD AN FM-1 BY BLUETOOTH opens the browser's list, one FM-1 a
  press (the browser asks each time), any number at once. The list shows devices that advertise the BLE-MIDI service
  (`03B80E5A-EDE8-4B33-A751-6CE34EC4C700`) under a name that starts with `FM-ARCAVE`; each FM-1 adds four characters
  of its own (`FM-ARCAVE 3F2A`), which is how two are told apart. If the computer still lists an FM-1 under an older
  name, NOT IN THE LIST? SHOW EVERY BLUETOOTH MIDI DEVICE drops the name from the filter. The page then reads and
  writes the MIDI I/O characteristic (`7772E5DB-3868-4112-A1A9-F2669D106BF3`: notifications in, writes without
  response out). Safari and Firefox have no Web Bluetooth: the button is greyed there and the page says so.
- **Through the computer's own Bluetooth MIDI** (on a Mac: Audio MIDI Setup, Bluetooth): the FM-1 then shows up as a
  MIDI port, and START THE HUB or USE MY FM-1 finds it like a USB one, in any browser with Web MIDI.

A Bluetooth FM-1 is a port like any other to the hub: LINK's clock and pings, UNDERSONG's relay (`undersong.js`'s
Gather), the online lobby. To the lobby server an FM-1 by Bluetooth is `usb`, its word for "an FM-1 plays here":
the server is unchanged.

### The packets

BLE-MIDI 1.0, the same rules as the firmware's codec (`firmware/bt/ble/ble_midi.c`): a header octet with the time's
high bits, then for each message a time octet and the message, a repeated status left out (running status) with its
time octet kept. Incoming packets are read whole: several messages a packet, running status with or without its time
octet, the time's high part stepping inside a packet, real time anywhere, SysEx followed to its end and left out.

Outgoing, the page is never told the MTU, so it never writes more than 20 octets (the smallest MTU, 23, less 3).
That is exactly six control changes on one channel: LINK's clock (102 to 107) is one write, and so is an UNDERSONG
message (118, 115, 116, 117) and the game leader's clock. What one `send` holds is kept in one packet whenever it
fits; a longer one is cut between two messages, never inside one. A browser refuses a second write while one is under
way, so each FM-1 has a queue: one write at a time, and what was said meanwhile goes together in the next, in order.
Channel messages and real time only, as the firmware takes nothing else over Bluetooth.

### Delay

A Bluetooth message waits for the two radios' next meeting (the connection interval, 7.5 to 15 ms on a computer,
sometimes more), so both directions are later than USB and not by the same amount twice. What the hub already did
for late messages covers it: of an FM-1's last six reports it goes by the one least held up, and the firmware trusts
the one of four hub messages that asks for the most. For Bluetooth ports the hub also pings twice as often (once a
second) and keeps the quickest of sixteen round trips instead of eight; the delay starts from 12 ms until the first
answers are in. The delay is still taken as half the quickest round trip, which over Bluetooth can be a few
milliseconds more than the quickest one-way trip: a Bluetooth follower may sit that much ahead. The BLE-MIDI time
octets are decoded but not used; using them to take the interval's jitter out is the next refinement, and needs
numbers from real FM-1s first.

### A broken link, and an FM-1 that is in twice

- A link that drops shows LINK LOST: TRYING AGAIN in the list; the page reconnects by itself (after 0.5, 1, 2, 4, then
  every 8 s) and the FM-1 keeps its row. REMOVE lets it go.
- One FM-1 heard on two ports (USB and Bluetooth, or Bluetooth through the computer's MIDI and through the page at
  once) would count as two players and be sent its own messages back. `Twins` in `ble.js` tells by content: the whole
  clock an FM-1 reports (tempo, position, sender) and its "I am here" (110) down the well. The same one on two ports
  within 0.4 s, four times running: one FM-1. The cable is kept before the air, the computer's MIDI before the page's
  link; the other port is a shadow: nothing it says is used, nothing is sent to it, and the page says THE SAME FM-1
  AS ... : COUNTED ONCE. Three reports running with no match (the cable pulled) and the shadow counts again.
- Two USB FM-1s carry the same port name. Inputs and outputs of one name are paired in order, as before; names that
  differ only by IN and OUT now pair too. Each ping now carries its own number, so an answer that comes in on
  another input than expected shows the output was the other one's, and the LOCAL hub swaps the pair. That needs
  LINK on (an FM-1 answers pings only then).

### What is proven, and what only FM-1s can show

`tests/ble_web_test.mjs` proves, in Node: the codec (round trips, and both ways against the firmware's own C codec
when it is in the tree or `BLE_MIDI_C` points at it); and the two pages' own scripts, run behind a made-up
`navigator.bluetooth` and Web MIDI with made-up FM-1s: two by Bluetooth take the page's clock, never two writes at
once and none over 20 octets, UNDERSONG's messages whole, in order and once, a link coming back, an FM-1 in twice
counted once, crossed USB pairs put right, the online lobby both ways.

Not proven, because it needs real FM-1s and a real radio: that the browser's list shows them (the firmware must put
the service in its advertisement and the name in its scan response, as the stack does); how long the real round
trips are and how steady; two or more real links at once on one computer; a computer-connected and a page-connected
link to the same FM-1 at once; Windows, where port names and Bluetooth MIDI differ; Chrome on Android.

## UNDERSONG: the game for two or three

Down the well in the Arcave's floor (`firmware/src/arcave/ac_duo.h`) two or three FM-1s play one game: a story of four
chapters (POTION CHOIR, TWIN ROAD, ECHO STEPS, NOTE RALLY), the same four again for the best score once the story is
told (TWIN ROAD then on any of five songs), and ROCK-OFF against each other. Each player sees the others' rocks, keys and dials. LINK does not have to be on.

With three in the well every game is its version for three: in ECHO STEPS the caller changes round the three and the
other two echo together; TWIN ROAD's song is split three ways, its big notes for all three; in POTION CHOIR one pours
and the other two each see half of the rock that needs it (one its shape and eyes, the other its colour, hat and
mouth); NOTE RALLY's note goes round the three; ROCK-OFF is all against all. Games for three keep their own best
scores. Whoever is in the well while the list of games is up plays; a fourth waits at the door. If anyone playing is
not heard from for two and a half seconds, the game stops on the others' screens with an error that names who left;
PLAY there goes on with whoever is still there.

An FM-1 only sends MIDI over USB, so they need something between them that passes each one's messages to the
others. Nothing else: the relay knows nothing of the game.

- **Two or three FM-1s:** all by USB on one computer, the Link page open and its hub started.
- **No FM-1:** the emulator page open in two or three windows of one browser. They find each other by themselves.
- An emulator with MIDI on and an FM-1 on the same computer meet through the Link page too (a virtual MIDI port).

The messages, control changes on channel 16 like LINK's own. All but 110 carry the seat s of the one who sends it
(0, 1 or 2), so each FM-1 knows whose they are:

| CC | Value | |
| --- | --- | --- |
| 110 | n, 1-126 | "I am here", every 300 ms. n is drawn at the door; the lowest number is seat 0 and leads, the next seat 1, the next seat 2. The same number twice: draw again |
| 112, 113 | key 0-26, + 32 s | a key pressed, a key let go (113 with 127: all) |
| 114 | 0-11, + 16 s | POTION CHOIR: the rock the potion is over (0-7); NOTE RALLY: the lane your rock is in (0-3) |
| 118, 115, 116 | c, a, b | the three numbers of a message |
| 117 | op + 16 s | ends a message and names it |

The Link page holds 118, 115 and 116 until their 117 comes, then passes the four on together: with three FM-1s, the
messages of two players must not mix on the way to the third. (The emulator's windows send what each one says in
one piece anyway.)

| op | a, b, c | |
| --- | --- | --- |
| 1 hello | chapters cleared, state | with every 110 |
| 2 epoch | the leader's clock / 32, 21 bits (c high, a, b low) | LINK's clock is sent modulo 2^19 ticks; this puts all in the same bar |
| 3 pick | the line of the list | the list is shared: any player turns it |
| 4 open | game, + 8 as the story's chapter; 127: closed | |
| 5 ready | 1 | |
| 6 go | game (+ 8; + 16 times the song, TWIN ROAD), the beat it starts on (14 bits: b low, c high) | from seat 0, once all are ready |
| 7 hit, 8 miss | the note (a low, c high), 1 PERFECT or 2 GOOD | each FM-1 judges its own player and tells the others |
| 7 hit, 8 miss | NOTE RALLY: a the lane it flies to (drawn at random by the one who hit it) + 4 times how well (0 a serve, 1 PERFECT, 2 GOOD), the beat it lands on (b low, c high); miss: it fell | from the one who hit, or dropped, it; the note goes to the next seat (seat 2 to seat 0) |
| 7 hit, 8 miss | ONE VOICE (game 5, two players): the wall (a low, c high); hit: your half of it was right, miss: wrong | a wall rings only when both halves were right. How high the Hum is and its shape are the key held (CC 112, 113) and the dial (CC 114: 0 round, 1 square, 2 spiky) |
| 9 quit | 0: out of this game; 1: back up to the Arcave | REC twice in a game sends 1: the others see that player leave |
| 10 stray | | ROCK-OFF: a key where no note was |
| 11 song | the song, 0-4 | TWIN ROAD's card, the story told: turned to another song (nobody is ready again) |
| 12 pour | the round, the rock (127: none, the time ran out), the bars it took (1-8) | POTION CHOIR, from the one who pours: the next round starts one bar after that bar ends |

Seat 0 also says where its clock is four times a second: CC 102-106 as above, then CC 107 with value 13 (sender 5,
"follow"). Seats 1 and 2 follow it; the relay passes it on unchanged, with no delay taken off, so they run behind by
the relay's delay. Each FM-1 judges its own player against its own clock, so that delay never costs a note; it only
shows as the friends' notes sounding that much late.

`tests/duo_test.mjs` plays the whole of it between two browser builds wired to each other, then all five games for
three between three, and one of the three going dark in the middle of a game. Two real FM-1s have played it on the
Link page; three real FM-1s have not been tried yet.

## The music room, together: up to four in one room

`firmware/src/arcave/ac_roomnet.h`. Whoever is in their music room, with TOGETHER on in the room's settings (it is on
unless turned off), shares one room with everyone else who is, through the same relay as UNDERSONG: the Link page
(LOCAL or ONLINE), or emulator windows in one browser. There is nothing to press: the Link page is the door.

- **One room.** It is the room of whoever was there first. Someone coming in is sent all of it (TUNING IN, about five
  seconds), and their own room stays as it was in their save: it is theirs again the next time they come in alone,
  or as soon as they turn TOGETHER off. SAVE in a friend's room keeps that room as one's own.
- **One player to an instrument.** An instrument, or the pool, belongs to one player at a time. In the room their
  Rock sits over it with their name, and PLAY and its button do nothing for the others until it is free. Two at the
  same moment: the lower seat stays. The quick mix (GLO held) is everyone's.
- **Watching, and swapping.** GLO held and a station's button goes straight onto it; onto a friend's, to watch it:
  nothing there answers but HOME. The one watching is told whose it is, the one playing who watches, and a strip on
  both screens says so. OCT- and OCT+ pressed together by both within 1.5 s and they swap. The one playing decides
  (it has seen the watcher's ask, op 20 with b 0, and its own two buttons) and says so (op 20 with b 1), so the two
  FM-1s never disagree. When the player leaves, the one watching plays.
- **What is sent is not sound.** Every FM-1 plays the whole room itself, on seat 0's clock. What the players change
  is sent: each FM-1 compares the room with a copy of what it last told or heard and sends the bytes that differ, a
  few a frame. So laws, layouts, the mixer, scale and tempo are the same everywhere and the steady devices fall on
  the same beats; the loose parts of each world (stones, pods, shards) fall as each FM-1 has them. Keys played by
  hand are sent as keys. What changes by chance (a layout tending itself, the pool's dives) is drawn by one FM-1
  only: the one whose player is on that instrument, else the host (the lowest seat that has the room). That one also
  records the instrument onto the loop and tells the rest each hit; playing together, each instrument has a quarter
  of the loop's 256 places.
- **I HEAR: MY OWN** (the room's settings): an FM-1 sounds only the instrument its player is on, for players in one
  room with a loudspeaker each.

The messages, control changes on channel 16:

| CC | Value | |
| --- | --- | --- |
| 20 | n, 1-127 | "I am in my music room", every 300 ms. n is drawn on coming in, after 600 ms of listening, above every number heard: seats are the numbers in order, so nobody's seat moves when someone joins |
| 21, 22, 23 | a, b, c | the three numbers of a message |
| 24 + s | op | ends a message from seat s (0-3) and names it |

The Link page holds 21, 22 and 23 until their 24-27 comes and passes the four on together (`web/link/undersong.js`).

| op | a, b, c | |
| --- | --- | --- |
| 1 hello | the number of the room held (0: none yet), station sat on (0-5) + 8 if the room is settled | with every 20 |
| 2 epoch | seat 0's clock / 32, 21 bits (c high, a, b low) | as UNDERSONG's; seat 0 also sends LINK's clock as sender 5 |
| 3 sit | station played 0-5 (0: none; 1 ROOTS, 2 THE GEODE, 3 THE POOL, 4 MUSHROOM RACK, 5 CRYSTAL SEAM), station watched 0-5 | when either changes, and every second while watching (an FM-1 before 0.29 sends b 0) |
| 4 pos | x / 8, y / 2, (x mod 8) * 16 | Rock loose in the room, when he moves |
| 5 look, 6 name | as UNDERSONG's 14 and 15 | in turn with the hello |
| 7 poke | byte number / 8, (number mod 8) + 8 * (value / 128), value mod 128 | one shared byte changed here. The bytes: each instrument (157), the four mixer strips (32), the pool (7), the room (scale, root, loop bars, sieve, echo time, echo lasts, tempo, loop rests, REC) |
| 8 scrub | as poke | the host, four a second, round and round: mends a byte lost on the way (not taken within a second of a change made here) |
| 9 key | instrument + 4 if the top row, the key's place in its row | a key played by hand |
| 10, 11 | a hit recorded onto the loop, in two halves | from whoever records that instrument |
| 12 wipe | instrument 0-3, or 4: all | a part of the loop wiped |
| 13 need | the number asked | "send me the whole room" |
| 14 snap0 ... 18 done | the count; pokes (15); loop hits (16, 17); the room's number | the whole room, six messages a frame, to whoever asked |
| 19 bye | | leaving, or TOGETHER turned off |
| 20 swap | station, 0, 0: "OCT- and OCT+, from the one watching it"; station, 1, seat: "seat plays it now, I watch it" | the first from the one watching, the second from the one playing |

Everything that comes in is brought into range before it is believed (`rn_sane`). A lesson (HOME) has the room to
itself on the FM-1 it plays on: what the others change meanwhile is put in when it ends.

Tested by `tests/room_test.mjs` (four browser builds through undersong.js's gathering, at 0 and 150 ms each way).
Not yet tried between real FM-1s.

## Online: UNDERSONG between computers

The Link page has two tabs. **LOCAL** (`web/link/`) is the hub above, for FM-1s on one computer. **ONLINE**
(`web/link/online/`) puts this computer into a private lobby with up to three others, anywhere (UNDERSONG is for two or three of them; the
music room for all four):

- **What plays here:** every FM-1 plugged in by USB (USE MY FM-1, Web MIDI) and every emulator window open in the same
  browser (OPEN THE EMULATOR; they speak on the BroadcastChannel `fm-arcave-link`). Any mix at either end.
- **The lobby:** MAKE A PRIVATE LOBBY gives a code of six letters and numbers (no I, O, 0 or 1); JOIN takes one, or a
  link that ends in `#CODE`. Lobbies are never listed. Four computers at most; an empty lobby closes after half an
  hour; a page whose connection breaks joins again by itself and keeps its place.
- **What passes:** UNDERSONG's messages only, gathered as the LOCAL hub gathers them (`web/link/undersong.js`: 118, 115
  and 116 held for their 117; the leader's clock in one piece). From an FM-1 here they go to the lobby and to the
  others here; from the lobby to every FM-1 here that is down the well, and to the emulator windows. What the page
  passes to the windows is marked (`{ relay, m }`), so another Link tab never sends it back out.
- **One Link tab at a time** passes UNDERSONG on in a browser (a Web Lock, `fm-arcave-link-hub`): the LOCAL hub and
  an ONLINE lobby together would pass every message twice.

Nothing is corrected for the distance: the leader's clock goes on as the local relay passes it, so a follower runs
behind the leader by the time a message takes, and each FM-1 still judges its own player against its own clock.
Games start five or six beats after the leader's "go", NOTE RALLY's note lands four beats after it is hit, and a
player is only given up after 2.5 s of silence, so a few hundred milliseconds cost nothing but late-sounding friends.

### The lobby server

`server/lobby.mjs` holds the rules; two hosts run them:

- `server/worker.mjs` with `server/wrangler.toml`: a Cloudflare Worker, one Durable Object per lobby
  (`cd server && npx wrangler deploy`), live at `wss://fm-arcave-lobby.mystic-arcave.workers.dev` on the project's
  Cloudflare account (free plan). That address is `SERVER` in `web/link/lobby.js`.
- `server/node.mjs`: the same on Node with nothing to install (`node server/node.mjs 8787`), for trying it on one
  computer and for the tests. The Online page takes `?server=ws://localhost:8787`, and a link it copies carries that on.

JSON text frames over a WebSocket at `/new` (make a lobby) or `/join/CODE`:

| from | message | |
| --- | --- | --- |
| page | `{t:"hi", name, token}` | first; the token is the same when the page comes back after a break |
| server | `{t:"lobby", code, you}` | in; `you` is this page's number in the lobby |
| server | `{t:"err", why}` | `no-lobby`, `full`, `bad-code` or `busy`; then the socket closes |
| page | `{t:"m", d:[cc, v, ...]}` | control changes on channel 16 for everyone else |
| server | `{t:"m", f, d}` | the same, from member `f` |
| page | `{t:"ping", n, rtt, kind, well}` | every 2 s: the last round trip, what plays here (`usb`, `emu`, `usb+emu`), whether one is in the well |
| server | `{t:"pong", n}`, `{t:"roster", code, members}` | the answer; who is in, whenever that changes |

A member not heard from for 15 s is dropped. `tests/online_test.mjs` runs the Node server and three browser builds
behind the Online page's own code, with 80 ms each way: a lobby made and joined, a wrong code and a fourth computer
refused, TWIN ROAD and NOTE RALLY for two, a connection broken and back, ECHO STEPS for three, and one leaving the
lobby mid-game. With `SERVER=wss://fm-arcave-lobby.mystic-arcave.workers.dev DELAY=0` it runs the same against the
live server, over the real internet. No real FM-1 has played online yet.

## PLAY WITH MY FM-1: the game in a browser, played on a real FM-1

The PLAY page (`web/emu/`) has a PLAY WITH MY FM-1 button (not the Link page's USE MY FM-1 (USB), which adds an FM-1
to the hub). With an FM-1 plugged in by USB, switched on and running this
version of FM-ARCAVE, the page runs the game and shows it; the FM-1 becomes its controller:

- the FM-1 sends the save it holds, and the page starts the game from it (this browser's own save is left alone);
- every button, key and knob of the FM-1 plays the game on the page, together with the page's own panel and keyboard;
- the game's lights light the FM-1's keys and buttons as they would on the FM-1 itself;
- every save the game makes goes back to the FM-1 and is kept in its flash, so the FM-1 carries on from there.

Meanwhile the FM-1 shows a screen of its own (ON THE COMPUTER, Rock under the computer, in its language) and makes no
sound: the sound is the computer's. Press the button again (FM-1: ON) and the page first saves the game as it is
that moment (not waiting for the game's own next save, which can be a second and a half away, or a quarter of an hour
for the time played), sends that save and waits until the FM-1 says it is kept (SAVING ON YOUR FM-1..., a few seconds
by Bluetooth); only then does it say goodbye, and the FM-1 is itself again, as after a power-on, with that save. A
goodbye sent at once would cut a save still on its way, and what was just played would be lost. Close the page or
unplug the FM-1 instead and within three seconds it is itself again with the last save it had kept. The page then
goes back to the game saved in the browser.

Both ways a loading screen covers the FM-1's picture on the page while the save is on its way: READING YOUR FM-1'S
SAVE when it connects, SAVING WHAT YOU PLAYED ON YOUR FM-1 when it is let go, a strip of 32 lights filling from where
the save is to where it goes, with how much of it has gone (by Bluetooth, as much as the FM-1 says it has read: CC 69).
Going back, the strip is full while the FM-1 writes it to its flash (YOUR FM-1 IS KEEPING IT), then KEPT ON YOUR FM-1.
The page's side: `remote.js`'s `progress()`.

Like everything else here it is control changes on channel 16, so it would pass through anything that carries MIDI,
and the browser build can play the FM-1's part in the tests. The firmware's side is `firmware/src/arcave/ac_remote.h`,
the page's `web/emu/remote.js`:

| CC | way | value |
|----|-----|-------|
| 78 | to the FM-1 | hello, every half second: the page's number, 1-127 (a new number: the save is sent again); 0 goodbye |
| 70 | from it | here, the answer to every hello: the protocol's version (2; 1 before 0.25, without CC 69) |
| 69 | from it | how much of a save coming in it has read: 0 when its length is read, then after every 16th CC 76 the number of sixteens (mod 128). By Bluetooth the page keeps no more than 40 CC 76 beyond it |
| 71 | from it | a button (`BTN_...`), + 64 pressed |
| 72 | from it | a key (0 the lowest), + 64 pressed |
| 73 | from it | a knob: its number (`ENC_...`) × 16 + 8 + the detents turned (-8..7, never 0) |
| 74, 75 | both ways | a save follows: its length in bytes, high 7 bits, then low |
| 76 | both ways | its bytes, seven at a time: their top bits first (bit i: the group's byte i), then each byte's low 7 |
| 77, 77 | both ways | its end: the sum of its bytes (14 bits), high, then low |
| 79 | from it | what became of the page's save: 1 kept, 2 not this FM-1's size (it needs the page's version), 3 damaged |
| 80-92 | to the FM-1 | the lights, 7 bits each: lit buttons (bit 14 PLAY lit, 15 PLAY breathing) in 3, lit keys in 4, glowing keys in 4, breathing buttons in 2 |

The save is 3404 bytes (with the music room and the slots of its places), about 3900 messages. The FM-1 sends four a millisecond at most (its USB
ring holds 64 and is emptied at 2 kHz); the page sends two a millisecond by Web MIDI's own timestamps, and the FM-1
draws nothing while one comes in, so its ring of 64 coming in is read often enough. A save of another length is
refused (the FM-1 asks to be updated, on its screen; the page says the same), and a damaged one is never written.
The page needs Web MIDI: Chrome and Edge have it, Safari does not.

`tests/remote_test.mjs` runs all of it without an FM-1: one browser build is the FM-1, another the page's game, the
page's `remote.js` between them on a made-up MIDI port that keeps to Web MIDI's timestamps. What only a real FM-1 can
show: that its USB port is the one that answers (the page takes the first port to answer and the output of the same
name), that the flash write of a 3404-byte save while the page goes on sending lights loses nothing, and how quick the
keys feel through the computer.

### The same link for the SAVES and MACHINES pages

Two more pages say hello to an FM-1 the way the PLAY page does, and let it go again without playing:

- **SAVES** (`web/site/save`, `web/emu/saves.js`) takes the save the FM-1 sends on hello and keeps it as a backup; to
  put one back it sends that save (CC 74 to 77) and says goodbye once the FM-1 has answered CC 79 1. Nothing new on
  the FM-1's side, so it works with every FM-ARCAVE that has PLAY WITH MY FM-1.
- **MACHINES** (`web/site/machines`, `web/emu/machines.js`) needs protocol 3 (the FM-1's CC 70 says so): CC 60 to 63
  read the ledges' slots and load a machine into the bank. [MACHINES.md](MACHINES.md) has the messages.

### By Bluetooth

PLAY WITH MY FM-1 (BLUETOOTH), beside it, does the same over Bluetooth LE MIDI, straight from the page (Web
Bluetooth, Chrome or Edge, through `web/link/ble.js` as the Link pages use it). Bluetooth must be ON in the FM-1's
settings (GLO; the first time, the FM-1 restarts once). The button opens the browser's list of FM-1s (`FM-ARCAVE` and
four characters of the FM-1's own); the one chosen is connected and then asked for its save, exactly as by USB.
`remote.js`'s `bleAccess` makes the Bluetooth FM-1 look like Web MIDI to the rest. An FM-1 the computer itself has
connected as a Bluetooth MIDI device (Audio MIDI Setup on a Mac) is a MIDI port, and the plain PLAY WITH MY FM-1 finds
it like a USB one.

The messages are the same; only the pace differs. Bluetooth passes MIDI once a connection event (7.5 to 15 ms, more
on some computers), a dozen or so messages each way, so:

- the FM-1 sends its save only as fast as its Bluetooth queue of 64 empties (`bt_tx_room`), sixteen places always
  kept for the answers to hellos, the panel and a save's result: the queue never overflows, whatever the computer's
  interval. On a slow link the save takes about four seconds;
- the page sends its saves seven packets (42 messages) at a time, the next only once the last is written, so a hello
  or the lights never wait more than a few events behind a save and the page's write queue never drops anything. A
  newer save takes the place of one not yet all out. On a slow link a save takes about eight seconds; the FM-1 shows
  SAVED when it is in;
- and no further ahead than the FM-1 says it has read (CC 69): 40 of the save's messages at most, its ring coming in
  holding 64. "Written" only means the computer took it: a Mac takes writes at once and sends six packets an event
  or more, which overran that ring and every save came in damaged (0.24, on a real FM-1). An FM-1 from before 0.25
  says nothing, so the page sends it 12 messages each 20 ms.

`tests/remote_ble_test.mjs` plays a slow link (15 ms events, 20-octet packets, two notifications an event from the
FM-1, one write from the page) through the real packet code on both sides: both saves arrive whole, nothing is
dropped, no hello is missed. `remote_ble_test.mjs mac` plays it as a Mac does: writes taken at once, six packets an
event, the FM-1's ring of 64 read every 4 ms but now and then not for 40 ms. Without CC 69 the page's save never
arrives whole there. On a real FM-1 (0.24, a Mac): connecting and the FM-1's save work; the page's saves did not.
