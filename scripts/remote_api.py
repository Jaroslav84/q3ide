#!/usr/bin/env python3
"""
Q3IDE Remote API — Debug Bridge (Docker <-> macOS)
Run on macOS: python3 scripts/remote_api.py
Claude calls:  http://host.docker.internal:6666/...
               ws://host.docker.internal:6666/ws
"""

import base64
import hashlib
import json
import os
import platform
import signal
import socket
import struct
import subprocess
import sys
import threading
import time
import uuid
from collections import deque
from http.server import BaseHTTPRequestHandler
from http.server import ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

ROOT = Path(__file__).resolve().parent.parent
LOG_DIR = ROOT / "logs"
BUILD_SCRIPT = ROOT / "scripts" / "build.sh"
PID_FILE = Path("/tmp/q3ide_game.pid")

def _q3_binary():
    """Return (binary_path, build_dir) or (None, None)."""
    import sys as _sys
    arch = platform.machine()
    q3e_arch = 'arm64' if arch == 'arm64' else 'x86_64'
    os_name = 'darwin' if _sys.platform == 'darwin' else 'linux'
    build_dir = ROOT / 'quake3e' / 'build' / f'release-{os_name}-{q3e_arch}'
    binary = build_dir / f'quake3e.{q3e_arch}'
    if binary.exists():
        return binary, build_dir
    return None, None

def _build_engine_args(args):
    """Convert API args list (['--level','0','--execute','q3ide attach all']) to engine args."""
    engine_args = []
    i = 0
    while i < len(args):
        a = str(args[i])
        if a == '--level' and i + 1 < len(args):
            level = str(args[i + 1])
            if level.isdigit() or (len(level) <= 2 and level.lstrip('-').isdigit()):
                level = f'q3dm{level}'
            engine_args += ['+map', level]
            i += 2
        elif a == '--bots' and i + 1 < len(args):
            bots = int(args[i + 1])
            engine_args += ['+set', 'bot_minplayers', str(bots + 1)]
            i += 2
        elif a == '--execute' and i + 1 < len(args):
            engine_args += ['+set', 'nextdemo', str(args[i + 1])]
            i += 2
        else:
            engine_args.append(a)
            i += 1
    return engine_args

PORT = int(os.environ.get("Q3IDE_API_PORT", 6666))
RCON_PASSWORD = os.environ.get("Q3IDE_RCON_PASSWORD", "q3idedev666")
CLIENT_HOST = os.environ.get("Q3IDE_API_HOST", "host.docker.internal")
RCON_HOST = "127.0.0.1"
RCON_PORT = 27960

LOG_ALIASES = {
    "engine":  LOG_DIR / "engine.log",
    "multimon": LOG_DIR / "q3ide_multimon.log",
    "capture": LOG_DIR / "q3ide_capture.log",
    "build":   LOG_DIR / "build.log",
}

_game_proc = None
_game_start = None
_build_proc = None
_build_start = None
_build_timeout = 120  # seconds; set per-build based on --clean

# ── build queue ───────────────────────────────────────────────────────────────
# Each entry: {id, agent_id, args, run_args, auto_run, queued_at,
#              started_at, finished_at, status, returncode}
# status: 'queued' | 'building' | 'done' | 'failed' | 'cancelled'
_queue = deque()          # pending items (not yet started)
_queue_history = []       # completed / cancelled items (last 20)
_queue_lock = threading.Lock()
_queue_event = threading.Event()  # wakes worker when something is enqueued
_queue_current = None     # item currently being built

WS_GUID = b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


# ── game helpers ──────────────────────────────────────────────────────────────

def _game_pid():
    global _game_proc
    if _game_proc is not None:
        if _game_proc.poll() is None:
            return _game_proc.pid
        _game_proc = None
    if PID_FILE.exists():
        try:
            pid = int(PID_FILE.read_text().strip())
            os.kill(pid, 0)
            return pid
        except (ValueError, OSError):
            PID_FILE.unlink(missing_ok=True)
    return None


def _game_running():
    if _game_pid() is not None:
        return True
    # Fallback: check if quake3e process exists
    try:
        r = subprocess.run(['pgrep', '-f', 'quake3e'], capture_output=True)
        return r.returncode == 0
    except Exception:
        return False


