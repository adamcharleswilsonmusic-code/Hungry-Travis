# SPDX-License-Identifier: GPL-3.0-only
# Source from fm1/: build.sh, ../start.sh and ../check.sh enter this directory.
# Release portability update, 2026-10-26; firmware inputs are unchanged.
export PYTHON="${PYTHON:-python3}"
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-$(pwd -P)/vendor/ac79}"
export X0X_FONTSET="${X0X_FONTSET:-terminus}"
export JIELI_DOCKER_IMAGE="${JIELI_DOCKER_IMAGE:-debian:bookworm-slim@sha256:7c7b2c966bc9ee8cedfeef67e0e279108992c77681fa595db4a9d65c06ccc587}"
