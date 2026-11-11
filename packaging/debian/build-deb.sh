#!/bin/sh
# Builds a .deb of the widget for the Debian or Ubuntu release this runs on.
#
#   packaging/debian/build-deb.sh [source directory] [output directory]
#
# The source directory defaults to the repository this script is in, the output directory
# to ./dist. Needs the build dependencies the README lists for Debian, plus:
#   ninja-build dbus-daemon gettext dpkg-dev apt-file   (and `apt-file update` run once)
#
# The package depends on the libraries the plugin links to (found by dpkg-shlibdeps) and on
# the packages that ship the QML modules the widget imports (found by apt-file, because
# their names differ between Debian and Ubuntu releases).
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
src=$(realpath "${1:-$script_dir/../..}")
out=${2:-dist}
mkdir -p "$out"
out=$(realpath "$out")

# The same list as ecm_find_qmlmodule in CMakeLists.txt and Requires: in the Fedora spec.
qml_modules="
QtQuick.Shapes
org.kde.kcmutils
org.kde.kirigami
org.kde.plasma.components
org.kde.plasma.configuration
org.kde.plasma.core
org.kde.plasma.extras
org.kde.plasma.plasmoid
"

version=$(sed -n 's/^project(device-battery VERSION \([0-9.]*\).*/\1/p' "$src/CMakeLists.txt")
[ -n "$version" ] || { echo "no version in $src/CMakeLists.txt" >&2; exit 1; }
arch=$(dpkg --print-architecture)
. /etc/os-release
distro="$ID$VERSION_ID" # debian13, ubuntu26.04

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
root="$work/root"

cmake -S "$src" -B "$work/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTING=ON
cmake --build "$work/build"
ctest --test-dir "$work/build" --output-on-failure
DESTDIR="$root" cmake --install "$work/build" --strip

install -Dm644 "$src/LICENSE" "$root/usr/share/doc/device-battery/copyright"

plugin=$(find "$root" -name libdevicebatteryplugin.so)
[ -n "$plugin" ] || { echo "the plugin was not installed" >&2; exit 1; }

# Where this distribution keeps QML modules, relative to /: the plugin's own module is in
# <that>/io/github/giovannilentini/devicebattery.
qml_dir=$(dirname "$plugin")
qml_dir=${qml_dir#"$root"/}
qml_dir=${qml_dir%/io/github/giovannilentini/devicebattery}

depends=$(cd "$work" && mkdir debian && : > debian/control \
    && dpkg-shlibdeps --ignore-missing-info -O "$plugin" | sed 's/^shlibs:Depends=//')

# Prints the packages that ship a file, as alternatives ("a | b") if there are several.
owners() {
    apt-file search --package-only --regexp "$1\$" | sort -u | paste -sd'|' | sed 's/|/ | /g'
}

for module in $qml_modules; do
    path="$qml_dir/$(echo "$module" | tr . /)/qmldir"
    packages=$(owners "$path")
    case "$packages:$module" in
        # Before Plasma 6.4 these two are not files: plasmashell itself registers them.
        :org.kde.plasma.configuration | :org.kde.plasma.plasmoid) packages=$(owners usr/bin/plasmashell) ;;
    esac
    [ -n "$packages" ] || { echo "no package ships the QML module $module ($path)" >&2; exit 1; }
    echo "$module: $packages"
    case ", $depends, " in
        *", $packages, "*) ;;
        *) depends="$depends, $packages" ;;
    esac
done
depends="$depends, upower"

mkdir "$root/DEBIAN"
cat > "$root/DEBIAN/control" << EOF
Package: device-battery
Version: $version-1
Architecture: $arch
Maintainer: giovannilentini <giovannilentini@users.noreply.github.com>
Installed-Size: $(du -sk --exclude=DEBIAN "$root" | cut -f1)
Depends: $depends
Recommends: bluez
Section: kde
Priority: optional
Homepage: https://github.com/giovannilentini/device-battery
Description: Plasma 6 widget showing the battery of AirPods and other wireless devices
 A KDE Plasma 6 widget that shows the battery level of wireless devices, like
 Apple's Batteries widget: AirPods and Beats (left and right earbud and the case,
 read over Apple's own protocol), headphones, keyboards, mice and game
 controllers. It runs in the panel, the system tray or on the desktop.
EOF
(cd "$root" && find usr -type f -exec md5sum {} + > DEBIAN/md5sums)

deb="$out/device-battery_$version-1_${distro}_$arch.deb"
dpkg-deb --root-owner-group --build "$root" "$deb"
dpkg-deb --info "$deb"
dpkg-deb --contents "$deb"
