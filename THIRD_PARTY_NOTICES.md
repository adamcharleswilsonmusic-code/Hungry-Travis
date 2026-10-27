# Hungry Travis component notices

Paths below are inside the complete source archive, relative to Hungry-Travis/fm1/ unless stated otherwise. Existing source copyright and license headers remain authoritative for their files.

| Component | Attribution | License / retained evidence |
| --- | --- | --- |
| FM-1 HAL, startup, update loader, USB/audio, display, storage, MIDI, build, packaging and updater | Felucca by Leo Kuroshita (@kurogedelic), Hügelton Instruments; X0X/FoMni adaptations by Charles Vestal | GPL-3.0-only; retained source headers and LICENSE |
| Hungry Travis native face/spoon renderer, feeding physics, reward animations, audio scheduler, ambient synthesis and tests | Hungry Travis project work based on the platform above | GPL-3.0-only source headers and LICENSE |
| Lookup-table generator and shared DSP helpers | Felucca lineage; retained tools/gen_tables.py and firmware/src/dsp/fastmath.h | GPL-3.0-only headers |
| FM6 table derivations retained in the lookup generator | Music Synthesizer for Android (msfa); Copyright 2012 Google Inc. and Copyright 2013 Google Inc.; retained LFO table also matches Dexed's Apache-licensed msfa component | Apache-2.0; source attribution in tools/gen_tables.py, verified upstream references in LICENSES/msfa-NOTICE.txt, and complete license in LICENSES/Apache-2.0-msfa.txt. These are retained generator material, not a bundled msfa synthesizer engine. |
| Terminus bitmap fonts | Dimitar Toshkov Zhekov | SIL OFL 1.1; assets/fonts/Terminus-LICENSE.txt |
| Barlow Semi Condensed resources | The Barlow Project Authors | SIL OFL 1.1; assets/fonts/Barlow-OFL.txt |
| Lilita One game lettering | Juan Montoreano, copyright 2011; Reserved Font Name Lilita | SIL OFL 1.1; assets/fonts/LilitaOne-OFL.txt and LilitaOne-source.json; unmodified font |
| Character voice recordings and reference drawing | Supplied by Adam Wilson | Thirteen original WAVs and derived ADPCM/envelope banks with hashes and processing in assets/audio/ht_voice_manifest.json; character drawing in the project-root reference/ directory. See LICENSING.md for the distinction between code licensing and media provenance. |
| Three JieLi AC79 package inputs | JieLi AC79 SDK commit d179b4484759423312073f5fbb232501aa491047 | Apache-2.0 attribution from the pinned SDK root license and retained upstream credits; vendor/ac79/NOTICE.md and LICENSES/AC79-SDK-LICENSE.txt |
| Live recorder and movie export helpers | Hungry Travis live-state protocol/native-renderer bridge; macOS helpers adapted from the local GPL-3.0-only RAMBEAVER recording tools | GPL-3.0-only source notices in project-root tools/ |
| Historical browser-runtime license texts | Emscripten authors, musl contributors and LLVM/compiler-rt contributors | LICENSES/Emscripten-LICENSE.txt, musl-COPYRIGHT.txt and compiler-rt-LICENSE.txt. The current native viewer does not bundle or require Emscripten. |

Platform lineage: [Felucca](https://github.com/hugelton/Felucca) → [X0X](https://github.com/charlesvestal/fm1-x0x) → FoMni. The retained upstream platform notices do not imply that other engines, game assets or sample libraries are included in Hungry Travis.

The three SDK inputs are reproduced byte-for-byte from the pinned SDK. Their contents and upstream redistribution authority were not independently audited beyond the retained root-license and credit evidence. No complete SDK, external compiler toolchain or system framework binary is distributed in this source package.

The source package includes the original Hungry Travis voice recordings so their encoded firmware bank can be reproduced. It excludes historical playthrough movies and capture sessions. No RAMBEAVER beaver artwork, prior-game score, O Canada arrangement, Diddy game artwork or prior-game voice bank is compiled into Hungry Travis.
