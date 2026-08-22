"""Robot Framework library driving the firmware in the QEMU RP2040 fork.

Spawns qemu-system-arm (-M raspi-pico) with the firmware ELF and exposes
the emulated UART0 (GP0/GP1) as a TCP socket for frame-level tests.
"""

import os
import socket
import subprocess
import time

FRAME_END = 0x04  # cmd::kFrameEnd


class QemuFirmware:
    ROBOT_LIBRARY_SCOPE = "SUITE"

    def __init__(self, qemu_path=None, elf_path=None, port=45823):
        repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        self.qemu_path = (qemu_path
                          or os.environ.get("PICO_YUBIKEY_QEMU")
                          or os.path.join(repo, "tools/qemu-rp2040/build/qemu-system-arm"))
        self.elf_path = elf_path or os.path.join(
            repo, "build-fw/src/pico_yubikey.elf")
        self.port = port
        self.proc = None
        self.sock = None
        self._buf = b""

    def start_firmware(self):
        if not os.path.exists(self.qemu_path):
            raise AssertionError(f"QEMU binary not found: {self.qemu_path}")
        if not os.path.exists(self.elf_path):
            raise AssertionError(f"Firmware ELF not found: {self.elf_path} "
                                 "(run ./clidev.sh build)")
        self.proc = subprocess.Popen(
            [self.qemu_path, "-M", "raspi-pico", "-kernel", self.elf_path,
             "-display", "none", "-monitor", "none",
             "-serial", f"tcp:127.0.0.1:{self.port},server=on,wait=on"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        deadline = time.time() + 10
        while time.time() < deadline:
            try:
                self.sock = socket.create_connection(("127.0.0.1", self.port), timeout=2)
                return
            except OSError:
                if self.proc.poll() is not None:
                    raise AssertionError(f"QEMU exited early with code {self.proc.returncode}")
                time.sleep(0.2)
        raise AssertionError("could not connect to QEMU serial port")

    def stop_firmware(self):
        if self.sock:
            self.sock.close()
            self.sock = None
        if self.proc and self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()
        self.proc = None

    def _read_more(self, timeout):
        self.sock.settimeout(timeout)
        try:
            data = self.sock.recv(4096)
            if data:
                self._buf += data
        except socket.timeout:
            pass

    def banner_should_appear(self, timeout=10):
        """Wait for the boot banner on UART; consumes it from the buffer."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            idx = self._buf.find(b"pico-yubikey")
            if idx >= 0:
                eol = self._buf.find(b"\n", idx)
                if eol < 0:
                    self._read_more(0.5)  # line incomplete, wait for it
                    continue
                self._buf = self._buf[eol + 1:]
                return
            self._read_more(0.5)
        tail = self._buf[-200:]
        raise AssertionError(f"boot banner not seen within {timeout}s; got: {tail!r}")

    def send_frame(self, payload):
        """Send one binary config frame (EOT-terminated)."""
        self.sock.sendall(bytes(payload) + bytes([FRAME_END]))

    def read_frame(self, timeout=5):
        """Read one EOT-terminated response; returns payload without EOT."""
        deadline = time.time() + timeout
        while FRAME_END not in self._buf:
            if time.time() > deadline:
                raise AssertionError(f"no response frame within {timeout}s; buffer: {self._buf!r}")
            self._read_more(0.5)
        frame, _, rest = self._buf.partition(bytes([FRAME_END]))
        self._buf = rest
        return list(frame)

    def get_status_should_be_ok(self):
        """GET_STATUS (0x01) must answer status byte 0x00."""
        self.send_frame([0x01])
        resp = self.read_frame()
        if not resp or resp[0] != 0x00:
            raise AssertionError(f"GET_STATUS failed: {resp!r}")
        return resp
