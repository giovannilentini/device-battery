#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Sets the version in every file that carries it (the release workflow refuses a tag that any
# of them disagrees with):
#
#     scripts/bump-version.sh 1.2.0
#
# What is left to do by hand is printed at the end.
set -eu

version=${1:-}
case $version in
    [0-9]*.[0-9]*.[0-9]*) ;;
    *) echo "usage: $0 X.Y.Z" >&2; exit 1 ;;
esac

cd "$(dirname "$0")/.."

sed -i "s/^\(project(device-battery VERSION \)[0-9.]*/\1$version/" CMakeLists.txt
sed -i "s/\(\"Version\": \"\)[^\"]*/\1$version/" package/metadata.json
sed -i -e "s/^\(Version: *\).*/\1$version/" -e "s/^\(Release: *\)[0-9]*/\11/" packaging/fedora/device-battery.spec
sed -i -e "s/^pkgver=.*/pkgver=$version/" -e "s/^pkgrel=.*/pkgrel=1/" packaging/arch/PKGBUILD

# Each file must now carry the version: a pattern that no longer matches would go unnoticed.
fail=0
check() { # file, pattern
    grep -q "$2" "$1" || { echo "error: $1 was not updated" >&2; fail=1; }
}
check CMakeLists.txt "^project(device-battery VERSION $version "
check package/metadata.json "\"Version\": \"$version\""
check packaging/fedora/device-battery.spec "^Version: *$version\$"
check packaging/arch/PKGBUILD "^pkgver=$version\$"
[ "$fail" -eq 0 ] || exit 1

cat <<EOF
Version set to $version. Still to do:
  - CHANGELOG.md: rename "Unreleased" to "$version ($(date +%Y-%m-%d))"
  - packaging/fedora/device-battery.spec: add a %changelog entry
  - commit, tag and push, then update the AUR checksum (see packaging/README.md)
EOF
