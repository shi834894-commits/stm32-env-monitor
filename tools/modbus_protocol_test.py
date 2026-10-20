"""Exercise valid and invalid Modbus RTU requests against the target."""

import argparse
import time

import serial


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for value in data:
        crc ^= value
        for _ in range(8):
            crc = (crc >> 1) ^ (0xA001 if crc & 1 else 0)
    return crc


def frame(payload: bytes) -> bytes:
    crc = crc16(payload)
    return payload + bytes((crc & 0xFF, crc >> 8))


def receive(port: serial.Serial, minimum_wait: float = 0.25) -> bytes:
    deadline = time.monotonic() + minimum_wait
    result = bytearray()
    while time.monotonic() < deadline or port.in_waiting:
        waiting = port.in_waiting
        if waiting:
            result.extend(port.read(waiting))
            deadline = time.monotonic() + 0.08
        else:
            time.sleep(0.005)
    return bytes(result)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM6")
    args = parser.parse_args()

    tests = [
        ("valid read", frame(bytes((1, 4, 0, 0, 0, 4))), "response"),
        ("bad CRC", frame(bytes((1, 4, 0, 0, 0, 4)))[:-1] + b"\x00", "silent"),
        ("wrong slave", frame(bytes((2, 4, 0, 0, 0, 4))), "silent"),
        ("unsupported FC", frame(bytes((1, 3, 0, 0, 0, 4))), "exception"),
    ]
    failures = 0
    with serial.Serial(args.port, 115200, bytesize=8, parity="N", stopbits=1,
                       timeout=0.03, write_timeout=1) as port:
        for name, request, expected in tests:
            port.reset_input_buffer()
            port.write(request)
            port.flush()
            response = receive(port)
            valid = False
            if expected == "response":
                valid = (
                    len(response) == 13
                    and response[:3] == bytes((1, 4, 8))
                    and int.from_bytes(response[-2:], "little") == crc16(response[:-2])
                )
                if valid:
                    values = [int.from_bytes(response[i:i + 2], "big")
                              for i in range(3, 11, 2)]
                    valid = values[2] & 1 == 1
                    print(f"{name}: {response.hex(' ').upper()} regs={values}")
            elif expected == "exception":
                valid = (
                    len(response) == 5
                    and response[:3] == bytes((1, 0x83, 1))
                    and int.from_bytes(response[-2:], "little") == crc16(response[:-2])
                )
                print(f"{name}: {response.hex(' ').upper()}")
            else:
                valid = len(response) == 0
                print(f"{name}: {'no response (expected)' if valid else response.hex(' ').upper()}")
            print(f"  {'PASS' if valid else 'FAIL'}")
            failures += not valid
            time.sleep(0.3)

    print(f"SUMMARY pass={len(tests) - failures} fail={failures}")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
