# Device Battery

A KDE Plasma 6 widget that shows the battery level of your wireless devices, like
Apple's *Batteries* widget:

- **AirPods and Beats**: left earbud, right earbud and charging case, each to 1 %
- **Headphones and headsets** that report their battery over Bluetooth
- **Keyboards, mice, touchpads and game controllers**, over Bluetooth or a USB
  receiver (e.g. Logitech Unifying/Lightspeed)

It runs in the panel or the system tray. It shows the device that's closest to
running out inside a ring, and the popup lists every device. In the system tray it
hides when nothing is connected and comes forward when a battery gets low.

## Why AirPods need special handling

Linux doesn't normally show any battery level for AirPods. BlueZ only understands the
standard Bluetooth battery reports, and Apple uses its own *Apple Accessory Protocol*
(AAP) on L2CAP channel `0x1001`. This widget opens that channel for connected Apple
audio devices and listens for battery packets. It only reads data and never changes
settings on the earbuds.

Everything else comes from UPower, the same service Plasma's own battery widget
uses.

## How it works

```
                        ┌───────────── plasmashell ─────────────┐
 UPower (system D-Bus) ─┤ UPowerSource ──┐                       │
                        │                ├─ DeviceModel ── QML UI│
 BlueZ  (system D-Bus) ─┤ AirPodsSource ─┘   (singleton)         │
                        │      └─ AapSession ── L2CAP socket ────┼── AirPods
                        └───────────────────────────────────────┘
```

| File | Responsibility |
|---|---|
| `src/device.{h,cpp}` | `Device` / `BatteryPart` value types and helpers |
| `src/aapprotocol.{h,cpp}` | Builds and parses AAP packets (pure functions, unit tested) |
| `src/aapsession.{h,cpp}` | One non-blocking L2CAP connection to one accessory |
| `src/airpodssource.{h,cpp}` | Finds connected Apple audio devices through BlueZ and runs a session for each |
| `src/upowersource.{h,cpp}` | Peripheral batteries from UPower |
| `src/devicemodel.{h,cpp}` | Merges the sources into the list model the UI shows |
| `package/` | The plasmoid: QML UI, settings and metadata |

### Performance

- **No polling.** UPower and BlueZ push changes over D-Bus, and the AirPods push
  battery packets. When nothing changes, the widget does nothing.
- **No extra process.** The backend is a QML plugin loaded into plasmashell.
- **One backend for all widget instances.** The model is a QML singleton, so
  each device gets only one AAP connection.
- **Minimal wake-ups.** Signals are subscribed per peripheral, or filtered on the bus
  side, so updates to the laptop battery or other Bluetooth devices are never
  delivered to the widget.
- **Minimal UI updates.** The model applies precise row inserts, moves and changes,
  so the popup never rebuilds its delegates.

## Build and install

Build dependencies on Fedora:

```sh
sudo dnf install cmake gcc-c++ extra-cmake-modules \
    qt6-qtbase-devel qt6-qtdeclarative-devel bluez-libs-devel
```

On Debian/Ubuntu: `cmake g++ extra-cmake-modules qt6-base-dev qt6-declarative-dev libbluetooth-dev`.

```sh
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
sudo cmake --install build
systemctl --user restart plasma-plasmashell   # load the new plugin
```

Then right-click the panel → *Add Widgets…* → **Device Battery**. In the system tray,
go to *Configure System Tray… → Entries* instead.

To uninstall:

```sh
sudo xargs rm -v < build/install_manifest.txt
```

## Settings

- **Show percentage next to the icon.** Applies to horizontal panels. The system tray
  always shows the icon only.
- **Low battery at.** Below this level the ring turns red and the tray icon moves
  out of the overflow menu.

## Troubleshooting

To see what the backend is doing, run plasmashell with debug logging:

```sh
QT_LOGGING_RULES="devicebattery.debug=true" plasmashell --replace
```

- **AirPods show no level.** The AAP channel opens only while the AirPods are
  connected as an audio device, starting 2 seconds after they connect. AirPods
  answer **one AAP client at a time**, so close any other app that reads them
  (e.g. LibrePods). The widget retries automatically, and reconnects if a session
  goes silent for 5 minutes.
- **A Bluetooth device is missing.** Run `upower -d`. If the device isn't listed
  there, it doesn't report its battery to Linux. Some headsets only do so when
  BlueZ's experimental features are on (`Experimental = true` in
  `/etc/bluetooth/main.conf`).

## License

MIT, see [LICENSE](LICENSE).
