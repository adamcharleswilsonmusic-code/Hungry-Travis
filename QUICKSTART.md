# Hungry Travis — quick start

Firmware **FM-1_797** for the **M-VAVE FM-1**.

## Install

Download the `.fwsc` package and complete source archive from this repository. Extract the archive next to the firmware, enter the `Hungry-Travis` folder, and follow the Python environment and USB-MIDI installer commands in README.md. No firmware build is required. Stop recording and disconnect the live follower before updating; keep USB power connected until the installer verifies the restarted identity.

## Feed Travis

1. Press **GLO** for the first bean. The empty READY spoon stays still until then.
2. Use the **two leftmost white keys** to roll left/right.
3. Use the **two rightmost white keys** to move back/forward.
4. Use the **leftmost black key** to tip down and pour; the **rightmost black key** tips up.
5. Pour the bean through the mouth. Proximity alone does not feed him.

After a miss, the spoon returns empty and waits for GLO. Each miss adds frustration; GLO does not remove it. A successful bite calms Travis, celebrates, and refills automatically. Every fifth bite triggers the five-second head-to-bean reveal.

- **PLAY/STOP:** pause/resume physics, motion, speech, and ambience.
- **SAVE:** does not reload.
- **HOME:** reset view and reload.
- **SEL:** spoon game / face study.
- **Select knob:** rotate the scene. **KNOB2:** view tilt. **KNOB1:** mouth in face-study mode.

## Record a video on macOS

Run `sh start.sh` from the extracted source with its Python environment active. Open the printed local address, click **Connect FM-1**, and wait for the actual console picture. Click **Record** when ready, play on the FM-1, then **Stop & Export**. The finished movie appears on the Desktop with direct USB audio. No microphone fallback is used. Recording never starts by itself.

If no console is detected, connect and power on one FM-1 by USB and close other applications using its MIDI ports. The follower and device firmware must match. A disconnect preserves the received part of a take; reconnect and begin a new take after live follow returns.
