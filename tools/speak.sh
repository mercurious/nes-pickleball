#!/bin/sh
# Renders the cartridge's words with espeak-ng (a 1985 voice if ever there was one) and encodes them to DPCM.
# Usage: tools/speak.sh [rate_index]   -> art/speech.bin, art/speech.h   (committed, so the build needs no espeak)
cd "$(dirname "$0")/.." || exit 1
R=${1:-5}
args=""
while read -r id text; do
  case "$id" in \#*|"") continue;; esac
  espeak-ng -v en-us -s 190 -p 35 -a 180 -w "art/speech/$id.wav" "$text" || exit 1
  args="$args $id=art/speech/$id.wav"
done < art/words.txt
python3 tools/dmc.py "$R" art/speech.bin art/speech_words.h $args
