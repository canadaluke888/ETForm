import errno
import fcntl
import json
import os
import pty
import re
import select
import struct
import subprocess
import sys
import termios
import time
import unittest


FORM_SCRIPT = """
import json
from etform import Form, ITable
table = ITable(key="items", show_lines=True)
table.add_column("Name")
table.add_column("Value")
form = Form("Swap test")
form.add_table_input(table)
print("RESULT=" + json.dumps(form.run()), flush=True)
"""

UP = b"\x1b[A"
RIGHT = b"\x1b[C"
ENTER = b"\r"
CTRL_M = b"\x0d"
CTRL_R = b"\x12"
CTRL_D = b"\x04"
CTRL_E = b"\x05"
CTRL_S = b"\x13"


def run_form(keys):
    master, slave = pty.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 80, 0, 0))
    env = os.environ.copy()
    env["TERM"] = "xterm-256color"
    process = subprocess.Popen(
        [sys.executable, "-u", "-c", FORM_SCRIPT],
        stdin=slave,
        stdout=slave,
        stderr=slave,
        env=env,
    )
    os.close(slave)
    output = bytearray()

    def drain(timeout):
        while select.select([master], [], [], timeout)[0]:
            try:
                data = os.read(master, 65536)
            except OSError as exc:
                if exc.errno != errno.EIO:
                    raise
                return
            if not data:
                return
            output.extend(data)
            timeout = 0

    try:
        deadline = time.monotonic() + 10
        while b"Swap test" not in output and time.monotonic() < deadline:
            drain(0.1)
        if b"Swap test" not in output:
            raise AssertionError(f"form did not start: {output[-500:]!r}")
        for key in keys:
            os.write(master, key)
            time.sleep(0.1)
            drain(0)
        deadline = time.monotonic() + 10
        while process.poll() is None and time.monotonic() < deadline:
            drain(0.1)
        if process.poll() is None:
            raise AssertionError("form did not exit")
        drain(0)
        if process.returncode != 0:
            raise AssertionError(f"form failed: {output[-1000:]!r}")
        match = re.search(rb"RESULT=([^\r\n]+)", output)
        if not match:
            raise AssertionError(f"form did not return a result: {output[-1000:]!r}")
        return json.loads(match.group(1))
    finally:
        if process.poll() is None:
            process.kill()
        process.wait()
        os.close(master)


class TableSwapTest(unittest.TestCase):
    def test_swap_rows_and_submit(self):
        result = run_form([
            ENTER, b"first", RIGHT, b"one", CTRL_R, b"second", RIGHT, b"two",
            CTRL_R, b"third", RIGHT, b"three",
            CTRL_M, UP, UP, ENTER, CTRL_E, CTRL_S,
        ])
        self.assertEqual(result, {
            "items": {
                "columns": ["Name", "Value"],
                "data": [["third", "three"], ["second", "two"], ["first", "one"]],
            }
        })

    def test_cancel_and_swap_with_same_row(self):
        result = run_form([
            ENTER, b"first", RIGHT, b"one", CTRL_R, b"second", RIGHT, b"two",
            CTRL_M, UP, b"ignored", CTRL_R, CTRL_D, b"\x1b",
            b"!", CTRL_M, ENTER, CTRL_E, CTRL_S,
        ])
        self.assertEqual(result["items"]["data"], [["first", "one"], ["second", "two!"]])

    def test_swap_key_on_empty_table(self):
        result = run_form([ENTER, CTRL_D, CTRL_M, CTRL_R, b"value", CTRL_E, CTRL_S])
        self.assertEqual(result["items"]["data"], [["value", ""]])


if __name__ == "__main__":
    unittest.main()