def _kill_game():
    global _game_proc, _game_start
    killed = False
    # Kill by tracked PID
    pid = _game_pid()
    if pid is not None:
        try:
            os.kill(pid, signal.SIGTERM)
            time.sleep(0.5)
            try:
                os.kill(pid, 0)
                os.kill(pid, signal.SIGKILL)
            except OSError:
                pass
            killed = True
        except OSError:
            pass
    # Also kill any quake3e process by name (catches detached child)
    try:
        subprocess.run(['pkill', '-f', 'quake3e'], capture_output=True)
        killed = True
    except Exception:
        pass
    if _game_proc is not None:
        try:
            _game_proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            pass
        _game_proc = None
    _game_start = None
    PID_FILE.unlink(missing_ok=True)
    return killed


def _send_rcon(cmd):
    msg = b'\xff\xff\xff\xff' + f'rcon {RCON_PASSWORD} {cmd}\n'.encode()
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(2.0)
    try:
        sock.sendto(msg, (RCON_HOST, RCON_PORT))
        data = sock.recv(4096)
        text = data[4:].decode(errors='replace').strip()
        if text.startswith('print\n'):
            text = text[6:]
        return text
    except socket.timeout:
        return None
    finally:
        sock.close()


def _do_run(args=None, agent_id=''):
    """Launch the game binary. Returns dict with ok/pid/error."""
    global _game_proc, _game_start
    if args is None:
        args = ['--level', '0', '--execute', 'q3ide attach all']
    if _game_running():
        _kill_game()
        time.sleep(1)
    binary, build_dir = _q3_binary()
    if binary is None:
        return {'ok': False, 'error': 'quake3e binary not found — build first'}
    engine_args = _build_engine_args(args)
    cmd = [str(binary)] + engine_args
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    engine_log = LOG_DIR / 'engine.log'
    log_fh = open(engine_log, 'a')
    separator = '═' * 60
    log_fh.write(f'\n{separator}\n'
                 f'# session  {time.strftime("%Y-%m-%dT%H:%M:%S")}'
                 f'  agent={agent_id or "unknown"}  args={args}\n'
                 f'{separator}\n')
    log_fh.flush()
    print(f'[game] Launching: {" ".join(cmd)}', flush=True)
    try:
        _game_proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                      cwd=str(build_dir))
        _game_start = time.time()
        PID_FILE.write_text(str(_game_proc.pid))
        threading.Thread(target=_tee_output, args=(_game_proc, log_fh, '[game] '),
                         daemon=True).start()
        return {'ok': True, 'pid': _game_proc.pid, 'log': str(engine_log)}
    except Exception as e:
        return {'ok': False, 'error': str(e)}


def _tee_output(proc, log_fh, prefix=''):
    """Read proc stdout/stderr and write to both log_fh and sys.stdout."""
    try:
        for line in proc.stdout:
            text = line.decode(errors='replace')
            log_fh.write(text)
            log_fh.flush()
            sys.stdout.write(prefix + text)
            sys.stdout.flush()
    except Exception:
        pass
    finally:
        log_fh.close()


def _enqueue_build(args, run_args, auto_run, agent_id=''):
    """Add a build request to the queue. Returns the queue entry dict."""
    entry = {
        'id': str(uuid.uuid4())[:8],
        'agent_id': agent_id or '',
        'args': [str(a) for a in args],
        'run_args': [str(a) for a in run_args],
        'auto_run': auto_run,
        'queued_at': time.time(),
        'started_at': None,
        'finished_at': None,
        'status': 'queued',
        'returncode': None,
    }
    with _queue_lock:
        _queue.append(entry)
    _queue_event.set()
    return entry


