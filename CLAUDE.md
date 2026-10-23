# MIDIPlayer hybrid core for MiSTer (MIDI Player)

@hybrid/CLAUDE.md

## This core

- Name everywhere: `MIDIPlayer` (the engine: core, folders `games/dnk/MIDIPlayer/` and `logs/dnk/MIDIPlayer/`, `_Other/dnk/dnk-MIDIPlayer_*.rbf`, release), `midiplayer` (the program's folder, binary in `build/`, log, lock). The games and their menu entries (`_Other/dnk-<title>.mgl`): `package/games.txt` (in the zip as `games/dnk/MIDIPlayer/games.txt`, from which the database's build makes the launcher's catalogue).
- The program is ours, in `midiplayer/` of this repository (no submodule, no SDL2): C against the `MH_` library. The spec is `hybrid/docs/midiplayer.md`.
- Build: `./build.sh` (game), `./core/build_core.sh` (FPGA), `./package.sh` (zip in `dist/`), `MISTER=root@<address> tools/deploy.sh`.
- Release: the zip in `releases/`. Players get it once it is published to the database, only when Danik asks: `hybrid/launcher/distribution.py publish ~/dnk_Distribution_MiSTer releases/MIDIPlayer_YYYYMMDD.zip` (the rule in `hybrid/CLAUDE.md`).
- Try a build on the MiSTer without installing it: `MISTER=root@<address> tools/test_on_mister.sh <id>|list|nodata|log|stop` (the rule in `hybrid/CLAUDE.md`).
- Lines marked `GAME:` in the scripts and `TODO` in the READMEs are the places the template left for this game.

<!-- Notes that are only true for this game go below. -->

## The program

- `src/midifile.c` Standard MIDI Files (0, 1, 2, RMID) and MUS lumps as one list of events in audio frames; `src/archive.c` WAD and GRP directories; `src/library.c` the song list (My songs, the installed ports' games: CrispyDoom's WADs, jfSW's GRPs, read in place); `src/player.c` the audio thread that sends the events (seek, pause, mute and solo, levels, the channels' settings sent again); `src/fonts.c` SoundFont files and `slots.txt`; `src/ui.c` the two pages; `src/gfx.c` the 640x480 8bpp frame in the core's font.
- On the PC: `cmake -S midiplayer -B build/host && cmake --build build/host`, then fakecore (`hybrid/tools/fakecore.c`) and `MISTER_HYBRID_SHM=<shm> MISTER_HYBRID_SOUNDFONTS=<fonts> MIDIPLAYER_DNK=<a games/dnk with CrispyDoom/, jfSW/> build/host/midiplayer`, run in a folder of songs. `midiplayer/test/readers.c` and `test/library.c` check the readers and the list.
- The start (host version 17, 2026-10-22): `MH_Overlay(1)` before `MH_PickGame()`, which shows the logo (`package/logos/MIDIPlayer.png`, Danik's, 2026-10-23) with "Loading MIDI Player..." alone (no files to check); after its 2 seconds the logo goes out and the page comes in, without rising (Danik's choice, `hybrid/docs/game_logo.md`). `main.c` no longer shows its own "Loading" message.
- MT32-pi, first notes cut short (CrispyDoom's report, 2026-10-23; Hybrid_MiSTer 9efa1de): `send_settings()` (a new song, a seek, going on after a pause, a switch of synthesizer) is a reset of 144 bytes and the song's settings on the Pi's 31250 baud cable, and the notes after it went out late with their ends nearly on time. Now the song's own events up to its next note go out with the settings (a file's start: its GS reset, programs, volumes), and `step()` holds the song for `MH_MidiBacklog()` (0 with the synthesizer). Checked with fakecore and `hybrid/tools/midilog_wire.py` on a test song (a GS reset and 64 settings at tick 0, then a note every 100 ms): the start, a pause and a seek had a note up to 107 ms late, now none more than 1 ms.
- Host version 16 has what it plays with. Host version 17 (the spec's last section) adds the overlay (the purple screen under the frame, the handover), `MH_MidiLoadSoundFont()` (choosing a font plays it: the preview), `MH_MidiDest()` (the Synthesizer / MT32-pi switch) and the slot functions (now `src/fonts.c` writes `slots.txt` itself). Until then the frame covers the core's picture and the synthesizer plays the OSD's Default slot (the core has no SoundFont line).
