# mesh-cam

A two-board off-grid camera node for a Meshtastic mesh.

- **Heltec LoRa 32 V3** runs stock Meshtastic with the **Serial Module** enabled. It is
  the LoRa radio + mesh bridge.
- **Seeed XIAO ESP32-S3 Sense** runs the firmware in `src/main.cpp`: camera motion
  detection + a command parser, with full-res JPEG stills saved to microSD.

The two boards talk over a 3-wire UART. The mesh carries **text only** (alerts and
commands) — images stay on the SD card (LoRa has nowhere near the bandwidth for photos).

```
            LoRa mesh (text only)
   +--------------------------------------+
   |                                      |
[Heltec V3] --UART 38400-- [XIAO S3 Sense]-+- microSD (JPEG stills)
 Meshtastic   33/34<->44/43   camera + motion
 Serial Mod                   + command parser

 motion  -> "MOTION at cam1"  -> mesh
 "snap"  -> mesh -> Heltec -> XIAO -> photo to SD -> "SAVED ..." -> mesh
```

## Wiring (3 jumpers)

| XIAO S3 Sense   | -> | Heltec V3      | Meaning                      |
|-----------------|----|----------------|------------------------------|
| D6 / GPIO43 (TX)| -> | GPIO33 (RX)    | XIAO talks, Heltec listens   |
| D7 / GPIO44 (RX)| -> | GPIO34 (TX)    | mesh -> XIAO commands        |
| GND             | -> | GND            | common ground (required)     |

> Do NOT use Heltec GPIO 43/44 for the Serial Module — open bug where the USB-C/UART
> pins won't accept serial input. Use 33/34 as above.

## Heltec V3 — enable the Serial Module

Install the CLI (`pip install meshtastic`), plug the Heltec in over USB, then:

```bash
meshtastic --set serial.enabled true
meshtastic --set serial.mode TEXTMSG     # incoming serial text -> mesh broadcast
meshtastic --set serial.rxd 33           # Heltec listens here
meshtastic --set serial.txd 34           # Heltec sends here (mesh -> serial)
meshtastic --set serial.baud BAUD_38400  # MUST match the XIAO sketch
```

Same options exist in the app/web client under Settings -> Serial Module.

## XIAO S3 Sense — build & flash (PlatformIO)

```bash
pipx install platformio        # or: pip install platformio
cd mesh-cam
pio run                         # compile
pio run --target upload         # flash (hold BOOT if it won't auto-enter)
pio device monitor              # watch serial @115200
```

Board: XIAO_ESP32S3, PSRAM enabled (set in platformio.ini). The esp32-camera, SD_MMC,
and FS libraries ship with the Arduino-ESP32 core, so no lib_deps are needed.

## Tuning (src/main.cpp)

- `MOTION_FRAC` / `PIXEL_DELTA` — raise to reduce false alerts, lower to be more sensitive.
- `COOLDOWN_MS` — minimum gap between mesh alerts; your LoRa duty-cycle guardrail.
- `handleCommand()` — add keywords beyond `snap` (e.g. `status`, `arm`, `disarm`).

## Home Assistant bridge

See `HOMEASSISTANT.md` for relaying the field node to Home Assistant: the Cardputer ADV
at home acts as a WiFi MQTT gateway, uplinking everything it hears (including
`MOTION at cam1`) to HA's Mosquitto broker. Example HA entities/automations are in `ha/`.

## Building it

`BUILDLOG.md` is a phase-by-phase checklist to tick through during the actual build,
ordered so each phase isolates one failure point.