def _queue_worker():
    """Background thread: process build queue one item at a time."""
    global _build_proc, _build_start, _build_timeout, _queue_current
    while True:
        _queue_event.wait()
        _queue_event.clear()
        while True:
            with _queue_lock:
                if not _queue:
                    break
                entry = _queue.popleft()

            # Kill any running build first
            if _build_proc is not None and _build_proc.poll() is None:
                print(f'[queue] Killing running build for new item {entry["id"]}', flush=True)
                _build_proc.terminate()
                try:
                    _build_proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    _build_proc.kill()
                _build_proc = None

            # Mark started
            with _queue_lock:
                entry['status'] = 'building'
                entry['started_at'] = time.time()
                _queue_current = entry

            is_clean = '--clean' in entry['args']
            timeout = 300 if is_clean else 60
            _build_timeout = timeout

            cmd = ['bash', str(BUILD_SCRIPT)] + entry['args']
            LOG_DIR.mkdir(parents=True, exist_ok=True)
            build_log = LOG_DIR / 'build.log'
            log_fh = open(build_log, 'a')
            separator = '═' * 60
            log_fh.write(f'\n{separator}\n'
                         f'# session  {time.strftime("%Y-%m-%dT%H:%M:%S")}'
                         f'  agent={entry["agent_id"] or "unknown"}'
                         f'  queue_id={entry["id"]}  args={entry["args"]}\n'
                         f'{separator}\n')
            log_fh.flush()
            print(f'[queue] Starting build {entry["id"]} (agent={entry["agent_id"]}): {" ".join(cmd)}', flush=True)

            _build_proc = subprocess.Popen(
                cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, cwd=str(ROOT))
            _build_start = time.time()
            threading.Thread(target=_tee_output, args=(_build_proc, log_fh, '[build] '),
                             daemon=True).start()

            # Wait for completion (with timeout)
            deadline = time.time() + timeout
            while time.time() < deadline:
                if _build_proc.poll() is not None:
                    break
                time.sleep(0.5)

            if _build_proc.poll() is None:
                print(f'[queue] Build {entry["id"]} TIMEOUT ({timeout}s), killing.', flush=True)
                _build_proc.kill()
                _build_proc.wait()

            rc = _build_proc.returncode
            with _queue_lock:
                entry['returncode'] = rc
                entry['finished_at'] = time.time()
                entry['status'] = 'done' if rc == 0 else 'failed'
                _queue_current = None
                _queue_history.append(entry)
                if len(_queue_history) > 20:
                    _queue_history.pop(0)

            if rc == 0 and entry['auto_run']:
                print(f'[queue] Build {entry["id"]} OK — launching game…', flush=True)
                _do_run(entry['run_args'], agent_id=entry['agent_id'])
            elif rc != 0:
                print(f'[queue] Build {entry["id"]} FAILED (rc={rc})', flush=True)


def _tail_log(path, n=100):
    path = Path(path)
    if not path.exists():
        return None
    lines = path.read_text(errors='replace').splitlines()
    return '\n'.join(lines[-n:])


# ── websocket helpers ─────────────────────────────────────────────────────────

def _ws_accept_key(key_b64):
    digest = hashlib.sha1(key_b64.encode() + WS_GUID).digest()
    return base64.b64encode(digest).decode()


def _ws_send(sock, data):
    """Send a text frame (server→client, no masking)."""
    if isinstance(data, str):
        data = data.encode()
    n = len(data)
    if n < 126:
        header = bytes([0x81, n])
    elif n < 65536:
        header = bytes([0x81, 126]) + struct.pack('>H', n)
    else:
        header = bytes([0x81, 127]) + struct.pack('>Q', n)
    sock.sendall(header + data)


def _ws_recv(sock):
    """Read one frame from client (always masked). Returns bytes or None on close/error."""
    def _read(n):
        buf = b''
        while len(buf) < n:
            chunk = sock.recv(n - len(buf))
            if not chunk:
                raise ConnectionError
            buf += chunk
        return buf

    try:
        h = _read(2)
        opcode = h[0] & 0x0F
        if opcode == 8:  # close
            return None
        masked = bool(h[1] & 0x80)
        length = h[1] & 0x7F
        if length == 126:
            length = struct.unpack('>H', _read(2))[0]
        elif length == 127:
            length = struct.unpack('>Q', _read(8))[0]
        mask = _read(4) if masked else b'\x00\x00\x00\x00'
        payload = _read(length)
        return bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
    except (ConnectionError, OSError):
        return None


# ── websocket session ─────────────────────────────────────────────────────────

