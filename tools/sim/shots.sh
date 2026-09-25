#!/bin/bash
# shots.sh <shots file.cpp> <sketch folder> <picture folder>   e.g. ./shots.sh shots_v11.cpp ../../SomudTick_v11.2/SomudTick run/shots
cd "$(dirname "$0")"
n=$(basename "$1" .cpp)
./build.sh "$1" "$2" "$n" && mkdir -p run && ./bin/$n "$3" > "run/$n.log" 2>&1 && ls "$3" | wc -l
