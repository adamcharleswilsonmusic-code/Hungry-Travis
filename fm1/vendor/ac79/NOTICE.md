# Minimal AC79 package inputs

These three files are copied without modification from JieLi SDK commit `d179b4484759423312073f5fbb232501aa491047`:

- `cpu/wl82/tools/uboot.boot`
- `cpu/wl82/tools/cfg_tool.bin`
- `cpu/wl82/tools/cfg/eq_cfg_hw.bin`

Upstream: https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK/tree/d179b4484759423312073f5fbb232501aa491047

The SDK root LICENSE is retained here and in LICENSES/AC79-SDK-LICENSE.txt. Pinned Felucca credits and FoMni licensing identify these package files as Apache-2.0. They are opaque vendor binaries; the retained evidence does not independently audit their internals. No full SDK checkout is bundled.

Their SHA256 values are checked by tools/build.py. Set AC79_SDK to this vendor/ac79 directory when rebuilding with tools/fm1pkg_make.py directly; env.sh does so for the normal build. The checked inputs are:

| File | SHA-256 |
| --- | --- |
| uboot.boot | 4e3b4c220dc96641cb5a723f41e68ce41d5261ae9434bb33fbd7f2c59976ded4 |
| cfg_tool.bin | 276579954f076886a6a7694f65dc71c034a63a2c204b76749065c0ac7b010d1b |
| cfg/eq_cfg_hw.bin | 41167491bffed4651750719c973d2758adeb9021a5670d02d6a53c85ed80ea7d |

A release-preparation check found no additional LICENSE, COPYING or NOTICE files at cpu/, cpu/wl82/, cpu/wl82/tools/ or cpu/wl82/tools/cfg/. Other SDK subtrees carry notices for their own networking/OS/example components; those subtrees are not included or linked by this game. The root-license and pinned upstream-credit limitation above remains.