def _run_ws_session(sock, log_names):
    """
    Runs in its own thread (one per WS client).
    - Streams new lines from requested logs as JSON.
    - Accepts {cmd} messages → RCON → sends response.
    - Sends {type:status} every 5s.
    """
    alive = [True]
    send_lock = threading.Lock()

    def send(obj):
        try:
            with send_lock:
                _ws_send(sock, json.dumps(obj))
        except OSError:
            alive[0] = False

    # ── log tail threads ──────────────────────────────────────────────────────
    def tail_log(name, path):
        path = Path(path)
        # catchup: send last 30 lines
        content = _tail_log(path, 30)
        if content:
            for line in content.splitlines():
                send({'type': 'log', 'file': name, 'line': line})
        # watch for new lines
        pos = path.stat().st_size if path.exists() else 0
        while alive[0]:
            time.sleep(0.15)
            if not path.exists():
                continue
            size = path.stat().st_size
            if size <= pos:
                if size < pos:  # log was truncated (new run)
                    pos = 0
                continue
            try:
                with open(path, errors='replace') as f:
                    f.seek(pos)
                    chunk = f.read()
                    pos = f.tell()
                for line in chunk.splitlines():
                    if line:
                        send({'type': 'log', 'file': name, 'line': line})
            except OSError:
                pass

    for name in log_names:
        if name in LOG_ALIASES:
            t = threading.Thread(target=tail_log, args=(name, LOG_ALIASES[name]), daemon=True)
            t.start()

    # ── status heartbeat ──────────────────────────────────────────────────────
    def status_loop():
        while alive[0]:
            time.sleep(5)
            pid = _game_pid()
            send({
                'type': 'status',
                'running': pid is not None,
                'pid': pid,
                'uptime_s': int(time.time() - _game_start) if pid and _game_start else None,
            })

    threading.Thread(target=status_loop, daemon=True).start()

    # ── recv loop (RCON commands) ─────────────────────────────────────────────
    send({'type': 'hello', 'logs': log_names, 'rcon_port': RCON_PORT})
    while alive[0]:
        data = _ws_recv(sock)
        if data is None:
            break
        try:
            msg = json.loads(data)
        except json.JSONDecodeError:
            continue
        cmd = msg.get('cmd', '').strip()
        if cmd:
            if not _game_running():
                send({'type': 'rcon', 'cmd': cmd, 'error': 'game not running'})
            else:
                resp = _send_rcon(cmd)
                send({'type': 'rcon', 'cmd': cmd,
                      'response': resp, 'ok': resp is not None})

    alive[0] = False
    try:
        sock.close()
    except OSError:
        pass


# ── HTTP request handler ──────────────────────────────────────────────────────

