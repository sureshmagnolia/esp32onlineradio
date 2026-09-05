import socket
import time

def test_url(path):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(3)
    try:
        s.connect(("192.168.1.50", 80))
        req = f"GET {path} HTTP/1.1\r\nHost: 192.168.1.50\r\nConnection: close\r\n\r\n"
        s.sendall(req.encode())
        res = b""
        while True:
            chunk = s.recv(1024)
            if not chunk: break
            res += chunk
        print(f"[{path}] Response ({len(res)} bytes):\n{res[:200]}")
    except Exception as e:
        print(f"[{path}] Error: {e}")
    finally:
        s.close()

test_url("/api/status")
test_url("/")
