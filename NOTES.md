# MIDIPlayer for MiSTer: notes

How the port works and how it is built. For playing it, see the [README](README.md).

## Building

Everything builds in Docker on a Linux PC.

```sh
git clone --recursive https://github.com/ItsDanik/MIDIPlayer_MiSTer.git
cd MIDIPlayer_MiSTer
./build.sh              # ARM program      -> build/mister/midiplayer
./core/build_core.sh    # FPGA core (Quartus Lite 17.0.2) -> core/output_files/MIDIPlayer.rbf
./package.sh            # release zip      -> dist/MIDIPlayer_YYYYMMDD.zip
```

The toolchain image (Debian bullseye, glibc 2.31 to match the MiSTer) is built from `hybrid/toolchain/` on first use.

### Repository layout

| Path | |
|---|---|
| `hybrid/` | submodule: [ItsDanik/Hybrid_MiSTer](https://github.com/ItsDanik/Hybrid_MiSTer), what all our hybrid ports share: the FPGA host module, the ARM side library, the launcher (`danik_hybrid_ports.sh`), toolchain and conventions. See its README. |
| `core/` | FPGA core, based on [Template_MiSTer](https://github.com/MiSTer-devel/Template_MiSTer): `MIDIPlayer.sv` (OSD and button names) around `hybrid/rtl/hybrid_host.sv` |
| `midiplayer/` | the program: `src/` (the pages, the player, the MIDI / MUS / WAD / GRP readers, the song names), `test/` (tests of the readers and the library on the PC) |
| `package/` | files shipped in the release next to the binary (`danik_hybrid_launch.sh`, README), and `games.txt`: the games and their menu entries |
| `releases/` | release packages |

### Development notes

- `./build.sh host` builds the same program for the PC (or `cmake -S midiplayer -B build/host && cmake --build build/host` without Docker; one or the other for a build folder). It runs against `hybrid/tools/fakecore`, a stand-in for the FPGA core with the synthesizer's C model, that takes scripted input and saves screenshots and audio (`FAKECORE_MT32=1` for a stand-in MT32-pi):
  ```sh
  gcc -O2 -Ihybrid/hps -Ihybrid/tools -o build/host/fakecore hybrid/tools/fakecore.c -lpthread -lm
  build/host/fakecore /tmp/shm script.txt audio.raw &
  cd <songs folder> && MISTER_HYBRID_SHM=/tmp/shm MISTER_HYBRID_SOUNDFONTS=<fonts folder> <repository>/build/host/midiplayer
  ```
  `MIDIPLAYER_DNK=<folder>` names the `games/dnk/` whose ports' games it lists.
- `MISTER=root@<address> tools/test_on_mister.sh <id>` tries the build on a MiSTer next to what is installed there, without installing it: from `/tmp`, with the installed game's folder mirrored there (what the game writes stays in `/tmp`) and its own OSD settings. `list` and `nodata` for the core started by itself and without data, `log` for the game's log, `stop` to remove all of it again.
- `touch /tmp/midiplayer_nolaunch` on the MiSTer keeps the core loaded without starting the player, so you can start a development binary by hand (set `MISTER_HYBRID_CORE=MIDIPlayer`). `/tmp/danik_hybrid_ports.log` shows what the launcher daemon did.
