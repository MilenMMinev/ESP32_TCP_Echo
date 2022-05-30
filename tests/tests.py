import unittest
import socket
import time
import secrets
import multiprocessing as mp

HOST = "192.168.4.1"  # The server's hostname or IP address
PORT = 11122  # The port used by the server

cnt_route = "/clients/cnt\r".encode()
cnt_messages_route = "/messages_cnts\r".encode()
size_messages_route = "/messages_sizes\r".encode()


def new_socket():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, PORT))
    return s


class EchoSocket(object):
    def __init__(self):
        self.sock = new_socket()
    def __enter__(self):
        return self.sock
    def __exit__(self, type, value, traceback):
        self.sock.close()


def get_clients_cnt():
    with EchoSocket() as s:
        s.sendall(cnt_route)
        resp = s.recv(256)
        return int(resp)


def get_messages_cnt():
    with EchoSocket() as s:
        s.sendall(cnt_messages_route)
        data = s.recv(256)
        print(data)
        return list(map(int, data.decode().split(',')))


def get_messages_sizes():
    with EchoSocket() as s:
        s.sendall(size_messages_route)
        data = s.recv(256)
        print(data)
        return list(map(int, data.decode().split(',')))


def send_msg(in_str):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((HOST, PORT))
        s.sendall(cnt_route)
        data = s.recv(256)
        print(len(data))
        time.sleep(1)
    print(f"Received {data!r}")


class TCPEchoTests(unittest.TestCase):
    def test_valid_echo(self):
        with EchoSocket() as s:
            payload = b"\x00" + secrets.token_bytes(125) + b"\x0D"
            s.sendall(payload)
            data = s.recv(256)
        self.assertEqual(data, payload)

    def test_invalid_echo(self):
        with EchoSocket() as s:
            payload = b"\x00" + secrets.token_bytes(125) + b"\x1D"
            s.sendall(payload)
            data = s.recv(256)
        self.assertEqual(len(data), 0)

    def test_active_clients_cnt(self):
        self.assertEqual(get_clients_cnt(), 1)
        with EchoSocket() as s:
            self.assertEqual(get_clients_cnt(), 2)
        self.assertEqual(get_clients_cnt(), 1)

    def test_messages_cnt(self):
        with EchoSocket() as s:
            self.assertEqual(max(get_messages_cnt()), 0)
            s.sendall(secrets.token_bytes(5) + b"\x0A")
            data = s.recv(256)
            self.assertEqual(max(get_messages_cnt()), 1)

    def test_message_sizes(self):
        with EchoSocket() as s:
            s.sendall(secrets.token_bytes(5) + b"\x0A")
            s.sendall(secrets.token_bytes(5) + b"\x0A")
            self.assertEqual(max(get_messages_sizes()), 12)


if __name__ == "__main__":
    unittest.main()
