# KDE Store page

Category: **Plasma 6 Widgets**. Screenshots: `docs/screenshots/popup.png` and
`docs/screenshots/desktop.png`. Source: <https://github.com/giovannilentini/device-battery>.

Do not upload an installable widget file (see `README.md` in this folder): the widget needs
its compiled plugin, which *Get New Widgets…* cannot install.

---

**Name:** Device Battery

**Summary:** Battery levels of AirPods, headphones, keyboards, mice and other wireless devices

**Description:**

See the battery of all your wireless devices at a glance, like Apple's Batteries widget.

- AirPods and Beats: left earbud, right earbud and the case, each to 1 %, which earbuds are in your ears, the noise control mode, and a notification when they run low. Linux normally shows no battery for AirPods at all.
- Headphones, headsets, keyboards, mice, touchpads and game controllers, over Bluetooth or a USB receiver.
- In the panel or the system tray: the device closest to running out inside a ring, and a list of every device. In the tray it hides when nothing is connected and comes forward when a battery is low.
- On the desktop: a grid of rings that adapts to the widget's size.
- Disconnected devices stay listed with their last level and when they were last seen.
- Hide or rename devices. Devices that do not report a battery are named, with a hint.
- No polling: it only does work when a device reports something.

**Installation:** this widget contains a compiled plugin, so it cannot be installed from *Get New Widgets…*. Install it from your distribution instead:

- Arch Linux: `yay -S plasma6-applets-device-battery` (AUR)
- Fedora: `sudo dnf copr enable giovannilentini/device-battery` then `sudo dnf install device-battery`
- Other distributions: build it from source, see https://github.com/giovannilentini/device-battery#build-from-source

Then restart Plasma, and add **Device Battery** from *Add Widgets…*.

**Bugs and requests:** https://github.com/giovannilentini/device-battery/issues
