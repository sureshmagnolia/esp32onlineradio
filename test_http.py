import socket
import time

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.settimeout(5)
print("Connecting to 192.168.1.50:80...")
t0 = time.time()
s.connect(("192.168.1.50", 80))
print(f"Connected in {time.time()-t0:.3f}s")

req = "GET / HTTP/1.1\r\nHost: 192.168.1.50\r\nUser-Agent: test\r\nAccept: */*\r\n\r\n"
s.sendall(req.encode())
print("Request sent. Reading response...")

total = b""
while True:
    try:
        chunk = s.recv(1024)
        if not chunk:
            print("Connection closed by server.")
            break
        print(f"Received {len(chunk)} bytes: {chunk[:100]}")
        total += chunk
    except socket.timeout:
        print("Socket read timed out after 5s.")
        break

print(f"Total bytes received: {len(total)}")
s.close()
