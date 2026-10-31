#!/bin/sh
# Notarises and staples packages that were built without --notary-profile
# (for instance while the keychain was locked):
#   Packaging/notarize.sh dist/Stacks-0.1.2.pkg dist/Stacks-0.1.2-update.pkg
set -eu
PROFILE="${NOTARY_PROFILE:-stacks}"
for PKG in "$@"; do
    echo "== $PKG"
    xcrun notarytool submit "$PKG" --keychain-profile "$PROFILE" --wait | grep -E "status:|id:" | tail -2
    xcrun stapler staple "$PKG" >/dev/null
    spctl -a -vv -t install "$PKG" 2>&1 | sed -n 2p
done
