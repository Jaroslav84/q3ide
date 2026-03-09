#!/usr/bin/env python3
"""
Map load tester — runs each map via remote API, verifies load via RCON status.
Usage: python3 scripts/test_maps.py
"""

import json
import os
import time
import urllib.request

BASE = f"http://{os.environ.get('Q3IDE_API_HOST', 'host.docker.internal')}:{os.environ.get('Q3IDE_API_PORT', '6666')}"

MAPS = [
    "lun3dm1",
    "lun3dm2",
    "lun3dm3-cpm",   # BSP name inside lun3dm3-cpm.pk3
    "lun3dm4",
    "lun3dm5",
    "lun3_20b1",
    "cpm18r",
    "acid3dm12",
    "ori_apt",
    "q3ctfchnu01",
    "QuadCTF",       # BSP name inside QuadCTF.pk3
    "quatrix",
    "r7-blockworld1",
]


def api(method, path, body=None, timeout=60):
    data = json.dumps(body).encode() if body else None
    headers = {"Content-Type": "application/json"} if data else {}
    req = urllib.request.Request(f"{BASE}{path}", data=data, method=method, headers=headers)
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read())


def stop_game():
    try:
        api("POST", "/stop")
        time.sleep(2)
    except Exception:
        pass


def run_map(level):
    stop_game()
    api("POST", "/run", {"args": ["--level", level]})


def rcon(cmd, retries=5, delay=2):
    for _ in range(retries):
        try:
            r = api("POST", "/console", {"cmd": cmd})
            return r.get("response", "")
        except Exception:
            time.sleep(delay)
    return ""


def get_engine_log_tail(n=80):
    try:
        r = api("GET", f"/logs?file=engine&n={n}")
        return r.get("content", "").splitlines()
    except Exception:
        return []


def detect_map_in_log(level, lines):
    """Return True if engine log shows the map was loaded (not main menu)."""
    needle = level.lower()
    for line in lines:
        l = line.lower()
        if needle in l and ("loading" in l or "map" in l or "bsp" in l or "devmap" in l):
            return True
    return False


def check_rcon_status(level):
    """Send 'status' RCON, return map name found or None."""
    resp = rcon("status")
    if not resp:
        return None
    for line in resp.splitlines():
        if "map:" in line.lower():
            return line.strip()
    return None


results = []
print(f"\n{'='*60}")
print(f"  MAP LOAD TEST — {len(MAPS)} maps")
print(f"{'='*60}\n")

for mapname in MAPS:
    print(f"  → {mapname} ... ", end="", flush=True)
    try:
        run_map(mapname)
        # Wait for game to start and map to load
        time.sleep(12)

        # Check RCON status for map name
        status_line = check_rcon_status(mapname)
        log_lines = get_engine_log_tail(100)
        log_hit = detect_map_in_log(mapname, log_lines)

        if status_line and mapname.lower() in status_line.lower():
            verdict = "PASS"
            detail = status_line.strip()
        elif log_hit:
            verdict = "PASS"
            detail = "(log confirms load)"
        else:
            # Check if it's at main menu (map: q3dm0 or no map)
            if status_line:
                verdict = "FAIL"
                detail = f"wrong map: {status_line.strip()}"
            else:
                # Check engine log for "couldn't load" or "not found"
                missing = any(
                    ("couldn" in l.lower() or "not found" in l.lower() or "invalid map" in l.lower())
                    and mapname.lower() in l.lower()
                    for l in log_lines
                )
                verdict = "FAIL"
                detail = "not found / main menu" if missing else "no map confirmation in log"

        results.append((mapname, verdict, detail))
        icon = "✓" if verdict == "PASS" else "✗"
        print(f"{icon} {verdict} — {detail}")

    except Exception as e:
        results.append((mapname, "ERROR", str(e)))
        print(f"✗ ERROR — {e}")

    stop_game()
    time.sleep(3)

print(f"\n{'='*60}")
print(f"  RESULTS")
print(f"{'='*60}")
passed = [r for r in results if r[1] == "PASS"]
failed = [r for r in results if r[1] != "PASS"]
for name, verdict, detail in results:
    icon = "✓" if verdict == "PASS" else "✗"
    print(f"  {icon} {name:<25} {verdict:<6} {detail}")
print(f"\n  {len(passed)}/{len(results)} passed\n")
