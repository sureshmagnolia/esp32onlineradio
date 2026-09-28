#!/usr/bin/env python3
"""
CrowPanel 1.28" ESP32-S3 HMI Rotary Radio — Local Simulation Server
Simulates the ESP32 SoftAP Web Server (port 8080)
Serves Web Remote at '/', Hardware Simulator at '/sim', and ESP32 REST APIs
"""

import http.server
import socketserver
import json
import os
import urllib.parse
from functools import partial

PORT = 8080
DIRECTORY = os.path.dirname(os.path.abspath(__file__))

STATE = {
    "title": "Akashvani Thrissur",
    "meta": "Malayalam • Kerala",
    "idx": 0,
    "playing": True,
    "buffering": False,
    "vol": 18,
    "muted": False,
    "ledMode": 0,
    "ledBright": 35,
    "isPoweredOn": True,
    "favorites": [
        {"name": "Akashvani Thrissur", "state": "Kerala", "lang": "Malayalam", "url": "https://radio.wavespb.com/live/f70fdeca437dc326/f70fdeca437dc326.m3u8"},
        {"name": "FM Rainbow Kochi", "state": "Kerala", "lang": "Malayalam", "url": "https://radio.wavespb.com/live/7df6f2a8c3c4d33b/7df6f2a8c3c4d33b.m3u8"},
        {"name": "Vividh Bharati", "state": "National", "lang": "Hindi", "url": "https://radio.wavespb.com/live/146ed6ec6dea5a24/146ed6ec6dea5a24.m3u8"},
        {"name": "VB Malayalam", "state": "Kerala", "lang": "Malayalam", "url": "https://radio.wavespb.com/live/ad3a8436a329e2d6/ad3a8436a329e2d6.m3u8"},
        {"name": "FM Gold Delhi", "state": "Delhi", "lang": "Hindi", "url": "https://airhlspush.pc.cdn.bitgravity.com/httppush/hlspbaudio005/hlspbaudio005_Auto.m3u8"},
        {"name": "Raagam Classical", "state": "National", "lang": "Classical", "url": "https://airhlspush.pc.cdn.bitgravity.com/httppush/hlspbaudioragam/hlspbaudioragam_Auto.m3u8"},
        {"name": "Akashvani Calicut", "state": "Kerala", "lang": "Malayalam", "url": "https://radio.wavespb.com/live/8321393de70015fc/8321393de70015fc.m3u8"}
    ]
}

class RadioHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path
        qs = urllib.parse.parse_qs(parsed.query)

        # Route root to web remote
        if path == "/" or path == "/index.html":
            self.path = "/web_remote.html"
            return super().do_GET()

        # Route /sim to simulator
        if path == "/sim" or path == "/simulator":
            self.path = "/index.html"
            return super().do_GET()

        # REST API: /api/status
        if path == "/api/status":
            payload = json.dumps(STATE).encode('utf-8')
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(payload)))
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(payload)
            return

        # REST API: /api/cmd
        if path == "/api/cmd":
            action = qs.get("action", [""])[0]
            if action == "play_pause":
                STATE["playing"] = not STATE["playing"]
            elif action == "next":
                STATE["idx"] = (STATE["idx"] + 1) % len(STATE["favorites"])
                cur = STATE["favorites"][STATE["idx"]]
                STATE["title"] = cur["name"]
                STATE["meta"] = f"{cur.get('lang','')} • {cur.get('state','')}"
                STATE["playing"] = True
            elif action == "prev":
                STATE["idx"] = (STATE["idx"] - 1 + len(STATE["favorites"])) % len(STATE["favorites"])
                cur = STATE["favorites"][STATE["idx"]]
                STATE["title"] = cur["name"]
                STATE["meta"] = f"{cur.get('lang','')} • {cur.get('state','')}"
                STATE["playing"] = True
            elif action == "mute":
                STATE["muted"] = not STATE["muted"]
            elif action == "vol":
                v = int(qs.get("val", [18])[0])
                STATE["vol"] = v
                STATE["muted"] = (v == 0)

            payload = json.dumps({"ok": True, "state": STATE}).encode('utf-8')
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(payload)))
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(payload)
            return

        # REST API: /api/play
        if path == "/api/play":
            st_id = int(qs.get("id", [0])[0])
            if 0 <= st_id < len(STATE["favorites"]):
                STATE["idx"] = st_id
                cur = STATE["favorites"][st_id]
                STATE["title"] = cur["name"]
                STATE["meta"] = f"{cur.get('lang','')} • {cur.get('state','')}"
                STATE["playing"] = True

            payload = json.dumps({"ok": True, "state": STATE}).encode('utf-8')
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(payload)))
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(payload)
            return

        # REST API: /api/favorites
        if path == "/api/favorites":
            payload = json.dumps(STATE["favorites"]).encode('utf-8')
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(payload)))
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(payload)
            return

        return super().do_GET()

if __name__ == "__main__":
    Handler = partial(RadioHTTPRequestHandler, directory=DIRECTORY)
    with socketserver.TCPServer(("127.0.0.1", PORT), Handler) as httpd:
        print(f"Server ready at http://localhost:{PORT}/")
        httpd.serve_forever()
