#!/usr/bin/env python3
"""Serves the device's settings page on a PC, with a stand-in for the device behind it.

    tools/sim/webmock.py [port]        then open http://127.0.0.1:8765/

The page is cut out of MeteoPlaneRadar/WebPage.h as it is; nothing in it is
changed. The API answers are canned: enough for the page to load, and a working
imitation of the cards and store endpoints (the store offers what
cards/index.json lists). It exists to try the page's JavaScript in a real
browser without flashing anything. It does not run any firmware code - what the
device would answer is imitated here, so this checks the page, not the device.
"""
import json
import os
import re
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def page():
    with open(os.path.join(ROOT, "MeteoPlaneRadar", "WebPage.h"), encoding="utf-8") as f:
        src = f.read()
    return re.search(r'R"rawliteral\((.*?)\)rawliteral"', src, re.S).group(1)


CONFIG = {
    "version": "0.8.5", "lang": "cs", "ip": "192.168.1.50", "apMode": False, "hasPassword": False,
    "hasLoc": True, "lat": 50.0755, "lon": 14.4378, "metric": True, "topBearing": 0,
    "screens": {"clock": True, "planes": True, "meteo": True, "forecast": True,
                "price": False, "mix": False, "cards": True},
    "autoRotate": 0, "briDay": 80, "briNight": 20, "nightAuto": True, "nightOffset": 0,
    "clockFont": 1, "clockColor": 65535, "secColor": 1535, "secStyle": 0, "meteoLegend": True, "screenDots": False,
    "radarSrc": 0, "stormOnly": False, "stormDbz": 30, "stormKm": 30,
    "altMin": 0, "altMax": 15000, "altBands": [2000, 6000, 10000],
    "altColors": [2016, 64512, 65504, 1535], "onlyCallsign": False,
    "planeAlert": False, "planeAlertKm": 10, "planeAlertSec": 30, "squawkAlert": True, "watch": [],
    "windowOn": False, "windowAz": 180, "windowWidth": 90, "windowKm": 30,
    "priceFee": 0, "priceVat": 21, "mixCountry": "cz", "haEnabled": False, "wifi": [],
}
STATUS = {
    "ip": "192.168.1.50", "ssid": "doma", "rssi": -58, "uptime": "0 d 01:12", "heap": 181234,
    "psram": 7340032, "resetReason": "power on", "adsb": "OK", "radar": "OK", "forecast": "OK",
    "price": "", "mix": "", "cards": "kurzy-cnb: OK, 2/2 hodnot", "storm": "", "planeWatch": "",
    "timezone": "Europe/Prague", "version": "0.8.5", "board": "2.1", "screen": 6, "range": "",
    "enabled": [True, True, True, True, False, False, True, True],
}
installed = {}        # id -> card
state = {"scripts": False, "off": {}}     # the scripts switch; id -> why a script was stopped


def store_offer():
    with open(os.path.join(ROOT, "cards", "index.json"), encoding="utf-8") as f:
        idx = json.load(f)
    return {"serial": idx["serial"], "cards": [
        {"id": c["id"], "title": c["title"], "host": c["host"], "size": c["size"],
         "desc": c["desc"], "script": c.get("script", False),
         "installed": c["id"] in installed} for c in idx["cards"]]}


class Handler(BaseHTTPRequestHandler):
    def send(self, code, body, ctype="application/json"):
        data = body if isinstance(body, bytes) else json.dumps(body, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", ctype + "; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        path = urlparse(self.path).path
        if path == "/":
            self.send(200, page().encode("utf-8"), "text/html")
        elif path == "/api/config":
            self.send(200, CONFIG)
        elif path == "/api/status":
            self.send(200, STATUS)
        elif path == "/api/cards":
            cards = []
            for c in sorted(installed.values(), key=lambda c: c["id"]):
                row = {"id": c["id"], "title": c["title"], "url": c.get("url", ""), "ok": True, "tried": True}
                if c.get("script"):
                    row["script"] = True
                    row["off"] = c["id"] in state["off"]
                    if row["off"]:
                        row["why"] = state["off"][c["id"]]
                cards.append(row)
            self.send(200, {"ready": True, "max": 8, "scripts": state["scripts"], "cards": cards})
        elif path == "/api/store":
            self.send(200, store_offer())
        elif path in ("/api/wifilist", "/api/scan", "/api/geocode"):
            self.send(200, [])
        else:
            self.send(404, {"error": "not found"})

    def do_POST(self):
        path = urlparse(self.path).path
        raw = self.rfile.read(int(self.headers.get("Content-Length", 0) or 0))
        try:
            body = json.loads(raw.decode("utf-8")) if raw else {}
        except ValueError:
            return self.send(400, {"error": "json"})
        if path == "/api/cards":
            card = body.get("card") or {}
            if not re.match(r"^[a-z0-9-]{1,23}$", card.get("id", "")):
                return self.send(400, {"error": "id must be 1-23 characters of a-z, 0-9, -"})
            installed[card["id"]] = card
        elif path == "/api/cards/delete":
            if installed.pop(body.get("id", ""), None) is None:
                return self.send(404, {"error": "not found"})
        elif path == "/api/scripts":
            if isinstance(body.get("enabled"), bool):
                state["scripts"] = body["enabled"]
            state["off"].pop(body.get("on", ""), None)
        elif path == "/api/mock/stop":            # not a device endpoint: lets a test play a stopped script
            state["off"][body.get("id", "")] = body.get("why", "ran longer than 500 ms")
        elif path == "/api/store/install":
            cid = body.get("id", "")
            fpath = os.path.join(ROOT, "cards", cid + ".json")
            if not re.match(r"^[a-z0-9-]{1,23}$", cid) or not os.path.exists(fpath):
                return self.send(400, {"error": "no such card in the store"})
            with open(fpath, encoding="utf-8") as f:
                installed[cid] = json.load(f)
        self.send(200, {"ok": True})

    def log_message(self, fmt, *args):
        sys.stderr.write("webmock: " + fmt % args + "\n")


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
    print("http://127.0.0.1:%d/" % port, flush=True)
    HTTPServer(("127.0.0.1", port), Handler).serve_forever()
