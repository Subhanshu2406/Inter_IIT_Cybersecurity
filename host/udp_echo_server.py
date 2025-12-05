#!/usr/bin/env python3
# """Simple UDP echo server for LiteX simulation debugging."""

# import argparse
# import logging
# import socket
# import sys
# import threading
# import time
# from typing import Tuple

# DEFAULT_HOST = "192.168.1.1"
# DEFAULT_PORT = 6000
# BUFFER_SIZE = 4096


# def hexdump(data: bytes) -> str:
#     return " ".join(f"{b:02x}" for b in data)


# def parse_args() -> argparse.Namespace:
#     parser = argparse.ArgumentParser(
#         description="UDP echo server used to test LiteX bare-metal clients",
#         formatter_class=argparse.ArgumentDefaultsHelpFormatter,
#     )
#     parser.add_argument("--bind", default=DEFAULT_HOST, help="IP address to bind (use TAP IP)")
#     parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="UDP port to bind")
#     parser.add_argument("--log-level", default="INFO", help="Logging level")
#     parser.add_argument(
#         "--hex",
#         action="store_true",
#         help="Dump payload bytes in hex for each packet",
#     )
#     return parser.parse_args()


# def serve(bind_ip: str, port: int, dump_hex: bool) -> None:
#     sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
#     sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
#     try:
#         sock.bind((bind_ip, port))
#     except OSError as exc:
#         logging.error("Failed to bind %s:%d (%s)", bind_ip, port, exc)
#         sys.exit(1)

#     logging.info("UDP echo server listening on %s:%d", bind_ip, port)
#     if dump_hex:
#         logging.info("Hex dump enabled for incoming payloads")

#     try:
#         while True:
#             data, addr = sock.recvfrom(BUFFER_SIZE)
#             src_ip, src_port = addr
#             logging.info("RX %d bytes from %s:%d", len(data), src_ip, src_port)
#             if dump_hex:
#                 logging.info("PAYLOAD: %s", hexdump(data))
#             else:
#                 logging.info("TEXT: %r", data)

#             sent = sock.sendto(data, addr)
#             logging.info("TX %d bytes back to %s:%d", sent, src_ip, src_port)
#     except KeyboardInterrupt:
#         logging.info("Stopping UDP server (Ctrl+C)")
#     finally:
#         sock.close()


# def main() -> None:
#     args = parse_args()
#     logging.basicConfig(
#         level=getattr(logging, args.log_level.upper(), logging.INFO),
#         format="[%(asctime)s] %(levelname)s: %(message)s",
#     )
#     serve(args.bind, args.port, args.hex)


# if __name__ == "__main__":
#     main()


#!/usr/bin/env python3
"""Simple UDP echo / DTLS-traffic logger for LiteX simulation debugging.

NOTE: This is *not* a DTLS server. It does not decrypt or speak DTLS.
It just receives UDP packets (including DTLS records), logs them, and
optionally echoes them back.
"""

import argparse
import logging
import socket
import sys
from typing import Tuple

# Defaults chosen to match your LiteX DTLS client:
#   - tap0 IP:   192.168.1.100
#   - server port: 5684 (DTLS_SERVER_PORT)
DEFAULT_HOST = "192.168.1.100"
DEFAULT_PORT = 6000
BUFFER_SIZE = 4096


def hexdump(data: bytes) -> str:
    return " ".join(f"{b:02x}" for b in data)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="UDP server used to test LiteX bare-metal clients "
                    "(DTLS traffic logger / echo)",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "--bind",
        default=DEFAULT_HOST,
        help="IP address to bind (use TAP IP, e.g. 192.168.1.100)",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=DEFAULT_PORT,
        help="UDP port to bind (5684 for DTLS demo, 6000 for simple echo)",
    )
    parser.add_argument(
        "--log-level",
        default="INFO",
        help="Logging level",
    )
    parser.add_argument(
        "--hex",
        action="store_true",
        help="Dump payload bytes in hex for each packet",
    )
    parser.add_argument(
        "--no-echo",
        action="store_true",
        help="Do not echo packets back (just log them)",
    )
    return parser.parse_args()


def serve(bind_ip: str, port: int, dump_hex: bool, echo: bool) -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    try:
        sock.bind((bind_ip, port))
    except OSError as exc:
        logging.error("Failed to bind %s:%d (%s)", bind_ip, port, exc)
        sys.exit(1)

    logging.info("UDP server listening on %s:%d", bind_ip, port)
    if dump_hex:
        logging.info("Hex dump enabled for incoming payloads")
    if not echo:
        logging.info("Echo disabled (logging-only mode)")

    try:
        while True:
            data, addr = sock.recvfrom(BUFFER_SIZE)
            src_ip, src_port = addr
            logging.info("RX %d bytes from %s:%d", len(data), src_ip, src_port)

            if dump_hex:
                logging.info("PAYLOAD: %s", hexdump(data))
            else:
                logging.info("TEXT (raw repr): %r", data)

            if echo:
                sent = sock.sendto(data, addr)
                logging.info("TX %d bytes back to %s:%d", sent, src_ip, src_port)
    except KeyboardInterrupt:
        logging.info("Stopping UDP server (Ctrl+C)")
    finally:
        sock.close()


def main() -> None:
    args = parse_args()
    logging.basicConfig(
        level=getattr(logging, args.log_level.upper(), logging.INFO),
        format="[%(asctime)s] %(levelname)s: %(message)s",
    )
    serve(args.bind, args.port, args.hex, not args.no_echo)


if __name__ == "__main__":
    main()
