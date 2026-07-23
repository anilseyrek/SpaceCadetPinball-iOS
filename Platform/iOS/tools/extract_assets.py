#!/usr/bin/env python3
"""
Extract the original game data files needed to run SpaceCadetPinball.

The playable web build (alula's fork, gh-pages branch) ships the game data as an
Emscripten asset bundle (SpaceCadetPinball.data), with a file offset table
embedded in SpaceCadetPinball.js. This script downloads those two files and
unpacks the individual assets (PINBALL.DAT, FONT.DAT, SOUND*.WAV, *.MID, ...)
into ../../game_resources/.

NOTE: these are Microsoft's copyrighted game assets, redistributed by that web
build. This tool simply unpacks what that public demo already serves.

Usage:
    python3 extract_assets.py
"""
import json
import os
import urllib.request

BASE = "https://raw.githubusercontent.com/alula/SpaceCadetPinball/gh-pages"
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(os.path.join(HERE, "..", "..", "game_resources"))


def fetch(name):
    url = f"{BASE}/{name}"
    print(f"downloading {url}")
    with urllib.request.urlopen(url) as r:
        return r.read()


def main():
    js = fetch("SpaceCadetPinball.js").decode("utf-8", "replace")
    data = fetch("SpaceCadetPinball.data")

    i = js.find("loadPackage(")
    start = js.find("{", i)
    depth, end = 0, None
    for j in range(start, len(js)):
        if js[j] == "{":
            depth += 1
        elif js[j] == "}":
            depth -= 1
            if depth == 0:
                end = j + 1
                break
    meta = json.loads(js[start:end])

    os.makedirs(OUT, exist_ok=True)
    count = 0
    for f in meta["files"]:
        rel = f["filename"].lstrip("/").split("game_resources/", 1)[-1]
        dest = os.path.join(OUT, rel)
        os.makedirs(os.path.dirname(dest) or OUT, exist_ok=True)
        with open(dest, "wb") as out:
            out.write(data[f["start"]:f["end"]])
        count += 1
    print(f"extracted {count} files into {OUT}")


if __name__ == "__main__":
    main()
