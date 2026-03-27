import socket
import struct
import time
import random

UDP_IP = "127.0.0.1"
UDP_PORT = 12345

print(f"UDP Target IP: {UDP_IP}")
print(f"UDP Target Port: {UDP_PORT}")

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# c  = char (1 byte)
# Q  = uint64_t (8 bytes)
# I  = uint32_t (4 bytes)
# x  = pad byte (zeros)

def create_add_order(order_id, price, qty, side):
    # 'A' (1) + id (8) + price (4) + qty (4) + side (1) = 18 bytes. 
    # Need 46 padding bytes to reach 64.
    return struct.pack('<cQIIc46x', b'A', order_id, price, qty, side)

def create_cancel_order(order_id):
    # 'X' (1) + id (8) = 9 bytes. 
    # Need 55 padding bytes to reach 64.
    return struct.pack('<cQ55x', b'X', order_id)

def create_modify_order(order_id, new_price, new_qty):
    # 'M' (1) + id (8) + price (4) + qty (4) = 17 bytes.
    # Need 47 padding bytes to reach 64.
    return struct.pack('<cQII47x', b'M', order_id, new_price, new_qty)


def run_simulation(num_messages=1000000):
    print(f"Generating {num_messages} messages...")
    
    packets = []
    for i in range(1, num_messages + 1):
        roll = random.random()
        
        if roll < 0.8:
            price = random.randint(100000, 101000)
            side = b'B' if random.random() > 0.5 else b'S'
            packets.append(create_add_order(i, price, 100, side))
        elif roll < 0.9:
            target_id = random.randint(1, max(1, i-1))
            packets.append(create_cancel_order(target_id))
        else:
            target_id = random.randint(1, max(1, i-1))
            new_price = random.randint(100000, 101000)
            packets.append(create_modify_order(target_id, new_price, 50))

    print("Sending UDP packets to C++ Engine...")
    input("Press Enter to start...")

    start_time = time.time()
    
    for packet in packets:
        sock.sendto(packet, (UDP_IP, UDP_PORT))

    end_time = time.time()
    elapsed = end_time - start_time
    
    print(f"\n--- Simulation Complete ---")
    print(f"Sent {num_messages} packets in {elapsed:.4f} seconds.")
    print(f"Throughput: {num_messages / elapsed:,.0f} messages/second")

if __name__ == "__main__":
    run_simulation()
