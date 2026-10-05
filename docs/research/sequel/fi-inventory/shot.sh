#!/bin/zsh
# usage: shot.sh <name|-> [cmd] [wait]
S=/private/tmp/claude-501/-Users-matteo-dev-kya/619f66d4-43a6-4a8e-bd4d-0b15727c29ea/scratchpad
if [ -n "$2" ]; then
  echo "$2" > $S/cmd.txt
  sleep ${3:-12}
fi
echo dump > $S/cmd.txt
sleep 1.5
grep -E "^(curSector|hero)" $S/cmd_out.txt | tail -2
if [ "$1" != "-" ]; then
  WID=$($S/winid | awk '{print $1}' | head -1)
  screencapture -x -l $WID $S/fi/$1.png
  sips -Z 1000 $S/fi/$1.png --out $S/view.png >/dev/null
  echo saved $S/fi/$1.png
fi
