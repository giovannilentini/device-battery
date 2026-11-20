# Working notes

Internal notes for the maintainer and Claude: how the repo is laid out, the rules we work by,
and a log of what we changed. Claude Code reads this file at the start of every session.
Keep it current: update the structure when files move, add every new instruction, and log
every change set.

## What this is

**Device Battery**, a KDE Plasma 6 widget (`io.github.giovannilentini.devicebattery`) that shows
the battery of wireless devices: AirPods and Beats (each earbud and the case, read over Apple's
AAP protocol on an L2CAP socket), plus everything UPower knows (Bluetooth and HID mice,
keyboards, headsets, ...). It runs in a panel, the system tray or on the desktop.

C++ backend (Qt 6, QtDBus, BlueZ headers) exposed to QML as a plugin; QML frontend.

## Repository structure

```
CMakeLists.txt            project + version (bumped by scripts/bump-version.sh)
src/                      C++ backend, built as the QML plugin io.github.giovannilentini.devicebattery
  device.h/.cpp           Device/BatteryPart structs and every pure function: merging, ordering,
                          lowest device, recent-devices cache (JSON), notification rules, keys.
                          Most unit tests target this file.
  devicesource.h          base class of the sources: upsert()/remove()/clear() + devicesChanged
  upowersource.*          UPower over D-Bus, event-driven (PropertiesChanged deltas, no polling)
  upowerdevice.*          pure: UPower properties -> Device
  airpodssource.*         watches BlueZ for connected Apple audio devices, one AapSession each;
                          also lists connected Bluetooth devices without a battery (silentDevices)
  aapsession.*            one AAP connection: non-blocking socket, handshake, retries, watchdog
  aapprotocol.*           pure: AAP packet building/parsing (battery, ear detection, noise control)
  usbreceivers.*          sysfs scan for USB wireless receivers without a battery
  devicelistmodel.*       QAbstractListModel with minimal-diff setDevices(); summary/lowestIndex
  devicemodel.*           the QML singleton DeviceModel: merges sources, grace period for vanished
                          devices, recently disconnected cache, low battery notifications
                          (AirPods only), acquire()/release() refcount per widget instance
  devicefilter.*          per-instance proxy: hidden devices, custom names, show disconnected
  qmltypes.h              QML_FOREIGN registration of DeviceFilter
  dbusutils.h, logging.*  async D-Bus helper; logging category DEVICEBATTERY
tests/                    Qt Test unit tests, one per src unit (fake D-Bus services on a private bus)
package/                  the Plasma package (installed as the plasmoid)
  metadata.json           id, version, Plasma API
  contents/config/        main.xml (settings schema), config.qml (settings pages)
  contents/ui/main.qml    PlasmoidItem: tray status, tooltip, actions, notification config
  contents/ui/CompactRepresentation.qml   panel/tray icon: ring of the lowest device
  contents/ui/FullRepresentation.qml      popup list; switches to DesktopRepresentation on the desktop
  contents/ui/DesktopRepresentation.qml   adaptive grid of rings; list fallback when too small
  contents/ui/DeviceDelegate.qml          one row of the list
  contents/ui/BatteryRing.qml, PercentLabel.qml, levels.js (isLow, shared by ring and tray)
  contents/ui/configGeneral.qml, configDevices.qml   settings pages
po/                       translation template (.pot) and translations
scripts/                  bump-version.sh, extract-messages.sh (regenerates po/*.pot)
packaging/                Arch (PKGBUILD), Fedora (spec), Debian (build-deb.sh), KDE Store notes
.github/                  CI (build+tests on several distros, clang-format, pot check, qmllint),
                          release workflow, issue forms, dependabot
CHANGELOG.md              user-facing changes; new entries go under "Unreleased"
```

## Checks before pushing (CI runs all of them)

- Build with warnings as errors and run the tests:
  `cmake -B <dir> -G Ninja -DBUILD_TESTING=ON -DCMAKE_COMPILE_WARNING_AS_ERROR=ON && cmake --build <dir> && ctest --test-dir <dir>`
  Build in a scratch directory, not `build/` (that one is the maintainer's).
- clang-format **23.1.3** exactly (pinned; other versions wrap differently).
- `scripts/extract-messages.sh` after changing any user-visible QML text; CI runs `--check`.
- qmllint with `--max-warnings 0 --unqualified info -I <build>/bin` (i18nc is injected by Plasma).

## Instructions from the maintainer

- Work on a branch, never directly on `main`.
- Keep this file up to date: repo structure, these instructions, and a log of what we change.
  It is committed and pushed with the work, and is just for us (not user documentation).
- The *Disconnected* section stays, but is off by default (`showDisconnected` default `false`
  in main.xml); users turn it on in the settings.

## Change log

### 2026-11-20 — branch `fix/review-2026-11-20`

Fixes from the review of `main` at fd4a0ee:

1. **Desktop list: "N min ago" froze.** The list fallback in `DesktopRepresentation.qml` did not
   pass `now` to `DeviceDelegate`, so each row kept the time it was created. It now has its own
   one-minute timer (running only while the list is shown) and passes `now: desktop.now`.
2. **"Yesterday" misused a plural form.** `DeviceDelegate.lastSeenText` used
   `i18ncp("Yesterday", "%1 days ago", days)`; in Slavic languages 21, 31, ... days use the
   singular form and read "Yesterday". Now `i18nc("Yesterday")` for exactly one day and
   `i18ncp("%1 day ago", "%1 days ago")` otherwise. Template regenerated.
3. **Grace period cut short for a second vanished device.** `DeviceModel::sync` started the
   2 s grace timer only if it was not running, so a device vanishing during another's grace
   period got the remainder only. It now tracks the vanishing ids (`m_vanishing`) and restarts
   the timer when a new one joins.
4. **Notifications ignored per-widget settings.** Low battery notifications used the device's
   own name and fired for devices hidden in every widget instance. `configureNotifications()`
   now also takes the instance's hidden keys and custom names; the new pure
   `notificationFor()` (device.cpp) decides per device: only instances that show it count, the
   threshold is the highest among them, and the name is the first enabled instance's custom
   name. `devicesToWarn()` got an overload with a per-device threshold. `isHiddenBy()` and
   `customDeviceName()` moved to device.cpp and `DeviceFilter` uses them too. Tests added.

5. **Disconnected section off by default** (maintainer's choice: useful, but not wanted by
   default). `showDisconnected` defaults to `false` in `config/main.xml`; README, KDE Store
   text and changelog updated. KConfig does not store default values, so existing users who
   never touched the checkbox also stop seeing the section. The backend still remembers
   recently disconnected devices, so turning it on shows them right away. README's
   notification text was also updated for fix 4.

Still open (not fixed, unverified): `applyPlacements` assigns primary/secondary by position
after `parseBatteryPacket` dropped Disconnected/unknown entries, so if the primary's entry is
dropped the secondary gets its placement. Needs a packet capture to confirm.
