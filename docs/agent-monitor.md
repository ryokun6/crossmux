# Agent Monitor

Agent Monitor is an optional foreground dashboard for viewing compact coding-agent and host status over MQTT. It keeps Wi-Fi active only while the app is open and prevents automatic sleep during monitoring.

## Configuration

Open **Apps → Agent Monitor**, then press **OK** to open MQTT settings.

Configure:

- Broker hostname or IPv4 address
- TCP port, default `1883`
- Optional username and password
- Subscription topic, default `xteink/agent-monitor/state`

Settings are stored on the SD card at `/.crosspoint/agent-monitor.json`. Passwords are device-bound obfuscated values, not cryptographic secrets. Use a dedicated, least-privilege MQTT account when authentication is enabled.

The app supports plain MQTT over TCP. TLS/MQTTS is not currently supported.

## MQTT payload

Publish a retained JSON object so the current state appears immediately after the device reconnects. QoS 1 is recommended.

```json
{
  "version": 1,
  "sequence": 42,
  "timestamp": 1770000000,
  "host": "workstation",
  "system": {
    "cpu": 32,
    "memory": 48,
    "disk": 67
  },
  "agents": [
    {
      "id": "agent-session-id",
      "type": "pi",
      "status": "tool_running",
      "project": "project-alias",
      "tool": "build",
      "summary": "Building firmware",
      "updated_at": 1770000000,
      "elapsed": 138
    }
  ]
}
```

Supported agent types are `pi`, `claude`, and `codex`. Unknown types use the generic agent label.

Supported states are:

- `thinking`
- `working`
- `tool_running`
- `waiting_permission`
- `completed`
- `failed`
- `idle`
- `offline`

Active states are shown first. Within active and non-active groups, entries are sorted by `updated_at` from newest to oldest. The app retains up to 12 rows and displays four rows per page.

## Controls

- Left/right: previous or next page
- Up/down: move the selected row
- OK: open MQTT settings
- Back: return to the Apps menu

## Resource and privacy guidance

Keep payloads below 4 KiB. Do not publish full prompts, source code, commands, absolute paths, credentials, or tool output. Prefer project aliases and short sanitized summaries.

The app uses fast e-ink refreshes and throttles automatic MQTT-driven refreshes to at most once every five seconds. User navigation refreshes immediately.
