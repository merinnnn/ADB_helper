#!/usr/bin/env python3
import subprocess
import sys
import re

def get_adb_devices():
    """Return a list of connected device serials that are in 'device' state."""
    try:
        output = subprocess.check_output(["adb", "devices"], text=True)
    except subprocess.CalledProcessError as e:
        print("Error running 'adb devices':", e, file=sys.stderr)
        sys.exit(1)

    lines = output.strip().splitlines()
    # The first line is a header: "List of devices attached"
    devices = []
    for line in lines[1:]:
        line = line.strip()
        if not line:
            continue
        parts = line.split()
        # Expect lines like: "<serial>\tdevice"
        if len(parts) >= 2 and parts[1] == "device":
            devices.append(parts[0])
    return devices

def get_device_ip(device):
    """
    Retrieve the device's IP address using 'adb shell ip addr show wlan0'.
    This assumes the Wi-Fi interface is named 'wlan0'.
    """
    try:
        output = subprocess.check_output(
            ["adb", "-s", device, "shell", "ip", "addr", "show", "wlan0"],
            text=True
        )
    except subprocess.CalledProcessError as e:
        print("Error retrieving IP address:", e, file=sys.stderr)
        return None

    # Look for a line containing "inet <IP>/..."
    match = re.search(r'\s+inet\s+(\d+\.\d+\.\d+\.\d+)/', output)
    if match:
        return match.group(1)
    else:
        return None

def main():
    devices = get_adb_devices()
    if len(devices) != 1:
        print(f"Error: Expected exactly one device connected, but found {len(devices)}.", file=sys.stderr)
        sys.exit(1)

    device = devices[0]
    print(f"Found device: {device}")

    # Switch the device to TCP/IP mode on port 5555
    try:
        subprocess.check_call(["adb", "-s", device, "tcpip", "5555"])
    except subprocess.CalledProcessError as e:
        print("Error switching device to TCP/IP mode:", e, file=sys.stderr)
        sys.exit(1)

    # Retrieve the device's IP address
    device_ip = get_device_ip(device)
    if not device_ip:
        print("Error: Could not determine the device's IP address.", file=sys.stderr)
        sys.exit(1)
    print(f"Device IP address: {device_ip}")

    # Connect wirelessly to the device
    try:
        output = subprocess.check_output(["adb", "connect", f"{device_ip}:5555"], text=True)
        print(output.strip())
    except subprocess.CalledProcessError as e:
        print("Error connecting to device wirelessly:", e, file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
