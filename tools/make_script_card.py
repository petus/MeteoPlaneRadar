#!/usr/bin/env python3
"""Packs a script card: the card description plus its Berry program into one card file.

    tools/make_script_card.py [cards/src/<id>.json ...]      (no arguments: every card in cards/src)

A card file is one JSON document, and JSON has no way to write a program over
several lines. So a script card is kept as two files in cards/src - <id>.json
with everything but the program and a "script_file" naming it, and the .be
file - and this tool writes cards/<id>.json with the program in "script".
Run tools/make_index.py afterwards to put the card on offer in the store.
"""
import glob
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "cards", "src")
SCRIPT_SRC_MAX = 6000        # Config.h
CARD_FILE_MAX = 8192


def pack(path):
    with open(path, encoding="utf-8") as f:
        card = json.load(f)
    name = card.pop("script_file", None)
    if not name:
        sys.exit("%s has no script_file" % path)
    with open(os.path.join(os.path.dirname(path), name), encoding="utf-8") as f:
        script = f.read()
    if len(script.encode("utf-8")) > SCRIPT_SRC_MAX:
        sys.exit("%s is %d bytes, the limit is %d" % (name, len(script.encode("utf-8")), SCRIPT_SRC_MAX))
    card["script"] = script
    out = os.path.join(ROOT, "cards", card["id"] + ".json")
    raw = (json.dumps(card, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    if len(raw) > CARD_FILE_MAX:
        sys.exit("%s would be %d bytes, the limit is %d" % (out, len(raw), CARD_FILE_MAX))
    with open(out, "wb") as f:
        f.write(raw)
    print("%s: %d bytes, script %d bytes" % (os.path.relpath(out, ROOT), len(raw), len(script.encode("utf-8"))))


if __name__ == "__main__":
    paths = sys.argv[1:] or sorted(glob.glob(os.path.join(SRC, "*.json")))
    if not paths:
        sys.exit("nothing to pack in " + SRC)
    for p in paths:
        pack(p)
