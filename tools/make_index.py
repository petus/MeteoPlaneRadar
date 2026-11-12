#!/usr/bin/env python3
"""Builds and signs the card store index.

    tools/make_index.py [--key PRIVATE_KEY.pem] [--check]

Reads every card in cards/*.json, checks it the way the firmware would, and
writes

    cards/index.json       the list: id, file, size, SHA-256, title, host, description
    cards/index.json.sig   base64 of an ECDSA P-256 / SHA-256 signature of index.json

The device accepts the index only if the signature matches the public key in
MeteoPlaneRadar/StoreKey.h, so the private key must be the other half of that
one. It is looked for in --key, then in $MPR_STORE_KEY, then in
~/.config/meteoplaneradar/store-signing-key.pem. It never belongs in the
repository.

Descriptions come from cards/store-meta.json ({"<id>": {"cs": "...", "en": "..."}}).

The serial number goes up by one whenever the list changes; a device refuses
an index older than one it has accepted. Running the tool again without
changes leaves both files alone. --check only verifies what is there (index
against the cards, signature against StoreKey.h) and needs no private key.

Needs Python 3 and the openssl command.
"""
import argparse
import base64
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile
from urllib.parse import urlparse

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CARDS = os.path.join(ROOT, "cards")
INDEX = os.path.join(CARDS, "index.json")
SIG = INDEX + ".sig"
META = os.path.join(CARDS, "store-meta.json")
KEY_H = os.path.join(ROOT, "MeteoPlaneRadar", "StoreKey.h")
DEFAULT_KEY = os.path.expanduser("~/.config/meteoplaneradar/store-signing-key.pem")
NOT_CARDS = {"index.json", "store-meta.json"}

ID_RE = re.compile(r"^[a-z0-9-]{1,23}$")
CARD_FILE_MAX = 2048          # Config.h
LIMITS = {"title": 27, "url": 199, "label": 19, "unit": 9, "path": 71}   # bytes, Cards.h


def fail(msg):
    sys.exit("make_index: " + msg)


def check_card(name, raw):
    """The checks Cards.cpp parseCard() makes that do not need the path parser."""
    if len(raw) > CARD_FILE_MAX:
        fail("%s is %d bytes, the limit is %d" % (name, len(raw), CARD_FILE_MAX))
    try:
        card = json.loads(raw.decode("utf-8"))
    except ValueError as e:
        fail("%s is not valid JSON: %s" % (name, e))
    cid = card.get("id", "")
    if not ID_RE.match(cid):
        fail("%s: id must be 1-23 characters of a-z, 0-9, -" % name)
    if name != cid + ".json":
        fail("%s: the file must be called %s.json" % (name, cid))
    if card.get("v", 1) != 1:
        fail("%s: unsupported card version" % name)
    for key in ("title", "url"):
        val = card.get(key, "")
        if not val or len(val.encode("utf-8")) > LIMITS[key]:
            fail("%s: %s is missing or longer than %d bytes" % (name, key, LIMITS[key]))
    url = urlparse(card["url"])
    if url.scheme not in ("http", "https") or not url.hostname:
        fail("%s: url must start with http:// or https://" % name)
    rows = card.get("rows")
    if not isinstance(rows, list) or not 1 <= len(rows) <= 4:
        fail("%s: rows must hold 1 to 4 items" % name)
    for row in rows:
        if not row.get("path") or len(row["path"].encode("utf-8")) > LIMITS["path"]:
            fail("%s: a row path is missing or too long" % name)
        for key in ("label", "unit"):
            if len(row.get(key, "").encode("utf-8")) > LIMITS[key]:
                fail("%s: a row %s is longer than %d bytes" % (name, key, LIMITS[key]))
    return card, url.hostname


def build_entries():
    meta = {}
    if os.path.exists(META):
        with open(META, encoding="utf-8") as f:
            meta = json.load(f)
    entries = []
    for name in sorted(os.listdir(CARDS)):
        if not name.endswith(".json") or name in NOT_CARDS:
            continue
        with open(os.path.join(CARDS, name), "rb") as f:
            raw = f.read()
        card, host = check_card(name, raw)
        desc = meta.get(card["id"], {})
        entries.append({
            "id": card["id"],
            "file": name,
            "size": len(raw),
            "sha256": hashlib.sha256(raw).hexdigest(),
            "title": card["title"],
            "host": host,
            "desc": {"cs": desc.get("cs", ""), "en": desc.get("en", "")},
        })
    if not entries:
        fail("no cards found in " + CARDS)
    return entries


