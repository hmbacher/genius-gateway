# v1.2.1 legacy fixtures

Test files for exercising the v1.3 split migrations registered by
`src/migrations/GatewayMigrations.cpp`.

## `mqtt-settings.json`

To make the device look like a fresh-from-v1.2.1 upgrade:

1. **Delete** these files if present (each one makes a migration's `shouldRun`
   return false, so the runner would skip):
   - `/config/haSettings.json`
   - `/config/alarm-publishing.json`
   - `/config/migrations.json`
2. **Upload** this file to `/config/mqtt-settings.json`.
3. **Reboot**.

Other config files (wifi, mqtt broker, security, devices, ...) are
independent and can stay.

The four keys this fixture contains are the ones the migrations look for:

| Key | Read by |
|---|---|
| `HAIntegrationEnabled` | `v1.3-split-mqtt-settings-ha` |
| `HAMQTTDiscoveryPrefix` | `v1.3-split-mqtt-settings-ha` |
| `alarmEnabled` | `v1.3-split-mqtt-settings-alarm` |
| `alarmTopic` | `v1.3-split-mqtt-settings-alarm` |

The discovery prefix is deliberately written **without a trailing slash** so
the boot also exercises the prefix-normalisation path
(`prefix += "/"` in `writeHASettingsFromLegacy`).

## Expected boot sequence (clean device, no `haSettings.json` / `alarm-publishing.json`)

1. **Pre-phase** runs both split migrations:
   - writes `/config/haSettings.json` with `enabled=true`, `discovery_prefix="homeassistant/"`,
     plus the factory device name / manufacturer / model.
   - writes `/config/alarm-publishing.json` with `alarmEnabled=true`,
     `alarmTopic="smarthome/genius-gateway/alarm"`.
2. Settings services load their new files.
3. **Post-phase** runs the cleanup migration and removes
   `/config/mqtt-settings.json`.
4. `/config/migrations.json` now lists all three IDs under `applied`.

## Verification

- `GET /rest/migrations` should show all three v1.3 migrations as `applied`.
- Open **System → Migrations** in the UI for the same view.
- Reboot again: the run loop should log `skip ... (already applied)` for each
  of the three IDs and do no file work.

## Partial scenarios

To test individual migrations, edit the file and remove the keys you don't
want migrated. Each migration's `shouldRun` only checks for the existence
of the legacy file (and absence of its successor); the apply step then
checks for its own subset of keys. Migrations whose keys are absent log
"legacy file lacks ... keys — nothing to migrate" and still mark themselves
as applied (so they don't re-check forever).
