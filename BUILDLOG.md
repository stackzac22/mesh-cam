# mesh-cam — build checklist

Tick these in order. Each phase isolates one failure point, so if something breaks you
know it's in the phase you just did — not somewhere in the whole chain.

## Phase 0 — Parts on the bench
- [ ] Heltec LoRa 32 V3
- [ ] Seeed XIAO ESP32-S3 Sense (+ camera ribbon seated)
- [ ] microSD card (<=32GB, FAT32) in the XIAO Sense expansion board
- [ ] 3x jumper wires, antennas attached to BOTH LoRa nodes (never power LoRa w/o antenna)
- [ ] Good DATA USB-C cable (not charge-only)

## Phase 1 — Flash Meshtastic (Heltec V3)
- [ ] Web flasher (flasher.meshtastic.org), select **Heltec V3**, **Erase flash = ON**
- [ ] `meshtastic --set lora.region US` (match on every node)
- [ ] `meshtastic --info` boots, shows a node — record hex `!id` and decimal `num`: ____________

## Phase 2 — Serial Module on the Heltec V3
- [ ] `serial.enabled true`, `serial.mode TEXTMSG`, `serial.rxd 33`, `serial.txd 34`, `serial.baud BAUD_38400`
- [ ] Bench test: USB-serial adapter -> GPIO33+GND, type a line @38400 -> appears on the mesh
      (proves the Serial Module works before the camera is involved)

## Phase 3 — Flash the XIAO camera firmware
- [ ] `pio run` compiles clean
- [ ] `pio run --target upload` (hold BOOT if needed)
- [ ] `pio device monitor` @115200 shows no "Camera init failed" / "SD init failed"

## Phase 4 — Wire the two boards
- [ ] XIAO D6/GPIO43 (TX) -> Heltec GPIO33 (RX)
- [ ] XIAO D7/GPIO44 (RX) -> Heltec GPIO34 (TX)
- [ ] GND -> GND
- [ ] Wave at the camera -> `MOTION at cam1` shows on the mesh (phone/2nd node)
- [ ] Adjust `MOTION_FRAC` / `PIXEL_DELTA` if too noisy or too deaf

## Phase 5 — Gateway (Cardputer ADV at home)
- [ ] WiFi: `network.wifi_enabled/ssid/psk`
- [ ] MQTT: `mqtt.enabled true`, `mqtt.address <broker>`, user/pass, `mqtt.json_enabled true`
- [ ] Channel: `--ch-index 0 --ch-set uplink_enabled true` (and `downlink_enabled true`)
- [ ] Same `lora.region` as the field node
- [ ] Record ADV decimal `num` (for downlink `from`): ____________

## Phase 6 — Verify the bridge (CLI, before HA)
- [ ] `mosquitto_sub -t 'msh/#' -v` shows traffic
- [ ] `mosquitto_sub -t 'msh/US/2/json/#' -v` shows decoded JSON
- [ ] Field node's `MOTION at cam1` text appears in the JSON stream
- [ ] Record exact topic (channel + gateway id): msh/____/2/json/__________/!__________
- [ ] `mosquitto_pub` a `snap` downlink -> XIAO saves a photo -> `SAVED /snap_N.jpg` comes back
      (see ha/mosquitto-cheatsheet.md)

## Phase 7 — Home Assistant
- [ ] Mosquitto broker add-on + MQTT integration up
- [ ] `meshtastic/home-assistant` installed via HACS; nodes auto-discovered as devices
- [ ] Paste `ha/configuration.yaml` (fill REGION / CHANNELNAME / GATEWAY_NODEID), reload
- [ ] `binary_sensor.cam1_motion` flips on real motion
- [ ] Paste `ha/automations.yaml` (fill region + decimal node id), reload automations
- [ ] Paste `ha/lovelace-card.yaml`; "Take photo" button triggers a real snap

## Phase 8 — Field deploy
- [ ] Field node on battery/solar, weatherproofed, antenna clear
- [ ] Confirmed range from deploy spot back to the home gateway
- [ ] `COOLDOWN_MS` set sane for duty cycle in your region/band
- [ ] Photos retrievable (pull SD, or plan a WiFi-in-range dump later)
