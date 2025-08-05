import socket
import struct
import time
import random
import argparse
import signal
import sys
import numpy as np

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

    def __init__(self, timestamp, price, size, side):
        self.timestamp = timestamp
        self.price = price
        self.size = size
        self.side = side

    def pack(self):
        """Pack the data into binary form."""
        return struct.pack(self.STRUCT_FORMAT, self.timestamp, self.price, self.size, self.side)

    @classmethod
    def generate_batch(cls, count):
        """Generate a batch of MarketUpdates using NumPy for vectorized random generation"""
        timestamps = np.full(count, time.perf_counter_ns()) + np.arange(count)
        prices = np.random.uniform(100.0, 200.0, size=count)
        sizes = np.random.randint(1, 100, size=count)
        sides = np.random.randint(0, 2, size=count, dtype=np.uint8)

        return [cls(t, p, s, side) for t, p, s, side in zip(timestamps, prices, sizes, sides)]

    @classmethod
    def unpack(cls, data):
        """Unpack bytes into a MarketUpdate instance."""
        vals = struct.unpack(cls.STRUCT_FORMAT, data)
        return cls(*vals[:4])   # Only use the first 4 unpacked values

    def __repr__(self):
        return f"MarketUpdate(timestamp={self.timestamp}, price={self.price}, size={self.size}, side={'BID' if self.side else 'ASK'})"

def send_udp_packets(host, port, send_count, packets_per_second):
    global total_packets_sent

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    packets_sent = 0
    start_time = time.perf_counter()
    while packets_sent < send_count and running:
        packets_to_send = min(packets_per_second, send_count - packets_sent)

        batch_updates = MarketUpdate.generate_batch(packets_to_send)
        for update in batch_updates:
            per_packet_start_time = time.perf_counter()
            sock.sendto(update.pack(), (host, port))
            total_packets_sent += 1
            if send_count <= 20:
                print(update)

            per_packet_time_elapsed = time.perf_counter() - per_packet_start_time
            per_packet_send_interval = 1.0 / packets_per_second
            if per_packet_time_elapsed < per_packet_send_interval:
                time.sleep(per_packet_send_interval - per_packet_time_elapsed)

        packets_sent += packets_to_send

        if packets_sent % 100_000 == 0 or packets_sent == send_count:
            print(f"Sent {packets_sent} packets in {time.perf_counter() - start_time}")


def send_udp_packets_batch(host, port, send_count, packets_per_second):
    global total_packets_sent

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    packets_sent = 0
    start_time = time.perf_counter()
    while packets_sent < send_count and running:
        batch_start_time = time.perf_counter()
        packets_to_send = min(packets_per_second, send_count - packets_sent)

        batch_updates = MarketUpdate.generate_batch(packets_to_send)
        for update in batch_updates:
            sock.sendto(update.pack(), (host, port))
            total_packets_sent += 1
            if send_count <= 20:
                print(update)

        packets_sent += packets_to_send
        batch_time_elapsed = time.perf_counter() - batch_start_time

        if batch_time_elapsed < 1.0:
            time.sleep(1.0 - batch_time_elapsed)

        if packets_sent % 100_000 == 0 or packets_sent == send_count:
            print(f"Sent {packets_sent} packets in {time.perf_counter() - start_time}")

def parse_args():
    parser = argparse.ArgumentParser(description="Send MarketUpdate UDP packets.")
    parser.add_argument('--host', type=str, default='127.0.0.1', help='Receiver IP address')
    parser.add_argument('--port', type=int, default=49152, help='Receiver UDP port')
    parser.add_argument('--count', type=int, default=5_000_000, help='Number of packets to send')
    parser.add_argument('--pps', type=int, default=1_000_000, help='Number of packets to send per second')
    parser.add_argument('--batch', type=bool, default=False, help='If turned on, number of packets specified by pps'
                        'will be sent immediately instead of being sent 1 by 1 based on calculated'
                        'sending rate(1.0s / pps)')
    args = parser.parse_args()
    return args

if __name__ == '__main__':
    args = parse_args()
    print(f'Start sending packets to {args.host} on port {args.port}')
    if args.batch:
        send_udp_packets_batch(args.host, args.port, args.count, args.pps)
    else:
        send_udp_packets(args.host, args.port, args.count, args.pps)