class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        try:
            print(f'[api] {self.command} {self.path}', flush=True)
        except BlockingIOError:
            pass

    def _send(self, code, body):
        data = json.dumps(body, indent=2).encode()
        self.send_response(code)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def _read_body(self):
        length = int(self.headers.get('Content-Length', 0))
        if length == 0:
            return {}
        try:
            return json.loads(self.rfile.read(length))
        except json.JSONDecodeError:
            return {}

    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path
        qs = parse_qs(parsed.query)

        # ── WebSocket upgrade ─────────────────────────────────────────────────
        if path == '/ws' and self.headers.get('Upgrade', '').lower() == 'websocket':
            key = self.headers.get('Sec-WebSocket-Key', '')
            self.send_response(101)
            self.send_header('Upgrade', 'websocket')
            self.send_header('Connection', 'Upgrade')
            self.send_header('Sec-WebSocket-Accept', _ws_accept_key(key))
            self.end_headers()
            self.wfile.flush()
            logs = qs.get('logs', ['engine,multimon'])[0].split(',')
            _run_ws_session(self.connection, logs)  # blocks until client disconnects
            return

        if path == '/lint':
            self._handle_lint(fix=False)
        elif path == '/build_status':
            self._handle_build_status(queue_id=qs.get('id', [None])[0])
        elif path == '/queue':
            self._handle_queue_list()
        elif path in ('/', '/status'):
            pid = _game_pid()
            self._send(200, {
                'ok': True,
                'running': pid is not None,
                'pid': pid,
                'uptime_s': int(time.time() - _game_start) if pid and _game_start else None,
            })
        elif path == '/logs':
            alias = qs.get('file', ['engine'])[0]
            n = int(qs.get('n', ['100'])[0])
            log_path = LOG_ALIASES.get(alias)
            if log_path is None:
                self._send(404, {'error': f'unknown: {alias}', 'available': list(LOG_ALIASES)})
                return
            content = _tail_log(log_path, n)
            if content is None:
                self._send(404, {'error': f'log not found: {log_path}'})
                return
            self._send(200, {'file': alias, 'lines': n, 'content': content})
        else:
            self._send(404, {'error': 'not found'})

    def do_DELETE(self):
        path = urlparse(self.path).path
        # DELETE /queue/<id>  — cancel a queued (not yet started) item
        if path.startswith('/queue/'):
            qid = path[len('/queue/'):]
            with _queue_lock:
                for i, entry in enumerate(_queue):
                    if entry['id'] == qid:
                        entry['status'] = 'cancelled'
                        entry['finished_at'] = time.time()
                        del _queue[i]
                        _queue_history.append(entry)
                        if len(_queue_history) > 20:
                            _queue_history.pop(0)
                        self._send(200, {'ok': True, 'cancelled': qid})
                        return
                # Check if it's the running build
                if _queue_current and _queue_current['id'] == qid:
                    self._send(409, {'error': 'build already started — use POST /stop to kill game'})
                    return
            self._send(404, {'error': f'queue_id {qid!r} not found or already done'})
        else:
            self._send(404, {'error': 'not found'})

    def do_POST(self):
        path = urlparse(self.path).path
        body = self._read_body()
        if path == '/lint':
            self._handle_lint(fix=body.get('fix', False))
        elif path == '/build':
            self._handle_build(body)
        elif path == '/run':
            self._handle_run(body)
        elif path == '/stop':
            self._handle_stop()
        elif path == '/console':
            self._handle_console(body)
        elif path == '/applescript':
            self._handle_applescript(body)
        else:
            self._send(404, {'error': 'not found'})

    def _handle_build(self, body):
        args = body.get('args', [])
        run_args = body.get('run_args', ['--level', '0', '--execute', 'q3ide attach all'])
        auto_run = body.get('auto_run', True)
        agent_id = body.get('agent_id', '')
        entry = _enqueue_build(args, run_args, auto_run, agent_id)
        with _queue_lock:
            position = list(_queue).index(entry) + 1 if entry in _queue else 0
        self._send(200, {
            'ok': True,
            'queue_id': entry['id'],
            'agent_id': entry['agent_id'],
            'position': position,
            'queued_at': entry['queued_at'],
            'queued_at_iso': time.strftime('%Y-%m-%dT%H:%M:%S', time.localtime(entry['queued_at'])),
            'log': 'build',
        })

    def _handle_build_status(self, queue_id=None):
        with _queue_lock:
            current = _queue_current
            pending = list(_queue)
            history = list(_queue_history)
        # Look up specific id
        if queue_id:
            entry = None
            if current and current['id'] == queue_id:
                entry = current
            else:
                for e in pending + history:
                    if e['id'] == queue_id:
                        entry = e
                        break
            if entry is None:
                self._send(404, {'error': f'queue_id {queue_id!r} not found'})
                return
            self._send(200, self._fmt_entry(entry))
            return
        self._send(200, {
            'current': self._fmt_entry(current) if current else None,
            'queue_depth': len(pending),
        })

    @staticmethod
    def _fmt_entry(e):
        now = time.time()
        out = {k: e[k] for k in ('id', 'agent_id', 'args', 'status', 'returncode')}
        out['queued_at_iso'] = time.strftime('%Y-%m-%dT%H:%M:%S', time.localtime(e['queued_at']))
        if e['started_at']:
            out['started_at_iso'] = time.strftime('%Y-%m-%dT%H:%M:%S', time.localtime(e['started_at']))
            out['elapsed_s'] = int((e['finished_at'] or now) - e['started_at'])
        if e['finished_at']:
            out['finished_at_iso'] = time.strftime('%Y-%m-%dT%H:%M:%S', time.localtime(e['finished_at']))
        return out

    def _handle_run(self, body):
        args = body.get('args', ['--level', '0', '--execute', 'q3ide attach all'])
        agent_id = body.get('agent_id', '')
        result = _do_run(args, agent_id=agent_id)
        if result.get('ok'):
            self._send(200, result)
        else:
            self._send(500, result)

    def _handle_stop(self):
        if not _game_running():
            self._send(200, {'ok': True, 'msg': 'not running'})
            return
        self._send(200, {'ok': _kill_game(), 'msg': 'stopped'})

    def _handle_applescript(self, body):
        script = body.get('script', '').strip()
        if not script:
            self._send(400, {'error': 'missing script'})
            return
        try:
            result = subprocess.run(
                ['osascript', '-e', script],
                capture_output=True, text=True, timeout=30
            )
            self._send(200, {
                'ok': result.returncode == 0,
                'stdout': result.stdout.strip(),
                'stderr': result.stderr.strip(),
                'returncode': result.returncode,
            })
        except FileNotFoundError:
            self._send(503, {'error': 'osascript not found (not macOS?)'})
        except subprocess.TimeoutExpired:
            self._send(504, {'error': 'AppleScript timeout'})
        except Exception as e:
            self._send(500, {'error': str(e)})

    def _handle_lint(self, fix=False):
        lint_script = ROOT / 'scripts' / 'lint.sh'
        try:
            if fix:
                import glob as _glob
                q3ide_dir = ROOT / 'quake3e' / 'code' / 'q3ide'
                files = _glob.glob(str(q3ide_dir / '*.c')) + _glob.glob(str(q3ide_dir / '*.h'))
                fix_result = subprocess.run(
                    ['clang-format', '-i'] + files,
                    capture_output=True, text=True, timeout=60, cwd=str(ROOT)
                )
                if fix_result.returncode != 0:
                    self._send(500, {'error': 'clang-format -i failed', 'stderr': fix_result.stderr})
                    return
            result = subprocess.run(
                ['sh', str(lint_script)],
                capture_output=True, text=True, timeout=300, cwd=str(ROOT)
            )
            output = result.stdout + result.stderr
            ok = result.returncode == 0
            try:
                print(output, flush=True)
            except BlockingIOError:
                pass
            self._send(200, {'ok': ok, 'fixed': fix, 'returncode': result.returncode, 'output': output})
        except FileNotFoundError:
            self._send(503, {'error': 'lint.sh not found'})
        except subprocess.TimeoutExpired:
            self._send(504, {'error': 'lint timeout'})
        except Exception as e:
            self._send(500, {'error': str(e)})

    def _handle_queue_list(self):
        with _queue_lock:
            current = _queue_current
            pending = list(_queue)
            history = list(_queue_history)
        self._send(200, {
            'current': self._fmt_entry(current) if current else None,
            'pending': [self._fmt_entry(e) for e in pending],
            'history': [self._fmt_entry(e) for e in reversed(history)],
        })

    def _handle_console(self, body):
        cmd = body.get('cmd', '').strip()
        if not cmd:
            self._send(400, {'error': 'missing cmd'})
            return
        if not _game_running():
            self._send(503, {'error': 'game not running'})
            return
        resp = _send_rcon(cmd)
        if resp is None:
            self._send(504, {'error': 'RCON timeout'})
            return
        self._send(200, {'ok': True, 'cmd': cmd, 'response': resp})


# ── main ──────────────────────────────────────────────────────────────────────

def main():
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    # Start build queue worker
    threading.Thread(target=_queue_worker, daemon=True, name='build-queue').start()
    server = ThreadingHTTPServer(('0.0.0.0', PORT), Handler)
    print(f'Q3IDE Remote API  http://0.0.0.0:{PORT}', flush=True)
    print(f'  WebSocket:  ws://{CLIENT_HOST}:{PORT}/ws?logs=engine,multimon,capture', flush=True)
    print(f'  From Docker: http://{CLIENT_HOST}:{PORT}/', flush=True)
    print(f'  RCON: {RCON_HOST}:{RCON_PORT}  password={RCON_PASSWORD!r}', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print('\nShutting down.', flush=True)
        server.server_close()
        sys.exit(0)


if __name__ == '__main__':
    main()
