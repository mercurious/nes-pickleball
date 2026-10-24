#!/bin/sh
# Tags and publishes a release of the cartridge: the ROM, its sha256, the box art and the manual, as mercurious.
# Needs a clean tree on main, VERSION in the file VERSION. DRY=1 only builds and checks. Never switches gh's account.
# RETAG=1 re-releases an existing tag from HEAD: the tag is moved and the published release replaced (only while
# nobody has the old one).
set -e
cd "$(dirname "$0")/.." || exit 1
V=$(cat VERSION); TAG="v$V"
[ -z "$(git status --porcelain)" ] || { echo "tree not clean"; exit 1; }
[ "$(git rev-parse --abbrev-ref HEAD)" = main ] || { echo "not on main"; exit 1; }
make -s
cp build/pickleball.nes "build/pickleball-$V.nes"
(cd build && sha256sum "pickleball-$V.nes" > "pickleball-$V.nes.sha256" && cat "pickleball-$V.nes.sha256")
[ "${DRY:-0}" = 1 ] && { echo "dry run: $TAG ready in build/"; exit 0; }
T=$(gh auth token --user mercurious); L=$(GH_TOKEN="$T" gh api user --jq .login)
[ "$L" = mercurious ] || { echo "token is $L, not mercurious: abort"; exit 1; }
FORCE=
if git rev-parse -q --verify "refs/tags/$TAG" >/dev/null && [ "$(git rev-parse "$TAG^{commit}")" != "$(git rev-parse HEAD)" ]; then
  [ "${RETAG:-0}" = 1 ] || { echo "$TAG exists at $(git rev-parse --short "$TAG^{commit}"); RETAG=1 to move it"; exit 1; }
  git tag -f -a "$TAG" -m "NES Pickleball $TAG: the cartridge"; FORCE=--force
  GH_TOKEN="$T" gh release delete "$TAG" --repo mercurious/nes-pickleball --yes 2>/dev/null || true
else
  git tag -a "$TAG" -m "NES Pickleball $TAG: the cartridge" 2>/dev/null || true
fi
git -c credential.helper= -c "credential.helper=!f() { echo username=mercurious; echo password=$T; }; f" push -q $FORCE origin main "$TAG"
NOTES="NES Pickleball $TAG, the cartridge: an MMC3 ROM (256 KB PRG, 32 KB CHR, 8 KB PRG-RAM), plays on any NES emulator or flash cart. See docs/MANUAL.md for how to play and docs/MAILBOX.md for the sidecar contract. Built from $(git rev-parse --short HEAD) with cc65 2.19; sha256 in the .sha256 file."
GH_TOKEN="$T" gh release create "$TAG" --repo mercurious/nes-pickleball --title "NES Pickleball $TAG" --notes "$NOTES" \
  "build/pickleball-$V.nes" "build/pickleball-$V.nes.sha256" art/boxart.png docs/MANUAL.md
echo "released $TAG"
