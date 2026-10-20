"""Automated Modbus RTU test for the STM32 temperature terminal."""

import argparse
import time

from pymodbus.client import ModbusSerialClient
from pymodbus.exceptions import ModbusIOException


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM6")
    parser.add_argument("--count", type=int, default=10)
    parser.add_argument("--interval", type=float, default=1.0)
    parser.add_argument("--timeout", type=float, default=2.0)
    args = parser.parse_args()

    client = ModbusSerialClient(
        port=args.port,
        baudrate=115200,
        bytesize=8,
        parity="N",
        stopbits=1,
        timeout=args.timeout,
        retries=0,
    )

    if not client.connect():
        print(f"OPEN FAILED: {args.port}")
        return 2

    success = 0
    try:
        for index in range(1, args.count + 1):
            try:
                result = client.read_input_registers(
                    address=0, count=4, device_id=1
                )
            except ModbusIOException as error:
                print(f"{index}: I/O TIMEOUT {error}")
                time.sleep(args.interval)
                continue
            if result.isError():
                print(f"{index}: MODBUS ERROR {result}")
            else:
                values = result.registers
                temperature = values[0] / 10.0
                humidity = values[1] / 10.0
                status = values[2]
                sequence = values[3]
                print(
                    f"{index}: T={temperature:.1f} C "
                    f"RH={humidity:.1f}% status=0x{status:04X} seq={sequence} "
                    f"raw={values}"
                )
                success += 1
            time.sleep(args.interval)
    finally:
        client.close()

    print(f"SUMMARY success={success} fail={args.count - success}")
    return 0 if success == args.count else 1


if __name__ == "__main__":
    raise SystemExit(main())
