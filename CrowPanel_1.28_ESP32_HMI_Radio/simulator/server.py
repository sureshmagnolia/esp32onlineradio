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

import threading
import time

PORTS = [8080, 8888]
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
    "isPoweredOn": False,  # Start in Standby by default (matches cold boot hardware state)
    "standby": True,
    "urlOffline": False,
    "favorites": [
        {"name": "AIR Thrissur", "state": "Kerala", "lang": "Malayalam", "url": "https://d1cvqgmbcpg5yn.cloudfront.net/f70fdeca437dc326/f70fdeca437dc326.m3u8"},
        {"name": "AIR Calicut", "state": "Kerala", "lang": "Malayalam", "url": "https://d1cvqgmbcpg5yn.cloudfront.net/8321393de70015fc/8321393de70015fc.m3u8"},
        {"name": "FM Rainbow Kochi", "state": "Kerala", "lang": "Malayalam", "url": "https://d1cvqgmbcpg5yn.cloudfront.net/7df6f2a8c3c4d33b/7df6f2a8c3c4d33b.m3u8"},
        {"name": "Vividh Bharati", "state": "National", "lang": "Hindi", "url": "https://radio.wavespb.com/live/146ed6ec6dea5a24/146ed6ec6dea5a24.m3u8"},
        {"name": "VB Malayalam", "state": "Kerala", "lang": "Malayalam", "url": "https://d3hrxqn1tritdh.cloudfront.net/ad3a8436a329e2d6/ad3a8436a329e2d6.m3u8"},
        {"name": "FM Gold Delhi", "state": "Delhi", "lang": "Hindi", "url": "https://airhlspush.pc.cdn.bitgravity.com/httppush/hlspbaudio005/hlspbaudio005_Auto.m3u8"},
        {"name": "Ahalia FM 90.4", "state": "Kerala", "lang": "Malayalam", "url": "https://cast1.my-control-panel.com/proxy/ahaliafm/stream"}
    ],
    "silicon": {
        "chip": "ESP32-S3 (revision v0.2)",
        "cores": 2,
        "arch": "Xtensa® 32-bit LX7 Dual-Core @ 240 MHz",
        "cpu0": {"role": "Audio DMA / Wi-Fi LwIP", "load": 24, "task": "AudioTask", "freq_mhz": 240},
        "cpu1": {"role": "LovyanGFX / LVGL HMI", "load": 41, "task": "gui_task", "freq_mhz": 240},
        "temp_c": 41.5,
        "sram": {
            "total_bytes": 327680,
            "used_bytes": 64076,
            "free_bytes": 263604,
            "min_free_bytes": 210432,
            "max_alloc_bytes": 184320
        },
        "psram": {
            "chip": "8MB Octal OPI PSRAM @ 80 MHz",
            "total_bytes": 8388608,
            "used_bytes": 377344,
            "free_bytes": 8011264,
            "ring_buf_total": 262144,
            "ring_buf_used": 196608,
            "framebuffer_bytes": 115200
        },
        "flash": {
            "chip": "16MB Quad SPI Flash",
            "table": "app3M_fat9M_16MB.csv",
            "partitions": [
                {"name": "nvs", "type": "data", "subtype": "nvs", "offset": "0x9000", "size": "20 KB", "used": "8 KB"},
                {"name": "otadata", "type": "data", "subtype": "ota", "offset": "0xe000", "size": "8 KB", "used": "4 KB"},
                {"name": "app0", "type": "app", "subtype": "factory", "offset": "0x10000", "size": "3.0 MB", "used": "2.48 MB (82.8%)"},
                {"name": "fatfs", "type": "data", "subtype": "fat", "offset": "0x310000", "size": "9.0 MB", "used": "320 KB"}
            ]
        },
        "buses": {
            "fspi": {"peripheral": "FSPI (SPI2 Host)", "speed": "80.0 MHz", "mode": 0, "dma": "Auto Channel", "fps": 15.6},
            "i2s": {"peripheral": "I2S0 Audio DAC", "rate": "48.0 kHz", "bits": 16, "bclk": "1.536 MHz", "lrc": "48 kHz", "dout_pin": 43},
            "i2c": {"peripheral": "I2C0 Touch Controller", "speed": "400 kHz Fast Mode", "sda_pin": 4, "scl_pin": 5},
            "ledc": {"peripheral": "LEDC PWM Timer 0", "freq": "5.0 kHz", "res": "8-bit", "pin": 46},
            "rmt": {"peripheral": "RMT Peripheral", "freq": "800 kHz", "channel": 0, "pin": 42},
            "usb_cdc": {"status": "USB Serial/JTAG APB Lock Active", "brick_risk": "Zero (CDC Uninterruptible)"}
        }
    }
}

class RadioHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        # Force aggressive anti-caching & wipe all old client-side storage from any previous app
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")
        self.send_header("Pragma", "no-cache")
        self.send_header("Expires", "0")
        self.send_header("Clear-Site-Data", '"cache", "storage"')
        super().end_headers()

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path
        qs = urllib.parse.parse_qs(parsed.query)

        # Self-destruct any stale ServiceWorker that the browser tries to fetch
        if path in ("/sw.js", "/service-worker.js", "/worker.js"):
            sw_killer = (
                "self.addEventListener('install', e => self.skipWaiting());\n"
                "self.addEventListener('activate', e => {\n"
                "  self.registration.unregister().then(() => self.clients.matchAll())\n"
                "  .then(clients => clients.forEach(c => c.navigate(c.url)));\n"
                "});\n"
            ).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/javascript")
            self.send_header("Content-Length", str(len(sw_killer)))
            self.end_headers()
            self.wfile.write(sw_killer)
            return

        # Route root or /sim to Virtual CrowPanel Display
        if path in ("/", "/sim", "/simulator", "/index.html"):
            self.path = "/index.html"
            return super().do_GET()

        # Route /remote to Web Remote
        if path in ("/remote", "/remote.html", "/web_remote", "/web_remote.html"):
            self.path = "/web_remote.html"
            return super().do_GET()

        # REST API: /api/silicon
        if path == "/api/silicon":
            payload = json.dumps(STATE["silicon"]).encode('utf-8')
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(payload)))
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(payload)
            return

        # REST API: /api/power
        if path == "/api/power":
            action = qs.get("action", [""])[0]
            if action == "wake" or action == "on":
                STATE["isPoweredOn"] = True
                STATE["standby"] = False
            elif action == "standby" or action == "off":
                STATE["isPoweredOn"] = False
                STATE["standby"] = True
                STATE["playing"] = False
            elif action == "toggle":
                STATE["isPoweredOn"] = not STATE["isPoweredOn"]
                STATE["standby"] = not STATE["isPoweredOn"]
                if STATE["standby"]:
                    STATE["playing"] = False

            payload = json.dumps({"ok": True, "state": STATE}).encode('utf-8')
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(payload)))
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(payload)
            return

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

class ReuseAddrServer(socketserver.TCPServer):
    allow_reuse_address = True

def run_server(port):
    Handler = partial(RadioHTTPRequestHandler, directory=DIRECTORY)
    try:
        with ReuseAddrServer(("0.0.0.0", port), Handler) as httpd:
            print(f"CrowPanel Virtual Server running at http://localhost:{port}/")
            httpd.serve_forever()
    except Exception as e:
        print(f"Notice on port {port}: {e}")

if __name__ == "__main__":
    threads = []
    for p in PORTS:
        t = threading.Thread(target=run_server, args=(p,), daemon=True)
        t.start()
        threads.append(t)
    print("Dual servers online on http://localhost:8080/ and http://localhost:8888/")
    while True:
        time.sleep(1)
