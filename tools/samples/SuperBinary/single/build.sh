#!/bin/bash
set -e
cd $( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )

if [ ! -f ../../../../apps/simple/build/zephyr/app_update.bin ]; then
    echo "The '../../../../apps/simple/build/zephyr/app_update.bin' file"
    echo "does not exists. Prepare an application image as described"
    echo "in the documentation."
    exit 1
fi

mkdir -p build
cp ../../../../apps/simple/build/zephyr/app_update.bin build/

python3 ../../../ncsfmntools SuperBinary    \
	SuperBinary.plist                   \
	--out-plist build/SuperBinary.plist \
	--out-uarp build/SuperBinary.uarp   \
	--metadata build/MetaData.plist     \
	--payloads-dir build                \
	--release-notes ../ReleaseNotes

