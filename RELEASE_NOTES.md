# v1.4.1

Build for the original Genius Gateway hardware.
Built with pioarduino/platform-espressif32: 55.03.39

> [!TIP]
> See [CHANGELOG.md](https://github.com/hmbacher/genius-gateway/blob/main/CHANGELOG.md) for more detailed descriptions of the changes.

## Upgrade Notes
Config migrates v1 → v2 automatically on first boot: stale `lineId`/`lineCharacter`/`lineNumber` carried over from old-module devices are purged unless already flagged as manually entered. No manual action required.

## Features

### Manual Alarm Line entry for older radio modules
FM.Basis/FM.Pro modules cannot provide their alarm line (along with other diagnostic data) by acoustic readout: these devices are flagged **"Line required"** and get a rotary-switch entry UI instead of auto-detected lines. Radio status/DIP/interference are hidden for these modules in the details dialog, manually-entered lines carry over on module swap, and old configs are automatically migrated to drop stale auto-detected lines (v1 → v2).

## Bugfixes
- Device export always includes `radioInterference` and `lineId`, even when zero
- SPI Pin Configuration dropdowns no longer clipped at the settings card boundary
- Update indicator no longer shows releases incompatible with the current build target
- GitHub OTA no longer sends an empty URL when no matching binary exists
- Re-selecting the same firmware file after Abort now re-triggers the upload
- LittleFS reformat after a flash erase no longer logs raw internal filesystem errors
- No more spurious "Migrating device config from v0 to v2" log on a fresh device
