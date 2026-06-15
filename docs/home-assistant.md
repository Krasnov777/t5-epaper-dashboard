# Home Assistant integration

The frame stays standalone (custom firmware), but its HTTP API makes it easy to
**control from HA** and to **show indoor climate** from your HA sensors.

---

## 1. Control the frame from HA (switch modes, refresh)

Add `rest_command`s that hit the device API (replace `t5frame.local` with the IP
if mDNS isn't reachable from HA):

```yaml
# configuration.yaml
rest_command:
  t5_mode_photo:   { url: "http://t5frame.local/api/mode", method: POST, content_type: "application/json", payload: '{"mode":0}' }
  t5_mode_metrics: { url: "http://t5frame.local/api/mode", method: POST, content_type: "application/json", payload: '{"mode":1}' }
  t5_mode_home:    { url: "http://t5frame.local/api/mode", method: POST, content_type: "application/json", payload: '{"mode":2}' }
  t5_refresh:      { url: "http://t5frame.local/api/refresh", method: POST, content_type: "application/json", payload: '{}' }
```

Optional — a dashboard dropdown that drives the mode:

```yaml
input_select:
  t5_frame_mode:
    name: T5 Frame Mode
    options: [Photo, Metrics, Home]

automation:
  - alias: T5 Frame mode switch
    trigger:
      - platform: state
        entity_id: input_select.t5_frame_mode
    action:
      - choose:
          - conditions: "{{ states('input_select.t5_frame_mode') == 'Photo' }}"
            sequence: [{ service: rest_command.t5_mode_photo }]
          - conditions: "{{ states('input_select.t5_frame_mode') == 'Metrics' }}"
            sequence: [{ service: rest_command.t5_mode_metrics }]
          - conditions: "{{ states('input_select.t5_frame_mode') == 'Home' }}"
            sequence: [{ service: rest_command.t5_mode_home }]
```

Optional — surface device status as a sensor:

```yaml
sensor:
  - platform: rest
    name: T5 Frame
    resource: "http://t5frame.local/api/status"
    value_template: "{{ value_json.version }}"
    json_attributes: [ip, freeHeap, current]
    scan_interval: 300
```

Now you can switch modes from a dashboard or automations (e.g. *Home* mode in the
morning, *Photo* in the evening, *Metrics* when you wake).

---

## 2. Home mode — indoor zones from HA

`MODE_HOME` keeps the weather block on top and shows a **2×2 grid of zones**
(Downstairs / Living Room / Upstairs / Bedroom by default), each with an icon +
**temperature** and optional **humidity**, read directly from HA.

### a) Create a long-lived token
HA → click your **profile** (bottom-left) → **Security** → **Long-Lived Access
Tokens** → *Create Token*. Copy it (shown once).

### b) Configure on the frame
Open `http://t5frame.local` → **Home** tab:
- **HA base URL** — e.g. `http://homeassistant.local:8123` (or `http://192.168.x.x:8123`).
- **Long-lived token** — paste it (stored on the device only, never shown again /
  never returned by the API).
- **Zones** — for each zone set a **label**, a **temperature** sensor
  `entity_id`, and (optionally) a **humidity** sensor `entity_id`. Leave a zone's
  entities blank to show `--`.

Example entity IDs: `sensor.living_room_temperature`,
`sensor.bedroom_humidity`, etc. (find them in HA → Developer Tools → States).

Hit **Save & show now**. The frame pulls each sensor via
`GET {haUrl}/api/states/{entity_id}` on the metrics refresh interval (default
15 min).

### Notes
- The device reads the sensor's **state** (so the entity's value must be the
  number, e.g. a `sensor.*_temperature`). For a `climate.*` entity, expose its
  current temp as a template sensor first.
- HTTP and HTTPS HA URLs both work (HTTPS uses an insecure TLS client — fine on a
  trusted LAN).
- The token is sensitive; it lives only in the device's `settings.json`.
