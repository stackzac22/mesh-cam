# Mosquitto / MQTT cheat-sheet — verify packets before touching Home Assistant

Install the client tools (Kali/Debian):

```bash
sudo apt install mosquitto-clients
```

`BROKER` = your Home Assistant Mosquitto IP, `USER`/`PASS` = the MQTT credentials.
`REGION` = your Meshtastic region (e.g. US).

## 1. See EVERYTHING the gateway is publishing

```bash
mosquitto_sub -h BROKER -u USER -P PASS -t 'msh/#' -v
```

`-v` prints `topic payload`. If this is silent, the gateway (Cardputer ADV) isn't
reaching the broker — check WiFi, `mqtt.enabled`, broker IP, and credentials.

## 2. Watch only the decoded JSON (needs mqtt.json_enabled true)

```bash
mosquitto_sub -h BROKER -u USER -P PASS -t 'msh/REGION/2/json/#' -v
```

A text packet looks like:

```json
{"channel":0,"from":2130636288,"to":4294967295,"type":"text",
 "payload":{"text":"MOTION at cam1"},"sender":"!7eef0000","timestamp":1718800000}
```

- `from`        = decimal node ID of the originating (field) node
- `sender`      = hex ID of the gateway that uplinked it
- `payload.text`= the message your binary_sensor matches on

## 3. Find your gateway node ID + channel name

The topic itself tells you both: `msh/US/2/json/<CHANNELNAME>/!<GATEWAY_NODEID>`
e.g. `msh/US/2/json/LongFast/!7eef0000` -> channel `LongFast`, gateway `!7eef0000`.

Decimal `num` (for the downlink `from` field) comes from the device:

```bash
meshtastic --info | grep -i num
```

## 4. Manually fire the "snap" downlink (test without HA)

```bash
mosquitto_pub -h BROKER -u USER -P PASS \
  -t 'msh/REGION/2/json/mqtt/' \
  -m '{"from": GATEWAY_NODEID_DECIMAL, "type": "sendtext", "payload": "snap", "channel": 0}'
```

If the field node's Serial Module is wired right, the XIAO should save a photo and the
mesh should carry back a `SAVED /snap_N.jpg` text — watch for it with the command in #2.

## 5. Common gotchas

| Symptom                          | Likely cause                                            |
|----------------------------------|---------------------------------------------------------|
| `msh/#` totally silent           | gateway not on WiFi / wrong broker IP / bad MQTT creds  |
| Protobuf only, no `/json/`       | `mqtt.json_enabled` is false on the gateway             |
| JSON appears but no field traffic| channel `uplink_enabled` false on the field node        |
| Downlink `snap` ignored          | channel `downlink_enabled` false, or wrong `from`/region|
| Region mismatch                  | nodes on different `lora.region` won't hear each other  |
