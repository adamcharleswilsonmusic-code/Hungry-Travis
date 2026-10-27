# Publication review — October 26, 2026

This records the practical release review for Hungry Travis FM-1_797. It is not a guarantee of legal immunity or an independent audit of opaque vendor binaries.

## Source and component evidence

- The platform carries GPL-3.0-only headers and the complete GPL text. The archive supplies the corresponding game/platform/loader source, build scripts, dependencies and original voice-bank inputs.
- [Felucca’s written licensing documentation](https://github.com/hugelton/Felucca/blob/main/LICENSING.md#jieli-sdk-files-in-the-packages-apache-20) explicitly identifies the same three JieLi SDK package inputs—uboot.boot, cfg_tool.bin and eq_cfg_hw.bin—as Apache-2.0. It says the SDK has no NOTICE file. The pinned files, root-license evidence, hashes and attribution are retained. This supports the distribution assessment; their internals and the vendor’s full rights chain were not independently audited.
- [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0) requires accompanying license text and retained attribution. The full text accompanies the firmware directly as APACHE-2.0.txt and is also in the archive. The msfa-derived generator material has its own retained copyright/derivation notice.
- Font license texts and source attribution are retained. The drawing and thirteen recordings were supplied by Adam Wilson, who expressly authorized this public project upload including those assets. No separate standardized media license is asserted.
- The README identifies the project as independent firmware and does not claim endorsement by the hardware or upstream authors.

## Verification and privacy

The staged source rebuilt the exact checkpoint firmware byte-for-byte. Native feeding/rendering, audio and capture-protocol checks passed. Hashes confirm the thirteen original recordings and vendor/font inputs. Private takes, prior games, compiler installations, environments, Git history, local absolute paths and camera metadata are excluded. The original working project remains unchanged.

The review found and corrected inherited documentation, local environment assumptions, a missing generated-table setup step, and a missing upstream table-generator attribution before publication. No material unresolved blocker was identified by the release checks.
