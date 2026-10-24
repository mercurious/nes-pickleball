#!/bin/sh
# Packs the teammate add-on (v0.0.2): what autogamer bundles as its pickleball/ directory. build/addon/pickleball/
# holds the cartridge (pickleball.nes, the v0.0.1 ROM, which this build must reproduce byte for byte), VERSION, the
# sidecar (sidecar/*.py), the mailbox contract, the RAM map, the manual, the credits and the licence; the tarball
# build/addon/pickleball-addon-<VERSION>.tar.gz is what the release carries and what autogamer's build/build.sh
# fetches (or takes from PICKLEBALL_SRC). Usage: tools/addon.sh     (make runs first)
set -e
cd "$(dirname "$0")/.." || exit 1
V=$(cat VERSION); ROM_SHA=01b464bf5b8de17de03379657aa5bd1dcb3c69ab5ce980bd33374dfbccf8a674   # the v0.0.1 cartridge
make -s
S=$(sha256sum build/pickleball.nes | cut -d' ' -f1)
[ "$S" = "$ROM_SHA" ] || { echo "addon: build/pickleball.nes is $S, not the v0.0.1 cartridge $ROM_SHA (the add-on changes no ROM)"; exit 1; }
A=build/addon/pickleball; rm -rf build/addon; mkdir -p "$A/sidecar"
cp build/pickleball.nes "$A/pickleball.nes"; echo "$V" > "$A/VERSION"
cp sidecar/pickleball_coop.py sidecar/voices.py sidecar/cache.py sidecar/ra.py sidecar/pad.py sidecar/stub.py sidecar/fakeservers.py "$A/sidecar/"
cp docs/MAILBOX.md docs/RAM_MAP.md docs/MANUAL.md audio/CREDITS.md LICENSE "$A/"
cat > "$A/README.md" <<EOF
# NES Pickleball $V: the teammate add-on

The cartridge (pickleball.nes, the v0.0.1 ROM, sha256 $ROM_SHA) and the sidecar that gives its players
voices and its computer partner a brain, for autogamer (https://github.com/mercurious/autogamer): sidecar/pickleball_coop.py
runs beside RetroArch and programs against MAILBOX.md and RAM_MAP.md; sidecar/cache.py renders the phrase cache with
llamatalk's tts-server. MANUAL.md is the instruction booklet. Source, docs and tests: https://github.com/mercurious/nes-pickleball
EOF
chmod +x "$A"/sidecar/*.py
(cd build/addon && tar -czf "pickleball-addon-$V.tar.gz" pickleball && sha256sum "pickleball-addon-$V.tar.gz" > "pickleball-addon-$V.tar.gz.sha256")
ls -la build/addon; du -sh "build/addon/pickleball-addon-$V.tar.gz"
