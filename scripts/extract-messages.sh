#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Collects the translatable texts of the widget into po/<domain>.pot and brings every
# po/<language>.po up to date with it. Run it after changing a text in the QML.
#
#     scripts/extract-messages.sh           # update the template and the translations
#     scripts/extract-messages.sh --check   # only tell whether the template is up to date (CI)
#
# Needs gettext (xgettext, msgmerge).
set -eu

cd "$(dirname "$0")/.."

# Plasma looks for a widget's translations in the domain plasma_applet_<plugin id>.
domain=plasma_applet_io.github.giovannilentini.devicebattery
template=po/$domain.pot
new=$(mktemp)
trap 'rm -f "$new"' EXIT

# Sorted, so that the template does not change with the order the file system lists files in.
find package -name '*.qml' -o -name '*.js' | LC_ALL=C sort | xgettext \
    --files-from=- --language=JavaScript --from-code=UTF-8 \
    -ki18n:1 -ki18nc:1c,2 -ki18np:1,2 -ki18ncp:1c,2,3 \
    --add-comments=i18n: --no-wrap \
    --package-name=device-battery \
    --msgid-bugs-address=https://github.com/giovannilentini/device-battery/issues \
    --output="$new"

# The texts alone: the header (creation date) and the layout differ between runs and between
# gettext versions.
texts() {
    grep -E '^(msgctxt|msgid|msgid_plural) ' "$1" | LC_ALL=C sort
}

if [ -f "$template" ] && [ "$(texts "$template")" = "$(texts "$new")" ]; then
    up_to_date=1
else
    up_to_date=0
fi

if [ "${1:-}" = --check ]; then
    if [ "$up_to_date" -eq 0 ]; then
        echo "$template does not match the texts in the QML: run scripts/extract-messages.sh" >&2
        exit 1
    fi
    echo "$template is up to date"
    exit 0
fi

# Leaves the template alone when no text changed, so that the repository stays clean.
if [ "$up_to_date" -eq 0 ]; then
    cp "$new" "$template"
fi

for po in po/*.po; do
    [ -e "$po" ] || continue
    msgmerge --quiet --update --backup=none --no-wrap "$po" "$template"
done

echo "$template: $(grep -c '^msgid ' "$template") texts"
