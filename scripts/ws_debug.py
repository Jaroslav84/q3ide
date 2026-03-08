#!/usr/bin/env python3
"""
Q3IDE WebSocket debug client — for use inside Docker.

Usage (interactive):
    python3 scripts/ws_debug.py

Usage (import):
    from scripts.ws_debug import Q3Debug
    dbg = Q3Debug()
    dbg.connect()
    dbg.cmd('q3ide status')
    dbg.watch(seconds=10)
    dbg.disconnect()
"""

import base64
import json
import os
import socket
import struct
import sys
import threading
import time

HOST = os.environ.get('Q3IDE_API_HOST', 'host.docker.internal')
PORT = int(os.environ.get('Q3IDE_API_PORT', 6666))


# ── minimal stdlib WebSocket client ──────────────────────────────────────────

def _ws_connect(host, port, path='/ws?logs=engine,multimon,capture'):
    key = base64.b64encode(os.urandom(16)).decode()
    sock = socket.create_connection((host, port), timeout=10)
    handshake = (
        f"GET {path} HTTP/1.1\r\n"
        f"Host: {host}:{port}\r\n"
        f"Upgrade: websocket\r\n"
        f"Connection: Upgrade\r\n"
        f"Sec-WebSocket-Key: {key}\r\n"
        f"Sec-WebSocket-Version: 13\r\n"
        "\r\n"
    ).encode()
    sock.sendall(handshake)
    # Read response headers
    buf = b''
    while b'\r\n\r\n' not in buf:
        buf += sock.recv(1024)
    if b'101' not in buf:
        raise ConnectionError(f'WS handshake failed: {buf[:200]}')
    sock.settimeout(None)
    return sock


def _ws_send(sock, text):
    """Send masked text frame (client→server must be masked per RFC 6455)."""
    data = text.encode() if isinstance(text, str) else text
    n = len(data)
    mask = os.urandom(4)
    masked = bytes(b ^ mask[i % 4] for i, b in enumerate(data))
    if n < 126:
        header = bytes([0x81, 0x80 | n])
    elif n < 65536:
        header = bytes([0x81, 0xFE]) + struct.pack('>H', n)
    else:
        header = bytes([0x81, 0xFF]) + struct.pack('>Q', n)
    sock.sendall(header + mask + masked)


def _ws_recv(sock):
    """Read one frame. Returns decoded str or None on close."""
    def _read(n):
        buf = b''
        while len(buf) < n:
            chunk = sock.recv(n - len(buf))
            if not chunk:
                raise ConnectionError
            buf += chunk
        return buf

    h = _read(2)
    opcode = h[0] & 0x0F
    if opcode == 8:
        return None
    masked = bool(h[1] & 0x80)
    length = h[1] & 0x7F
    if length == 126:
        length = struct.unpack('>H', _read(2))[0]
    elif length == 127:
        length = struct.unpack('>Q', _read(8))[0]
    mask = _read(4) if masked else b'\x00\x00\x00\x00'
    payload = _read(length)
    raw = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
    return raw.decode(errors='replace')


# ── Q3Debug class ─────────────────────────────────────────────────────────────

class Q3Debug:
    """
    WebSocket debug session with the Q3IDE bridge.

    Messages received (JSON):
        {"type": "log",    "file": "engine"|"multimon"|"capture", "line": "..."}
        {"type": "rcon",   "cmd": "...", "response": "...", "ok": true}
        {"type": "status", "running": bool, "pid": int|null, "uptime_s": int|null}
        {"type": "hello",  "logs": [...], "rcon_port": 27960}
    """

    def __init__(self, host=HOST, port=PORT, logs='engine,multimon,capture'):
        self.host = host
        self.port = port
        self.logs = logs
        self._sock = None
        self._alive = False
        self._recv_thread = None
        self._pending = {}   # cmd → Event + result
        self._lock = threading.Lock()
        self.on_log = None    # callback(file, line)
        self.on_status = None # callback(status_dict)

    def connect(self):
        path = f'/ws?logs={self.logs}'
        self._sock = _ws_connect(self.host, self.port, path)
        self._alive = True
        self._recv_thread = threading.Thread(target=self._recv_loop, daemon=True)
        self._recv_thread.start()
        # wait for hello
        time.sleep(0.3)
        return self

    def disconnect(self):
        self._alive = False
        if self._sock:
            try:
                self._sock.close()
            except OSError:
                pass

    def cmd(self, rcon_cmd, timeout=5.0):
        """Send RCON command, return response string (blocks up to timeout)."""
        ev = threading.Event()
        result = [None]
        with self._lock:
            self._pending[rcon_cmd] = (ev, result)
        _ws_send(self._sock, json.dumps({'cmd': rcon_cmd}))
        ev.wait(timeout)
        with self._lock:
            self._pending.pop(rcon_cmd, None)
        return result[0]

    def watch(self, seconds=30, filter_fn=None):
        """
        Block for `seconds`, printing log lines as they arrive.
        filter_fn(file, line) → bool — return False to suppress a line.
        """
        deadline = time.time() + seconds
        # Install a temporary log callback
        printed = []
        orig = self.on_log
        def _print(file, line):
            if filter_fn is None or filter_fn(file, line):
                msg = f'[{file}] {line}'
                print(msg)
                printed.append(msg)
            if orig:
                orig(file, line)
        self.on_log = _print
        try:
            while time.time() < deadline and self._alive:
                time.sleep(0.1)
        finally:
            self.on_log = orig
        return printed

    def _recv_loop(self):
        while self._alive:
            try:
                raw = _ws_recv(self._sock)
            except (OSError, ConnectionError):
                self._alive = False
                break
            if raw is None:
                self._alive = False
                break
            try:
                msg = json.loads(raw)
            except json.JSONDecodeError:
                continue

            t = msg.get('type')
            if t == 'log' and self.on_log:
                self.on_log(msg.get('file', '?'), msg.get('line', ''))
            elif t == 'status' and self.on_status:
                self.on_status(msg)
            elif t == 'rcon':
                orig_cmd = msg.get('cmd', '')
                with self._lock:
                    entry = self._pending.get(orig_cmd)
                if entry:
                    ev, result = entry
                    result[0] = msg.get('response') or msg.get('error')
                    ev.set()

    def __enter__(self):
        return self.connect()

    def __exit__(self, *_):
        self.disconnect()


# ── interactive CLI ───────────────────────────────────────────────────────────

def _interactive():
    print(f'Connecting to ws://{HOST}:{PORT}/ws ...')
    try:
        dbg = Q3Debug()
        dbg.connect()
    except Exception as e:
        print(f'ERROR: {e}')
        sys.exit(1)

    print(f'Connected. Streaming logs. Type RCON commands or Ctrl-C to exit.\n')

    def on_log(file, line):
        print(f'\r[{file}] {line}')

    dbg.on_log = on_log

    try:
        while True:
            try:
                line = input('rcon> ').strip()
            except EOFError:
                break
            if not line:
                continue
            if line in ('q', 'quit', 'exit'):
                break
            resp = dbg.cmd(line, timeout=5)
            print(f'  → {resp}')
    except KeyboardInterrupt:
        pass
    finally:
        dbg.disconnect()
        print('\nDisconnected.')


if __name__ == '__main__':
    _interactive()
