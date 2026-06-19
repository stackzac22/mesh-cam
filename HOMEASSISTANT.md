# mesh-cam → Home Assistant (MQTT bridge)

Goal: field nodes have **no internet**. One node at home with WiFi acts as the
**MQTT gateway**, hears everything over LoRa, and relays it to Home Assistant.

```
   FIELD (no internet)                 HOME
 +---------------------+        +-----------------------+      +------------------+
 | Heltec V3 + XIAO    | LoRa   | Cardputer ADV         | WiFi | Home Assistant   |
 | camera node         |======> | (MQTT gateway: WiFi   |=====>| Mosquitto broker |
 | "MOTION at cam1"    | <===== |  + MQTT enabled)      |<=====| + Meshtastic HACS|
 +---------------------+        +-----------------------+ MQTT +------------------+
        ^                                                          |
        |   "snap" downlink: HA publishes -> ADV transmits -> field V3 -> XIAO snaps
        +----------------------------------------------------------+
```

Role split:
- **Heltec V3 (field)** — paired with the XIAO camera (see main README). Plain
  Meshtastic, battery/solar, just needs its channel set to **uplink enabled**.
- **Cardputer ADV (home)** — on house WiFi, MQTT enabled, pointed at HA's broker.
  It uplinks every packet it hears (including the field node's text) and forwards
  downlink commands back out over LoRa.
- **Home Assistant** — Mosquitto broker add-on + the official `meshtastic/home-assistant`
  integration (auto-discovers nodes) + the custom motion sensor / snap button below.

---

## 1. Home Assistant: broker + integration

1. Install the **Mosquitto broker** add-on (Settings → Add-ons), create an MQTT user/pass.
2. Add **MQTT** integration if not present (it auto-detects Mosquitto).
3. Install **HACS**, then add `meshtastic/home-assistant` as a custom repository and
   install it. It subscribes to the broker and auto-creates a device per node with
   battery / SNR / position / telemetry sensors — no extra YAML needed for those.

## 2. Cardputer ADV: turn it into the MQTT gateway

Plug the ADV in over USB and run (CLI: `pip install meshtastic`):

```bash
# Join house WiFi
meshtastic --set network.wifi_enabled true \
           --set network.wifi_ssid "YOUR_SSID" \
           --set network.wifi_psk  "YOUR_WIFI_PASSWORD"

# Point MQTT at the Home Assistant Mosquitto broker
meshtastic --set mqtt.enabled true \
           --set mqtt.address "192.168.1.50" \
           --set mqtt.username "mqtt_user" \
           --set mqtt.password "mqtt_pass" \
           --set mqtt.json_enabled true \
           --set mqtt.encryption_enabled false

# Enable uplink (and downlink, for the "snap" command) on the primary channel
meshtastic --ch-index 0 --ch-set uplink_enabled true
meshtastic --ch-index 0 --ch-set downlink_enabled true
```

> Region must match on every node (e.g. `meshtastic --set lora.region US`). The MQTT
> root topic then becomes `msh/US/...`. Change `US` below to your region.

On the **field Heltec V3**, just make sure its channel uplinks too:

```bash
meshtastic --ch-index 0 --ch-set uplink_enabled true
```

## 3. Catch the motion alert + add a snap button

The camera node publishes the text `MOTION at cam1`. With `json_enabled`, the gateway
publishes it to:

`msh/US/2/json/LongFast/!<gateway_nodeid>`  (channel name = `LongFast` by default)

Add the entities in `ha/` to your Home Assistant config (see those files), then reload.

- **binary_sensor `cam1_motion`** flips `on` when a `MOTION at cam1` text arrives.
- **button `cam1_snap`** publishes a `sendtext` downlink so the field node tells the
  XIAO to take a photo.

Downlink note: HA publishes to `msh/US/2/json/mqtt/` with this payload (the
`meshtastic/firmware` JSON downlink format):

```json
{ "from": 2130636288, "type": "sendtext", "payload": "snap", "channel": 0 }
```

`from` = the **decimal** node ID of the gateway (ADV). Get it with
`meshtastic --info` (the `num` field) and put it in `ha/automations.yaml`.

## 4. Verify before you build

Before adding any HA entities, confirm packets are actually flowing with the MQTT
client tools — see `ha/mosquitto-cheatsheet.md`. Subscribe to `msh/#`, watch the field
node's text arrive, and test the `snap` downlink by hand. Only wire up HA once you see
traffic.

## 5. Dashboard

`ha/lovelace-card.yaml` is a ready-to-paste card: motion indicator, last message, a
"Take photo" button (calls the snap script), and the gateway's battery/SNR.
