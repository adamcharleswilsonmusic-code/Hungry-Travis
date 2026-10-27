# Hungry Travis

An unreasonable appetite. A spoonful of trouble.

Hungry Travis turns the **M-VAVE FM-1** into a spoon-feeding balance game. Keep the bean in the bowl, approach an increasingly grumpy face, and tip the spoon to deliver the bite. Misses bring out his gargoyle side; successful feeds bring relief, rainbows, and an especially strange transformation every fifth bite.

Created by **Adam Wilson / Trust This Mechanism**. Independent firmware, without endorsement from M-VAVE or the upstream projects.

## Download

**Current verified firmware: FM-1_797.**

- [Installable firmware](HUNGRY-TRAVIS-FM-1_797.fwsc)
- [Quick-start guide PDF](Hungry-Travis-Quick-Start.pdf)
- [Complete corresponding source, tests, viewer, and recorder](Hungry-Travis-FM-1_797-source.zip)
- [Installation and controls](QUICKSTART.md)
- [Checksums](SHA256SUMS.txt) · [Release manifest](manifest.json)
- [Credits and component notices](THIRD_PARTY_NOTICES.md) · [GPL source license](LICENSE) · [Apache license](APACHE-2.0.txt)
- [Publication review and limits](LICENSING-REVIEW.md)

Download the firmware with GitHub's **Download raw file** control; saving a repository page as HTML does not download the firmware. Extract the source archive to obtain the full development tree. You do not need to build firmware to install the supplied package.

## Play

The game starts with an empty spoon. Press **GLO** for a bean.

| Control | Action |
| --- | --- |
| Leftmost two white keys | Roll left / right |
| Rightmost two white keys | Move back / forward |
| Leftmost black key | Tip down to pour |
| Rightmost black key | Tip up |
| GLO | Load the first bean or replace a missed one |
| PLAY / STOP | Pause / resume |
| HOME | Reset the view and reload food |
| SEL | Switch spoon game / face study |

Balance the bean as you approach, then pour it through the mouth. Being close to the face does not score by itself. After a miss, the empty spoon returns and waits for GLO. Reloading preserves frustration; successful feeding calms Travis and refills automatically. SAVE does not reload the spoon.

Thirteen character recordings react to play. Happy and angry speech animates his mouth; Zen is an inner voice. The beatless ambient melody ducks under speech. Every fifth success has a five-second head-to-bean transformation and spoon reveal.

## Install

Use the bundled USB-MIDI installer in the source archive. Installation replaces the running firmware; retain the package for the firmware you want to return to. Close any live follower/recording before updating, keep USB power stable, and let the installer finish and verify the restart.

1. Download the firmware and source archive into the same folder.
2. Extract the source archive. It creates a folder named `Hungry-Travis`.
3. Open a terminal in that folder and create the Python environment:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements.txt
```

On Windows, activate with `.venv\Scripts\activate` instead. Connect and power on one FM-1 by USB, then run:

```sh
python fm1/tools/fm1_install.py ../HUNGRY-TRAVIS-FM-1_797.fwsc
```

The installer asks before writing and checks the restarted identity. Use `--info` to inspect the connected device without installing. If a previous transfer was interrupted, consult the source installer's recovery behavior before attempting another update.

## Live view and recording

The included native C viewer follows state snapshots sent by the actual FM-1. It does not run a second game or simulate device timing. The separate local test uses the same C renderer and is labeled separately.

On macOS, install the Xcode command-line tools for the C and Swift compilers. With the Python environment active, run:

```sh
sh start.sh
```

Open the local address printed by the server, click **Connect FM-1**, and wait for the actual console picture. **Record** captures the picture and direct FM-1 USB audio; **Stop & Export** saves a uniquely named movie to the Desktop. Recording never starts automatically. The recorder uses the console's USB audio and has no microphone fallback. Recording helpers require macOS; the source and USB-MIDI installer are separate from those helpers.

## What has been verified

The archived package matches the installed checkpoint. Post-restart identity checks, USB audio enumeration, and matching live follow passed on the FM-1. The publication source rebuilt this exact firmware byte-for-byte. Host rendering/physics, audio, and capture protocol checks passed. Detailed evidence and publication checks are in `manifest.json` and the source archive's `release/` folder.

The earlier **16.07 FPS** measurement belongs to firmware **FM-1_796** in an observed mode. It is not a new measurement of this release or the fifth-feed peak. No new current-build FPS benchmark or movie export is claimed. Hunger-driven head movement and difficulty progression remain future work.

## Source and credits

Extract the archive and read `fm1/BUILDING.md` to rebuild. Firmware behavior and recorded assets are unchanged from the verified checkpoint; publication changes make setup paths portable, generate the host table header on a clean checkout, and correct inherited documentation. The source archive includes the original thirteen recordings, font notices, and the three pinned SDK package inputs. It excludes private playthroughs, the developer's environment, compiler installation, and previous games.

The native platform descends from **Felucca → X0X → FoMni**, by Leo Kuroshita (@kurogedelic), Hügelton Instruments, and Charles Vestal. GPL-3.0-only source notices and component-specific font/vendor licenses are retained. The character drawing and recordings were supplied by Adam Wilson. See the notices for asset terms and vendor audit limitations.

If you share a video, tag [@trustthismechanism on Instagram](https://www.instagram.com/trustthismechanism/) or [YouTube](https://www.youtube.com/@trustthismechanism).
