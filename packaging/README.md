# Packaging and releases

The widget contains a compiled QML plugin (the AirPods support needs a Bluetooth socket),
so it is distributed as distribution packages, not through *Get New Widgets…*.

| File | What it is |
|---|---|
| `arch/PKGBUILD`, `arch/.SRCINFO` | The AUR package `plasma6-applets-device-battery` |
| `fedora/device-battery.spec` | The Fedora package, built on COPR as `giovannilentini/device-battery` |
| `kde-store.md` | Text for the KDE Store page, which points to the packages |
| `../.github/workflows/release.yml` | Publishes a GitHub release for every `v*` tag |

## Making a release

1. Set the new version in `CMakeLists.txt` (`project(... VERSION x.y.z)`),
   `package/metadata.json` (`"Version"`), `fedora/device-battery.spec` (`Version:`, reset
   `Release:` to 1 and add a `%changelog` entry) and `arch/PKGBUILD` (`pkgver=`, reset
   `pkgrel=1`). The release workflow refuses to run if any of them differs from the tag.
2. Commit, then tag and push:
   ```sh
   git tag -a vX.Y.Z -m "Device Battery X.Y.Z" -m "- what changed"
   git push origin main vX.Y.Z
   ```
   The tag message becomes the release notes. The workflow builds and tests the tagged
   sources and attaches the source tarball, the source RPM and `SHA256SUMS` to the
   GitHub release. It can also be run by hand (*Actions → Release → Run workflow*) for
   an existing tag.
3. Update the packages (below).

## AUR (Arch Linux)

The AUR repository holds only `PKGBUILD` and `.SRCINFO`. After a release:

```sh
cd packaging/arch
updpkgsums                       # the checksum of the new tag's tarball (pacman-contrib)
makepkg --printsrcinfo > .SRCINFO
makepkg -si                      # build, run the tests and install, to try it
```

Commit both files here, then copy them to the AUR repository and push it:

```sh
git clone ssh://aur@aur.archlinux.org/plasma6-applets-device-battery.git   # once
cp PKGBUILD .SRCINFO ../../../plasma6-applets-device-battery/
cd ../../../plasma6-applets-device-battery
git commit -am "Update to X.Y.Z" && git push
```

The first push creates the package; it needs an AUR account with your SSH key.

## COPR (Fedora)

Once: create the project `device-battery` on <https://copr.fedorainfracloud.org>, with the
Fedora releases (and architectures) to build for.

For each release, build the source RPM that the release workflow attached, either from
the web page (*New build → From URLs*) or with `copr-cli`:

```sh
copr-cli build device-battery \
  https://github.com/giovannilentini/device-battery/releases/download/vX.Y.Z/device-battery-X.Y.Z-1.src.rpm
```

To try the spec locally: `rpmbuild -ba fedora/device-battery.spec` with the tarball in
`~/rpmbuild/SOURCES` (`spectool -g -R fedora/device-battery.spec` downloads it).

## KDE Store

See `kde-store.md` for the page text. Do not upload an installable widget file: *Get New
Widgets…* would install the QML without the plugin, and the widget would only show an
error. Link to the packages and to the GitHub releases instead.
