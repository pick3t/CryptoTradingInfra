import socket
import struct
import time
import random
import argparse
import signal
import sys

running = True
total_packets_sent = 0

def signal_handler(sig, frame):
    global running
    print("\nSIGINT received. Stopping sender...")
    print(f'{total_packets_sent} packets sent.')
    running = False
    sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)

class MarketUpdate:
    # MarketUpdate struct:
    # uint64_t timestamp, double price, double size, uint8_t side
    # Equivalent to C++: <QddB (little-endian, 8-byte uint, double, double, 1-byte uint)
    STRUCT_FORMAT = '<QddB7x'
    SIZE = struct.calcsize(STRUCT_FORMAT)

    def __init__(self, timestamp = int(time.time_ns()), price = random.uniform(100.0, 200.0),
                 size = random.uniform(1.0, 10.0), side = random.randint(0, 1)):
        self.timestamp = timestamp
        self.price = price
        self.size = size
        self.side = side

    def pack(self):
        """Pack the data into binary form."""
        return struct.pack(self.STRUCT_FORMAT, self.timestamp, self.price, self.size, self.side)

    @classmethod
    def unpack(cls, data):
        """Unpack bytes into a MarketUpdate instance."""
        vals = struct.unpack(cls.STRUCT_FORMAT, data)
        return cls(*vals[:4])   # Only use the first 4 unpacked values

    def __repr__(self):
        return f"MarketUpdate(timestamp={self.timestamp}, price={self.price}, size={self.size}, side={self.side})"

def send_udp_packets(host, port, send_count, packets_per_second):
    global total_packets_sent

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sleep_per_packet = 1.0 / packets_per_second

    for i in range(send_count):
        sock.sendto(MarketUpdate().pack(), (host, port))
        total_packets_sent += 1

        if (i + 1) % 100_000 == 0:
            print(f"Sent {i + 1} packets")
        time.sleep(sleep_per_packet)

def parse_args():
    parser = argparse.ArgumentParser(description="Send MarketUpdate UDP packets.")
    parser.add_argument('--host', type=str, default='127.0.0.1', help='Receiver IP address')
    parser.add_argument('--port', type=int, default=49152, help='Receiver UDP port')
    parser.add_argument('--count', type=int, default=5_000_000, help='Number of packets to send')
    parser.add_argument('--interval', type=float, default=1_000_000, help='Number of packets to send per second')
    args = parser.parse_args()
    return args

if __name__ == '__main__':
    args = parse_args()
    print(f'Start sending packets to {args.host} on port {args.port}')
    send_udp_packets(args.host, args.port, args.count, args.interval)
