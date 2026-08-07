"""
pc_console_daemon.py

Bidirectional companion for the Embedded USB Console:
  - Sends CPU/RAM stats to the STM32 once per second (for the Computer screen)
  - Listens for "ACTION:<CMD>" from the STM32 (Dev Tools screen) and runs
    the matching developer action on this PC (build / open in VS Code /
    git status)

Setup:
    pip install pyserial psutil

Find your COM port:
    Windows: Device Manager -> Ports (COM & LPT) ->
             "STMicroelectronics STLink Virtual COM Port (COMx)"

BEFORE RUNNING, set the three paths below to match your machine:
  PROJECT_FOLDER - root of your STM32CubeIDE project (where .git and
                   your source folders live)
  BUILD_DIR      - the IDE's build output folder (contains the Makefile
                   the IDE itself invokes -- usually PROJECT_FOLDER/Debug)
  MAKE_EXE       - "make" only works here if your ARM toolchain is on
                   PATH, which usually is NOT true outside the IDE.
                   If BUILD fails with "'make' is not recognized", find
                   the real path via STM32CubeIDE: Window -> Preferences
                   -> C/C++ -> Build -> Environment (or MCU Settings),
                   look for the bundled toolchain's "make" executable,
                   and paste its full path here instead.

Run:
    python pc_console_daemon.py
"""

import glob
import os
import subprocess
import threading
import time

import psutil
import serial


COM_PORT = "COM5"
BAUD_RATE = 115200
SEND_INTERVAL_SEC = 1.0

PROJECT_FOLDER = r"C:\FirmwareProjects\EmbeddedUSBConsole"
BUILD_DIR = os.path.join(PROJECT_FOLDER, "Debug")
ELF_FILE = os.path.join(BUILD_DIR, "EmbeddedUSBConsole.elf")

MAKE_EXE = (
    r"C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins"
    r"\com.st.stm32cube.ide.mcu.externaltools.make.win32_2.2.200.202604021615"
    r"\tools\bin\make.exe"
)

ARM_TOOLCHAIN_BIN = (
    r"C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins"
    r"\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740"
    r"\tools\bin"
)

def find_programmer_cli():
    matches = glob.glob(
        r"C:\ST\**\STM32_Programmer_CLI.exe",
        recursive=True,
    )

    if matches:
        return matches[0]

    return None


PROGRAMMER_EXE = find_programmer_cli()

def run_build():
    print("Building project...")

    try:
        build_env = os.environ.copy()
        build_env["PATH"] = (
            ARM_TOOLCHAIN_BIN
            + os.pathsep
            + build_env.get("PATH", "")
        )

        result = subprocess.run(
            [MAKE_EXE, "-j8", "all"],
            cwd=BUILD_DIR,
            capture_output=True,
            text=True,
            env=build_env,
            shell=False,
        )

        if result.returncode == 0:
            print("BUILD SUCCESS")
            if result.stdout.strip():
                print(result.stdout[-1000:])
        else:
            print("BUILD FAILED")

            if result.stdout.strip():
                print(result.stdout[-2000:])

            if result.stderr.strip():
                print(result.stderr[-2000:])

    except Exception as exc:
        print(f"Build failed to run: {exc}")

def run_flash():
    print("Flashing firmware...")

    if PROGRAMMER_EXE is None:
        print("FLASH FAILED: STM32_Programmer_CLI.exe was not found.")
        return

    if not os.path.isfile(ELF_FILE):
        print("FLASH FAILED: EmbeddedUSBConsole.elf does not exist.")
        print("Run BUILD first.")
        return

    try:
        result = subprocess.run(
            [
                PROGRAMMER_EXE,
                "--connect",
                "port=SWD",
                "mode=UR",
                "reset=HWrst",
                "--download",
                ELF_FILE,
                "--verify",
		"-rst",
      
            ],
            capture_output=True,
            text=True,
            shell=False,
        )

        if result.returncode == 0:
            print("FLASH SUCCESS")
        else:
            print("FLASH FAILED")

        if result.stdout.strip():
            print(result.stdout[-4000:])

        if result.stderr.strip():
            print(result.stderr[-4000:])

    except Exception as exc:
        print(f"Flash failed to run: {exc}")


def run_git_status():
    print("Checking git status...")

    try:
        result = subprocess.run(
            ["git", "status", "--short"],
            cwd=PROJECT_FOLDER,
            capture_output=True,
            text=True,
            shell=False,
        )

        if result.returncode != 0:
            print("git status failed")
            if result.stderr.strip():
                print(result.stderr.strip())
            return

        output = result.stdout.strip()
        print(output if output else "Working tree clean")

    except Exception as exc:
        print(f"git status failed: {exc}")


def run_open_code():
    print("Opening project in VS Code...")

    try:
        subprocess.Popen(
            ["code", PROJECT_FOLDER],
            shell=True,
        )
    except Exception as exc:
        print(f"Failed to open VS Code: {exc}")


ACTION_HANDLERS = {
    "BUILD": run_build,
    "FLASH": run_flash,
    "OPEN_CODE": run_open_code,
    "GIT_STATUS": run_git_status,
}


def reader_thread(ser):
    """Read commands from the STM32 and run their matching PC actions."""

    while True:
        try:
            line = ser.readline().decode(
                "ascii",
                errors="ignore",
            ).strip()
        except serial.SerialException:
            break

        if not line:
            continue

        print(f"Received <- {line}")

        if line.startswith("ACTION:"):
            action_key = line[len("ACTION:"):]
            handler = ACTION_HANDLERS.get(action_key)

            if handler:
                handler()
            else:
                print(f"Unknown action: {action_key}")


def main():
    print(f"Connecting to {COM_PORT} at {BAUD_RATE} baud...")

    try:
        ser = serial.Serial(
            COM_PORT,
            BAUD_RATE,
            timeout=1,
        )
    except serial.SerialException as exc:
        print(f"Could not open {COM_PORT}: {exc}")
        return

    time.sleep(2)
    print(
        "Connected. Sending stats + listening for commands. "
        "Ctrl+C to stop."
    )

    thread = threading.Thread(
        target=reader_thread,
        args=(ser,),
        daemon=True,
    )
    thread.start()

    try:
        while True:
            cpu_percent = int(
                psutil.cpu_percent(interval=None)
            )
            ram_percent = int(
                psutil.virtual_memory().percent
            )

            ser.write(
                f"CPU:{cpu_percent}\n".encode("ascii")
            )
            ser.write(
                f"RAM:{ram_percent}\n".encode("ascii")
            )

            time.sleep(SEND_INTERVAL_SEC)

    except KeyboardInterrupt:
        print("\nStopped.")

    finally:
        ser.close()


if __name__ == "__main__":
    main()