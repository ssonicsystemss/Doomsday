run() { k=$1; sp=$2; set -- $(cat presets/$k.args); python3 tools/search.py "$1" $2 35 presets/${k}_base.json $sp presets/$k.json $3 > presets/$k.log 2>&1; }
run dive_shot space_b.json &
run sylenth_talk space_b.json
wait
run tritone_fall space_trit.json
for k in dive_shot sylenth_talk tritone_fall; do grep -E "^(SCREECH|FHSP|Tritone)" presets/$k.log | head -1; grep BEST presets/$k.log; done
