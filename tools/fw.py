#!/usr/bin/env python3
"""fw — FreeWili2 BSP task runner (cross-platform).

Commands:
  fw configure       configure build/ against the pinned Pico SDK (--clean wipes first)
  fw build [app]     configure+build an app for the RP2350B target (default hello_display)
  fw flash [app]     program the app over the cmsis-dap debug probe via OpenOCD
  fw rtt             stream SEGGER RTT diagnostics
  fw test            build+run host unit tests (CTest, no hardware)
  fw new-app <name>  scaffold apps/<name> from apps/template
Add --print to any build/flash/test command to print the command(s) instead of running.
"""
import argparse, os, pathlib, shutil, socket, stat, struct, subprocess, sys, time, zlib

REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
BUILD_DIR = REPO_ROOT / "build"
DEFAULT_APP = "hello_display"
OPENOCD_CFG = str(REPO_ROOT / "tools" / "openocd" / "freewili2.cfg")
RTT_PORT = 9090
# RP2350 SRAM is 0x20000000..0x20082000; scan the whole range for the RTT block.
RTT_SETUP = 'rtt setup 0x20000000 0x82000 "SEGGER RTT"'
AGENTIO_PORT = 9091          # RTT channel 1: agentio commands + pixels
AGENTIO_CHANNEL = 1
AGENTIO_MAGIC = b"FW2C"
AGENTIO_HEADER_LEN = 18
SURFACES = {"lcd": 0, "dvi": 1}
# Button indices must match uartkbd_btn_t in bsp/input/uartkbd_parse.h.
BUTTONS = ["grey", "yellow", "green", "blue", "red", "nav_center", "nav_up",
           "nav_down", "nav_left", "nav_right", "home", "ok", "cancel", "page"]

def packbits_decode(data, units):
    """Decode PackBits-16 (see bsp/agentio/agentio_proto.h) into a list of
    RGB565 values. `units` is the expected count; raises ValueError on
    truncated or malformed input."""
    out, i = [], 0
    while i < len(data) and len(out) < units:
        ctrl = data[i] - 256 if data[i] > 127 else data[i]
        i += 1
        if ctrl >= 0:
            count = ctrl + 1
            if i + count * 2 > len(data):
                raise ValueError("truncated literal run")
            for _ in range(count):
                out.append((data[i] << 8) | data[i + 1])
                i += 2
        elif ctrl != -128:
            count = 1 - ctrl
            if i + 2 > len(data):
                raise ValueError("truncated repeat run")
            v = (data[i] << 8) | data[i + 1]
            i += 2
            out.extend([v] * count)
        else:
            raise ValueError("reserved control byte")
    if len(out) < units:
        raise ValueError(f"short payload: {len(out)} of {units} units")
    return out[:units]

