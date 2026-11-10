# Build Hungry Travis for FM-1

The supplied FM-1_797 firmware is the verified checkpoint. Building source is optional for players. These instructions compile and package source; they do not install firmware, contact the console, start recording or launch the local viewer.

## Host requirements

- Python 3 and Pillow. The recorded build environment used Pillow 11.3.0.
- JieLi's Linux x86-64 toolchain, containing pi32v2/ and common/.
- Native Linux x86-64, or Docker with Linux/amd64 support on macOS and other hosts. Docker must already be running for a container build.
- A host C compiler for optional native checks and the local viewer. The macOS recorder additionally needs swiftc and AVFoundation; MIDI install/capture tools need Python packages mido and python-rtmidi.

The full audio parity test imports the standard-library audioop module. Use Python 3.9–3.12 for that test; newer Python installations need a compatible audioop module. The importer and firmware build themselves do not import audioop.

## Python environment

From the project root:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements.txt
export PYTHON="$PWD/.venv/bin/python"
```

MIDI packages are needed for install/live capture, rather than native firmware compilation. No microphone fallback is used by the recorder.

## Compiler and package inputs

The original setup downloaded JieLi's compiler from the official endpoint:

[JieLi Linux toolchain download](https://pkgman.jieliapp.com/s/linux-toolchain)

It is an XZ-compressed tar archive. Extract it into a directory you control and set JIELI_TOOLCHAIN to the extracted directory containing pi32v2/ and common/:

```sh
export JIELI_TOOLCHAIN="/absolute/path/to/jieli-linux-toolchains-20260730.1"
```

The endpoint is mutable. The recorded compiler distribution was jieli-linux-toolchains-20260730.1 (the setup script describes its pi32v2 compiler as clang 4.0.1). Its pi32v2/bin/clang SHA-256 was:

```text
c45189ee624de0430deb0db02e6dce171fb5538e7fe323e3964dc8ac6236240f
```

A newer download may produce different bytes. Compiler binaries are not bundled here.

The normal build uses the three package inputs already bundled under vendor/ac79/. They come from [JieLi AC79 SDK commit d179b4484759423312073f5fbb232501aa491047](https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK/tree/d179b4484759423312073f5fbb232501aa491047), identified as AC79NN_SDK_V1.2.1_2023-12-13. A full SDK checkout is unnecessary. tools/build.py checks all three input hashes; [vendor/ac79/NOTICE.md](vendor/ac79/NOTICE.md) lists the files and hashes. AC79_SDK can override the bundled directory with another checkout having the same layout and exact input files.

env.sh selects this recorded container image by default:

```text
debian:bookworm-slim@sha256:7c7b2c966bc9ee8cedfeef67e0e279108992c77681fa595db4a9d65c06ccc587
```

The historical host used Docker Desktop 4.94.0. JIELI_DOCKER_IMAGE can override the image. Native Linux x86-64 runs the compiler directly by default; JIELI_DOCKER=1 explicitly selects Docker.

## Compile

From the project root, with the Python environment active and JIELI_TOOLCHAIN configured:

```sh
./fm1/build.sh
```

Default outputs:

- fm1/build/HUNGRY-TRAVIS-FM-1_797.fwsc — installable update package.
- fm1/build/omni.bin — native application.
- fm1/build/loader/ota.bin — update loader.
- fm1/build/gen/ — generated fonts, lookup tables and voice-bank header.

The build checks entry placement, image/RAM/pool reserves, single-precision arithmetic and low-level hardware boundaries. It uses the included encoded audio assets; archived original WAVs can reproduce those assets through tools/import_ht_audio.py as documented in assets/audio/README.txt. Default font selection is Terminus for platform/recovery roles and Lilita One for game text; the fonts and OFL notices are bundled.

The checked release checkpoint has an application size of 501,948 bytes, application RAM of 23,212 bytes and pool RAM of 288,000 bytes. Its installable package SHA-256 is:

```text
de460db393835644f013540ba5620396eb23c72e4373cdcbac889466bd7ef32c
```

The staged release changes documentation and host portability only. Preserve the supplied checkpoint when experimenting. Current release evidence records whether a clean rebuild of this staged source has been verified; the historical device validation alone does not establish that result.

## Optional host checks

From the project root, use the host compiler and a Python version providing audioop:

```sh
./check.sh
```

If the selected main Python lacks audioop, an existing compatible Python can run just the reference test:

```sh
AUDIO_TEST_PYTHON=/usr/bin/python3 ./check.sh
```

That example uses the older system Python provided on the original macOS host; other systems should select an installed Python 3.9–3.12 instead. These native checks cover rendering, physics, recorded-audio decoding, scheduler/pause behavior and ambient synthesis. They do not measure physical FM-1 frame rate. Current hardware verification and explicitly earlier performance measurements are separated in validation/current-build.json.

The historical folder and original local environment remain outside this release package. env.sh expects to be sourced while the working directory is fm1/; build.sh, start.sh and check.sh enter that directory before sourcing it.
