from pathlib import Path
import base64
import functools
import hashlib
import http.server
import json
import os
import shutil
import socket
import struct
import subprocess
import tempfile
import threading
import time
import urllib.parse
import urllib.request

PUBLIC = Path(r"C:/programmieren/zencrifice/zhaozhou/runs/CLAUDE-RUNS/RUN-20260829-2226-zixxtrixx-v10-lighting-spring-repair/work/Upheaval-v10/website/public")
EVID = Path(r"C:/programmieren/zencrifice/zhaozhou/runs/CLAUDE-RUNS/RUN-20260829-2226-zixxtrixx-v10-lighting-spring-repair/evidence")
EDGE = Path(r"C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe")
HTTP_PORT = 8765
CDP_PORT = 9239
URL = f"http://127.0.0.1:{HTTP_PORT}/"


class Quiet(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass


class WS:
    def __init__(self, url):
        u = urllib.parse.urlsplit(url)
        self.sock = socket.create_connection((u.hostname, u.port), 10)
        self.buf = b""
        self.seq = 0
        key = base64.b64encode(os.urandom(16)).decode()
        path = u.path + (("?" + u.query) if u.query else "")
        request = (
            f"GET {path} HTTP/1.1\r\n"
            f"Host: {u.hostname}:{u.port}\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            f"Origin: http://127.0.0.1:{u.port}\r\n\r\n"
        ).encode()
        self.sock.sendall(request)
        raw = b""
        while b"\r\n\r\n" not in raw:
            raw += self.sock.recv(4096)
        head, self.buf = raw.split(b"\r\n\r\n", 1)
        if b" 101 " not in head.split(b"\r\n", 1)[0]:
            raise RuntimeError(head.decode(errors="replace"))

    def exact(self, n):
        while len(self.buf) < n:
            block = self.sock.recv(max(4096, n - len(self.buf)))
            if not block:
                raise EOFError("websocket closed")
            self.buf += block
        out, self.buf = self.buf[:n], self.buf[n:]
        return out

    def frame(self, payload, opcode=1):
        if isinstance(payload, str):
            payload = payload.encode()
        mask = os.urandom(4)
        n = len(payload)
        header = bytes([0x80 | opcode])
        if n < 126:
            header += bytes([0x80 | n])
        elif n < 65536:
            header += bytes([0x80 | 126]) + struct.pack("!H", n)
        else:
            header += bytes([0x80 | 127]) + struct.pack("!Q", n)
        body = bytes(value ^ mask[i % 4] for i, value in enumerate(payload))
        self.sock.sendall(header + mask + body)

    def message(self):
        parts = []
        started = False
        while True:
            first, second = self.exact(2)
            final = bool(first & 0x80)
            opcode = first & 15
            n = second & 127
            if n == 126:
                n = struct.unpack("!H", self.exact(2))[0]
            elif n == 127:
                n = struct.unpack("!Q", self.exact(8))[0]
            mask = self.exact(4) if second & 0x80 else None
            data = self.exact(n)
            if mask:
                data = bytes(value ^ mask[i % 4] for i, value in enumerate(data))
            if opcode == 8:
                raise EOFError("websocket close frame")
            if opcode == 9:
                self.frame(data, 10)
                continue
            if opcode in (1, 2):
                parts = [data]
                started = True
            elif opcode == 0 and started:
                parts.append(data)
            else:
                continue
            if final:
                return b"".join(parts).decode()

    def send(self, method, params=None):
        self.seq += 1
        ident = self.seq
        self.frame(json.dumps({"id": ident, "method": method, "params": params or {}}))
        while True:
            message = json.loads(self.message())
            if message.get("id") != ident:
                continue
            if "error" in message:
                raise RuntimeError(f"{method}: {message['error']}")
            return message.get("result", {})

    def eval(self, expression):
        result = self.send(
            "Runtime.evaluate",
            {"expression": expression, "returnByValue": True, "awaitPromise": True},
        )
        if "exceptionDetails" in result:
            raise RuntimeError(result["exceptionDetails"])
        return result["result"].get("value")

    def close(self):
        try:
            self.frame(b"", 8)
        except Exception:
            pass
        try:
            self.sock.close()
        except Exception:
            pass


def port_closed(port):
    probe = socket.socket()
    probe.settimeout(0.3)
    try:
        probe.connect(("127.0.0.1", port))
        return False
    except OSError:
        return True
    finally:
        probe.close()


def main():
    handler = functools.partial(Quiet, directory=str(PUBLIC))
    server = http.server.ThreadingHTTPServer(("127.0.0.1", HTTP_PORT), handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    profile = Path(tempfile.mkdtemp(prefix="zv10-final-browser-"))
    edge = None
    cdp = None

    def wait_ready():
        for _ in range(100):
            try:
                if cdp.eval("document.readyState") == "complete":
                    return
            except Exception:
                pass
            time.sleep(0.1)
        raise RuntimeError("page did not load")

    def set_view(width, height):
        cdp.send(
            "Emulation.setDeviceMetricsOverride",
            {"width": width, "height": height, "deviceScaleFactor": 1, "mobile": False},
        )
        cdp.send("Page.navigate", {"url": URL})
        wait_ready()
        time.sleep(0.2)

    def shot(name):
        result = cdp.send(
            "Page.captureScreenshot",
            {"format": "png", "fromSurface": True, "captureBeyondViewport": False},
        )
        path = EVID / name
        path.write_bytes(base64.b64decode(result["data"]))
        return hashlib.sha256(path.read_bytes()).hexdigest()

    def current_facts(tab):
        return cdp.eval(
            f'''(()=>{{
              document.getElementById({json.dumps(tab)}).click();
              const vis=e=>getComputedStyle(e).display!=="none"&&getComputedStyle(e).visibility!=="hidden";
              const outer=[...document.querySelectorAll('.panels > .panel')].filter(vis);
              const vids=[...document.querySelectorAll('.panels > figure.panel > video')];
              return {{
                innerWidth,
                clientWidth:document.documentElement.clientWidth,
                scrollWidth:document.documentElement.scrollWidth,
                selected:document.querySelector('input[name="tab-zixxtrixx"]:checked')?.id,
                visibleOuter:outer.length,
                caption:outer[0]?.querySelector('figcaption')?.textContent.trim(),
                robots:document.querySelector('meta[name="robots"]')?.content,
                liveVideos:vids.length,
                liveBehavior:vids.every(v=>v.autoplay&&v.loop&&v.muted&&v.playsInline)
              }};
            }})()'''
        )

    def archive_facts(generation):
        return cdp.eval(
            f'''(()=>{{
              document.getElementById('zixxtrixx-archive').click();
              document.getElementById({json.dumps(generation)}).click();
              const vis=e=>getComputedStyle(e).display!=="none"&&getComputedStyle(e).visibility!=="hidden";
              const outer=[...document.querySelectorAll('.panels > .panel')].filter(vis);
              const gens=[...document.querySelectorAll('.archive-generation')].filter(vis);
              const vids=gens[0]?[...gens[0].querySelectorAll('video')]:[];
              return {{
                innerWidth,
                clientWidth:document.documentElement.clientWidth,
                scrollWidth:document.documentElement.scrollWidth,
                selected:document.querySelector('input[name="tab-zixxtrixx"]:checked')?.id,
                archiveSelected:document.querySelector('input[name="archive-zixxtrixx"]:checked')?.id,
                visibleOuter:outer.length,
                visibleGenerations:gens.length,
                heading:gens[0]?.querySelector('h3')?.textContent.trim(),
                generationSelectors:document.querySelectorAll('.archive-generations > input[type="radio"]').length,
                archiveVideos:vids.length,
                controls:vids.every(v=>v.controls),
                noAutoplay:vids.every(v=>!v.autoplay),
                preloadNone:vids.every(v=>v.preload==='none'),
                robots:document.querySelector('meta[name="robots"]')?.content
              }};
            }})()'''
        )

    try:
        edge = subprocess.Popen(
            [
                str(EDGE),
                "--headless=new",
                "--disable-gpu",
                "--no-first-run",
                "--disable-features=msEdgeFirstRunExperience",
                f"--remote-debugging-port={CDP_PORT}",
                "--remote-allow-origins=*",
                f"--user-data-dir={profile}",
                "--window-size=1410,1105",
                "about:blank",
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        target = None
        for _ in range(150):
            try:
                targets = json.load(
                    urllib.request.urlopen(
                        f"http://127.0.0.1:{CDP_PORT}/json/list", timeout=1
                    )
                )
                pages = [item for item in targets if item.get("type") == "page"]
                if pages:
                    target = pages[0]
                    break
            except Exception:
                pass
            time.sleep(0.1)
        if not target:
            raise RuntimeError("Edge CDP target unavailable")
        cdp = WS(target["webSocketDebuggerUrl"])
        cdp.send("Page.enable")
        cdp.send("Runtime.enable")

        set_view(1410, 1105)
        desktop_current = current_facts("zixxtrixx-6")
        time.sleep(0.1)
        desktop_current_sha = shot("task22-browser-desktop-current.png")
        desktop_archive = archive_facts("zixxtrixx-archive-generation-0")
        time.sleep(0.1)
        desktop_archive_sha = shot("task22-browser-desktop-archive.png")

        set_view(390, 1105)
        narrow_current = current_facts("zixxtrixx-6")
        time.sleep(0.1)
        narrow_current_sha = shot("task22-browser-narrow-current.png")
        narrow_archive = archive_facts("zixxtrixx-archive-generation-0")
        time.sleep(0.1)
        narrow_archive_sha = shot("task22-browser-narrow-archive.png")

        def current_ok(facts, width):
            return (
                facts["innerWidth"] == width
                and facts["scrollWidth"] == facts["clientWidth"]
                and facts["selected"] == "zixxtrixx-6"
                and facts["visibleOuter"] == 1
                and facts["caption"]
                == "Slow neck-led left/right taunt, 120 keys / 239 canonical frames"
                and facts["robots"] == "noindex, nofollow"
                and facts["liveVideos"] == 21
                and facts["liveBehavior"]
            )

        def archive_ok(facts, width):
            return (
                facts["innerWidth"] == width
                and facts["scrollWidth"] == facts["clientWidth"]
                and facts["selected"] == "zixxtrixx-archive"
                and facts["archiveSelected"] == "zixxtrixx-archive-generation-0"
                and facts["visibleOuter"] == 1
                and facts["visibleGenerations"] == 1
                and facts["heading"] == "v9 cel-main · 2026-08-29"
                and facts["generationSelectors"] == 7
                and facts["archiveVideos"] == 21
                and facts["controls"]
                and facts["noAutoplay"]
                and facts["preloadNone"]
                and facts["robots"] == "noindex, nofollow"
            )

        ok = (
            current_ok(desktop_current, 1410)
            and archive_ok(desktop_archive, 1410)
            and current_ok(narrow_current, 390)
            and archive_ok(narrow_archive, 390)
        )
        print("Zixxtrixx v10 local Edge browser M01")
        print("browser=Microsoft Edge headless Chromium via CDP")
        print(
            "desktop_current="
            + json.dumps(desktop_current, sort_keys=True)
            + f" screenshot_sha256={desktop_current_sha}"
        )
        print(
            "desktop_archive="
            + json.dumps(desktop_archive, sort_keys=True)
            + f" screenshot_sha256={desktop_archive_sha}"
        )
        print(
            "narrow_current="
            + json.dumps(narrow_current, sort_keys=True)
            + f" screenshot_sha256={narrow_current_sha}"
        )
        print(
            "narrow_archive="
            + json.dumps(narrow_archive, sort_keys=True)
            + f" screenshot_sha256={narrow_archive_sha}"
        )
        print(
            "M01 BROWSER: "
            + ("PASS" if ok else "FAIL")
            + " — exact 1410/390 CSS viewports, one requested panel/generation, no overflow, live/autoplay and archive/control behavior"
        )
        if not ok:
            raise SystemExit(1)
    finally:
        if cdp:
            try:
                cdp.send("Browser.close")
            except Exception:
                pass
            cdp.close()
        if edge:
            try:
                edge.wait(timeout=10)
            except subprocess.TimeoutExpired:
                edge.terminate()
                try:
                    edge.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    edge.kill()
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)
        shutil.rmtree(profile, ignore_errors=True)
        time.sleep(0.2)
        print(
            f"cleanup_http_{HTTP_PORT}_closed={port_closed(HTTP_PORT)} "
            f"cdp_{CDP_PORT}_closed={port_closed(CDP_PORT)} "
            f"edge_main_exited={edge is None or edge.poll() is not None}"
        )


if __name__ == "__main__":
    main()