def png_write(path, w, h, pixels):
    """Write RGB565 `pixels` (row-major, w*h values) as an 8-bit RGB PNG.
    Stdlib only — no Pillow."""
    raw = bytearray()
    for y in range(h):
        raw.append(0)                       # filter type 0 (None) per scanline
        for x in range(w):
            v = pixels[y * w + x]
            r, g, b = (v >> 11) & 0x1F, (v >> 5) & 0x3F, v & 0x1F
            # scale 5/6-bit channels to 8-bit so full-scale maps to 255
            raw += bytes(((r * 255 + 15) // 31,
                          (g * 255 + 31) // 63,
                          (b * 255 + 15) // 31))

    def chunk(tag, payload):
        body = tag + payload
        return (struct.pack(">I", len(payload)) + body
                + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF))

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
        f.write(chunk(b"IEND", b""))

# Pinned toolchain versions under ~/.pico-sdk. The SDK path used to come from an
# ambient PICO_SDK_PATH (VS Code injects one) plus whatever landed in the CMake
# cache, so `rm -rf build` silently changed SDK versions. Pinning here makes the
# configure reproducible; each falls back to the newest installed version.
PICO_SDK_VERSION       = "2.3.0"      # 2.3.0 adds hardware_psram (official PSRAM support)
PICO_TOOLCHAIN_VERSION = "14_2_Rel1"  # the version every hardware-verified build used

def _pico_sdk_dir(kind, pinned):
    """~/.pico-sdk/<kind>/<pinned>, else the newest installed version, else None."""
    root = pathlib.Path.home() / ".pico-sdk" / kind
    if not root.is_dir():
        return None
    exact = root / pinned
    if exact.is_dir():
        return exact
    versions = sorted((d for d in root.iterdir() if d.is_dir()), reverse=True)
    return versions[0] if versions else None

def _ninja():
    """The Ninja bundled with the Pico SDK VS Code extension, if installed."""
    root = pathlib.Path.home() / ".pico-sdk" / "ninja"
    exe = "ninja.exe" if sys.platform == "win32" else "ninja"
    if root.is_dir():
        found = sorted(root.glob(f"*/{exe}"), reverse=True)
        if found:
            return found[0]
    return pathlib.Path(shutil.which("ninja")) if shutil.which("ninja") else None

def configure_command():
    """`cmake --preset target` with the SDK/toolchain pinned explicitly, so the
    configure does not depend on PICO_SDK_PATH being exported in the shell.
    NEVER add -DPICO_BOARD here — the top-level CMakeLists owns it (AGENTS.md
    invariant 1); overriding it on the command line reverts the board config."""
    cmd = ["cmake", "--preset", "target"]
    sdk = _pico_sdk_dir("sdk", PICO_SDK_VERSION)
    if sdk:
        cmd.append(f"-DPICO_SDK_PATH={sdk.as_posix()}")
    tc = _pico_sdk_dir("toolchain", PICO_TOOLCHAIN_VERSION)
    if tc:
        cmd.append(f"-DPICO_TOOLCHAIN_PATH={tc.as_posix()}")
    # The SDK's Findpicotool only finds a prebuilt picotool via picotool_DIR;
    # without it every fresh configure rebuilds picotool from source (~2 min).
    pt = _pico_sdk_dir("picotool", PICO_SDK_VERSION)
    if pt and (pt / "picotool" / "picotoolConfig.cmake").exists():
        cmd.append(f"-Dpicotool_DIR={(pt / 'picotool').as_posix()}")
    ninja = _ninja()
    if ninja:
        cmd.append(f"-DCMAKE_MAKE_PROGRAM={ninja.as_posix()}")
    return cmd

def _cached_sdk_path():
    """PICO_SDK_PATH recorded in build/CMakeCache.txt, or None if unconfigured."""
    cache = BUILD_DIR / "CMakeCache.txt"
    if not cache.exists():
        return None
    for line in cache.read_text(errors="replace").splitlines():
        if line.startswith("PICO_SDK_PATH:"):
            return line.split("=", 1)[1].strip()
    return None

def needs_configure():
    """True when build/ is missing or was configured against a different SDK.
    Changing PICO_SDK_PATH in place leaves stale SDK-derived cache entries, so a
    version change is handled by wiping build/ and configuring fresh."""
    cached = _cached_sdk_path()
    if cached is None:
        return True
    # A configure that failed part-way (missing submodule, bad path) leaves a
    # CMakeCache.txt behind but no generator file. Without this check the cache
    # looks valid, the configure is skipped, and the build dies on a missing
    # build.ninja instead of just re-configuring.
    if not (BUILD_DIR / "build.ninja").exists():
        return True
    sdk = _pico_sdk_dir("sdk", PICO_SDK_VERSION)
    return sdk is not None and pathlib.Path(cached) != sdk

def force_rmtree(path):
    """shutil.rmtree that survives read-only files. On Windows the git pack
    files under build/_deps/picotool-src are read-only, and a plain rmtree dies
    on them with PermissionError."""
    def on_error(func, p, _exc):
        os.chmod(p, stat.S_IWRITE)
        func(p)
    if sys.version_info >= (3, 12):
        shutil.rmtree(path, onexc=on_error)
    else:
        shutil.rmtree(path, onerror=lambda f, p, e: on_error(f, p, e))

def run_configure(clean=False):
    if clean and BUILD_DIR.exists():
        # flush: this print would otherwise buffer past the cmake output below
        print(f"removing {BUILD_DIR} (stale SDK configuration)", flush=True)
        force_rmtree(BUILD_DIR)
    subprocess.run(configure_command(), cwd=REPO_ROOT, check=True)

def build_command(app):
    return ["cmake", "--build", "--preset", "target", "--target", app]

def _openocd():
    """(exe, scripts_dir) for the Pico-SDK OpenOCD. Uses the ~/.pico-sdk install
    (newest version) when present — matching how subghz flashes — otherwise falls
    back to `openocd` on PATH with its built-in scripts (scripts_dir = None)."""
    root = pathlib.Path.home() / ".pico-sdk" / "openocd"
    if root.is_dir():
        exe_name = "openocd.exe" if sys.platform == "win32" else "openocd"
        for ver in sorted(root.iterdir(), reverse=True):
            exe, scripts = ver / exe_name, ver / "scripts"
            if exe.exists():
                return str(exe), (str(scripts) if scripts.is_dir() else None)
    return "openocd", None

def _openocd_base():
    exe, scripts = _openocd()
    cmd = [exe]
    if scripts:
        cmd += ["-s", scripts]
    return cmd + ["-f", OPENOCD_CFG]

def flash_command(app):
    elf = f"build/apps/{app}/{app}.elf"
    return _openocd_base() + ["-c", f"program {elf} verify reset exit"]

def rtt_command():
    """OpenOCD serving BOTH RTT channels: 0 (DIAG) and 1 (agentio). Only one
    process can own the debug probe, so a running `fw rtt` doubles as the
    session that `fw screenshot` / `fw press` reuse."""
    return _openocd_base() + [
        "-c", "init", "-c", RTT_SETUP, "-c", "rtt start",
        "-c", f"rtt server start {RTT_PORT} 0",
        "-c", f"rtt server start {AGENTIO_PORT} {AGENTIO_CHANNEL}"]

def _host_toolchain_args():
    """Extra `cmake` configure args that pin a host C compiler + Ninja for the
    standalone tests/ tree (no Pico SDK, no cross-compiler). Returns [] entries
    that are simply omitted when a tool can't be found, so CMake falls back to
    its own defaults (e.g. system cc/gcc, non-Ninja generator).
    """
    args = []
    if sys.platform == "win32":
        # Mirrors the subghz repo's proven host-test toolchain: MSYS2 MinGW
        # GCC + the Ninja bundled with the Pico SDK VS Code extension.
        gcc = pathlib.Path("C:/msys64/mingw64/bin/gcc.exe")
        if gcc.exists():
            args += [f"-DCMAKE_C_COMPILER={gcc}"]
        ninja_root = pathlib.Path.home() / ".pico-sdk" / "ninja"
        ninja = next(iter(sorted(ninja_root.glob("*/ninja.exe"), reverse=True)), None) \
            if ninja_root.is_dir() else None
        if ninja is not None:
            args += ["-G", "Ninja", f"-DCMAKE_MAKE_PROGRAM={ninja}"]
    else:
        # Non-Windows: trust the default host cc/gcc; use Ninja if it's on
        # PATH, otherwise let CMake pick its default generator (e.g. Make).
        if shutil.which("ninja"):
            args += ["-G", "Ninja"]
    return args

def test_command():
    tests_dir = REPO_ROOT / "tests"
    build_dir = REPO_ROOT / "build-tests"
    configure = ["cmake", "-S", str(tests_dir), "-B", str(build_dir)]
    configure += _host_toolchain_args()
    return [
        configure,
        ["cmake", "--build", str(build_dir)],
        ["ctest", "--test-dir", str(build_dir), "--output-on-failure"],
    ]

def new_app(name, repo_root=REPO_ROOT):
    src = pathlib.Path(repo_root) / "apps" / "template"
    dest = pathlib.Path(repo_root) / "apps" / name
    if dest.exists():
        raise FileExistsError(dest)
    shutil.copytree(src, dest)
    cml = dest / "CMakeLists.txt"
    cml.write_text(cml.read_text().replace("template", name))
    return dest

def _run(cmds, do_print):
    if isinstance(cmds[0], str):
        cmds = [cmds]
    for c in cmds:
        if do_print:
            print(" ".join(c))
        else:
            subprocess.run(c, cwd=REPO_ROOT, check=True)

def run_rtt(seconds=0):
    """Start OpenOCD's RTT server (attached, no flash) and stream channel 0 to
    stdout. seconds=0 runs until Ctrl+C; seconds>0 exits after that window
    (for scripted checks). Diagnostics on the FreeWili2 are RTT-only."""
    proc = subprocess.Popen(rtt_command(), cwd=REPO_ROOT,
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        time.sleep(2)  # let OpenOCD attach and locate the RTT control block
        if proc.poll() is not None:
            print("openocd exited early — is the debug probe connected?", file=sys.stderr)
            return 1
        try:
            sock = socket.create_connection(("127.0.0.1", RTT_PORT), timeout=5)
        except OSError as e:
            print(f"could not connect to RTT server on {RTT_PORT}: {e}", file=sys.stderr)
            return 1
        sock.settimeout(0.5)
        deadline = time.time() + seconds if seconds > 0 else None
        print(f"--- RTT connected (port {RTT_PORT}); Ctrl+C to stop ---", file=sys.stderr)
        while deadline is None or time.time() < deadline:
            try:
                data = sock.recv(4096)
                if not data:
                    break
                sys.stdout.write(data.decode("ascii", "replace"))
                sys.stdout.flush()
            except socket.timeout:
                pass
    except KeyboardInterrupt:
        pass
    finally:
        try:
            sock.close()
        except NameError:
            pass
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()
    return 0

def _port_open(port):
    try:
        socket.create_connection(("127.0.0.1", port), timeout=0.3).close()
        return True
    except OSError:
        return False

class _Agentio:
    """Connection to the agentio RTT channel. Reuses an OpenOCD already serving
    AGENTIO_PORT (e.g. a running `fw rtt`); otherwise spawns one and tears it
    down on exit."""
    def __init__(self):
        self.proc = None
        self.sock = None

    def __enter__(self):
        if not _port_open(AGENTIO_PORT):
            self.proc = subprocess.Popen(rtt_command(), cwd=REPO_ROOT,
                                         stdout=subprocess.DEVNULL,
                                         stderr=subprocess.DEVNULL)
            deadline = time.time() + 10
            while time.time() < deadline and not _port_open(AGENTIO_PORT):
                if self.proc.poll() is not None:
                    raise RuntimeError("openocd exited — is the probe connected?")
                time.sleep(0.2)
        self.sock = socket.create_connection(("127.0.0.1", AGENTIO_PORT), timeout=10)
        self.sock.settimeout(30)
        return self

    def __exit__(self, *exc):
        if self.sock:
            self.sock.close()
        if self.proc:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.proc.kill()
        return False

    def send(self, line):
        self.sock.sendall((line + "\n").encode("ascii"))

    def recv_exact(self, n):
        buf = b""
        while len(buf) < n:
            chunk = self.sock.recv(n - len(buf))
            if not chunk:
                raise RuntimeError("agentio connection closed mid-transfer")
            buf += chunk
        return buf

    def recv_line(self):
        buf = b""
        while not buf.endswith(b"\n"):
            chunk = self.sock.recv(1)
            if not chunk:
                raise RuntimeError("agentio connection closed")
            buf += chunk
        return buf.decode("ascii", "replace").strip()

def agentio_command(line):
    """Send one command expecting an OK/ERR reply. Returns the reply text."""
    with _Agentio() as a:
        a.send(line)
        return a.recv_line()

def agentio_capture(surface, crop, scale, out_path):
    """CAP + decode + PNG. Returns (w, h)."""
    x, y, w, h = crop if crop else (0, 0, 0, 0)
    with _Agentio() as a:
        a.send(f"CAP {SURFACES[surface]} {x} {y} {w} {h} {scale}")
        hdr = a.recv_exact(AGENTIO_HEADER_LEN)
        if hdr[:4] != AGENTIO_MAGIC:
            # a validation failure answers with a plain ERR line instead
            raise RuntimeError(hdr.decode("ascii", "replace").strip())
        ow, oh = struct.unpack(">HH", hdr[10:14])
        payload_len = struct.unpack(">I", hdr[14:18])[0]
        payload = a.recv_exact(payload_len)
    pixels = packbits_decode(payload, ow * oh)
    png_write(out_path, ow, oh, pixels)
    return ow, oh

def main(argv=None):
    p = argparse.ArgumentParser(prog="fw")
    sub = p.add_subparsers(dest="cmd", required=True)
    for name in ("build", "flash"):
        sp = sub.add_parser(name); sp.add_argument("app", nargs="?", default=DEFAULT_APP)
        sp.add_argument("--print", dest="show", action="store_true")
    sp = sub.add_parser("configure")
    sp.add_argument("--clean", action="store_true", help="wipe build/ before configuring")
    sp.add_argument("--print", dest="show", action="store_true")
    sp = sub.add_parser("rtt")
    sp.add_argument("--print", dest="show", action="store_true")
    sp.add_argument("-s", "--seconds", type=int, default=0,
                    help="capture for N seconds then exit (0 = until Ctrl+C)")
    sp = sub.add_parser("test"); sp.add_argument("--print", dest="show", action="store_true")
    sp = sub.add_parser("new-app"); sp.add_argument("name")

    sp = sub.add_parser("screenshot")
    sp.add_argument("-o", "--out", default="screenshot.png")
    sp.add_argument("--surface", choices=sorted(SURFACES), default="lcd")
    sp.add_argument("--crop", help="x,y,w,h")
    sp.add_argument("--scale", type=int, default=1)
    sp.add_argument("--print", dest="show", action="store_true")
    for name in ("press", "hold", "release"):
        sp = sub.add_parser(name); sp.add_argument("buttons")
    sp = sub.add_parser("touch")
    sp.add_argument("x", type=int); sp.add_argument("y", type=int)
    sp.add_argument("--down", action="store_true")
    sp.add_argument("--up", action="store_true")
    sp = sub.add_parser("type"); sp.add_argument("text")

    a = p.parse_args(argv)
    if a.cmd == "configure":
        if a.show:
            _run(configure_command(), True)
        else:
            run_configure(clean=a.clean)
    elif a.cmd == "build":
        # Self-healing: a missing build/ — or one left over from another SDK
        # version — is configured (wiping first on a version change) before the
        # build, so `rm -rf build` no longer strands the tree on whatever SDK
        # happens to be in the shell environment.
        if not a.show and needs_configure():
            run_configure(clean=BUILD_DIR.exists())
        _run(build_command(a.app), a.show)
    elif a.cmd == "flash": _run(flash_command(a.app), a.show)
    elif a.cmd == "rtt":
        if a.show:
            _run(rtt_command(), True)
        else:
            return run_rtt(a.seconds)
    elif a.cmd == "test":  _run(test_command(), a.show)
    elif a.cmd == "new-app":
        print("created", new_app(a.name))
    elif a.cmd == "screenshot":
        crop = tuple(int(v) for v in a.crop.split(",")) if a.crop else None
        if crop is not None and len(crop) != 4:
            print("--crop needs x,y,w,h", file=sys.stderr)
            return 1
        if a.show:
            print(f"CAP {SURFACES[a.surface]} "
                  f"{crop[0] if crop else 0} {crop[1] if crop else 0} "
                  f"{crop[2] if crop else 0} {crop[3] if crop else 0} {a.scale}")
            return 0
        w, h = agentio_capture(a.surface, crop, a.scale, a.out)
        print(f"wrote {a.out} ({w}x{h})")
    elif a.cmd in ("press", "hold", "release"):
        try:
            idx = [BUTTONS.index(b.strip()) for b in a.buttons.split(",")]
        except ValueError:
            print(f"unknown button; known: {', '.join(BUTTONS)}", file=sys.stderr)
            return 1
        if a.cmd == "press":
            for i in idx:
                print(agentio_command(f"TAP {i}"))
        else:
            mask = 0 if a.cmd == "release" else sum(1 << i for i in idx)
            print(agentio_command(f"BTN {mask:X}"))
    elif a.cmd == "touch":
        mode = 1 if a.down else (0 if a.up else 2)
        print(agentio_command(f"TCH {a.x} {a.y} {mode}"))
    elif a.cmd == "type":
        print(agentio_command(f"TYPE {a.text}"))
    return 0

if __name__ == "__main__":
    sys.exit(main())