def public_key_pem():
    """The key the firmware checks against, cut out of StoreKey.h."""
    with open(KEY_H, encoding="utf-8") as f:
        lines = re.findall(r'"([^"]*)\\n"', f.read())
    pem = "\n".join(lines) + "\n"
    if "BEGIN PUBLIC KEY" not in pem:
        fail("cannot find the public key in " + KEY_H)
    return pem


def signature_ok(index_bytes, sig_b64):
    with tempfile.TemporaryDirectory() as tmp:
        paths = {n: os.path.join(tmp, n) for n in ("pub.pem", "index", "sig.der")}
        with open(paths["pub.pem"], "w") as f:
            f.write(public_key_pem())
        with open(paths["index"], "wb") as f:
            f.write(index_bytes)
        try:
            with open(paths["sig.der"], "wb") as f:
                f.write(base64.b64decode(sig_b64, validate=True))
        except ValueError:
            return False
        r = subprocess.run(["openssl", "dgst", "-sha256", "-verify", paths["pub.pem"],
                            "-signature", paths["sig.der"], paths["index"]],
                           capture_output=True, text=True)
        return r.returncode == 0 and "Verified OK" in r.stdout


def sign(index_bytes, key_path):
    with tempfile.TemporaryDirectory() as tmp:
        src, out = os.path.join(tmp, "index"), os.path.join(tmp, "sig.der")
        with open(src, "wb") as f:
            f.write(index_bytes)
        r = subprocess.run(["openssl", "dgst", "-sha256", "-sign", key_path, "-out", out, src],
                           capture_output=True, text=True)
        if r.returncode != 0:
            fail("openssl could not sign: " + r.stderr.strip())
        with open(out, "rb") as f:
            return base64.b64encode(f.read()).decode("ascii")


def read_existing():
    if not (os.path.exists(INDEX) and os.path.exists(SIG)):
        return None, None, None
    with open(INDEX, "rb") as f:
        raw = f.read()
    with open(SIG, encoding="ascii") as f:
        sig = f.read().strip()
    try:
        return raw, json.loads(raw.decode("utf-8")), sig
    except ValueError:
        return raw, None, sig


def main():
    ap = argparse.ArgumentParser(description="Build and sign cards/index.json")
    ap.add_argument("--key", help="private key (PEM); default $MPR_STORE_KEY or " + DEFAULT_KEY)
    ap.add_argument("--check", action="store_true", help="verify the existing index and signature only")
    args = ap.parse_args()

    entries = build_entries()
    old_raw, old, old_sig = read_existing()

    if args.check:
        if old is None:
            fail("there is no readable index to check")
        if old.get("cards") != entries:
            fail("cards/index.json does not match the cards - run tools/make_index.py")
        if not signature_ok(old_raw, old_sig):
            fail("the signature does not match the key in StoreKey.h")
        print("index serial %d, %d card(s), signature OK" % (old.get("serial", 0), len(entries)))
        return

    if old is not None and old.get("cards") == entries and signature_ok(old_raw, old_sig):
        print("index serial %d is up to date, nothing written" % old.get("serial", 0))
        return

    key_path = args.key or os.environ.get("MPR_STORE_KEY") or DEFAULT_KEY
    if not os.path.exists(key_path):
        fail("no private key at %s (use --key or $MPR_STORE_KEY)" % key_path)

    serial = (old.get("serial", 0) if old else 0) + 1
    index = {"v": 1, "serial": serial, "cards": entries}
    raw = (json.dumps(index, ensure_ascii=False, indent=1) + "\n").encode("utf-8")
    sig = sign(raw, key_path)
    if not signature_ok(raw, sig):
        fail("the private key does not belong to the public key in StoreKey.h - nothing written")
    with open(INDEX, "wb") as f:
        f.write(raw)
    with open(SIG, "w", encoding="ascii") as f:
        f.write(sig + "\n")
    print("wrote index serial %d with %d card(s) and its signature" % (serial, len(entries)))


if __name__ == "__main__":
    main()
