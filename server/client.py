import socket 
from sys import byteorder, stdin
from select import poll, POLLIN

PORT = 3000
HOST = socket.gethostname()

def connect():
	s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

	s.connect((HOST, PORT))

	print(f"connected to {HOST} on port {PORT}")

	return s

def send_input(s: socket):
	message = input()
	data = len(message).to_bytes(4, byteorder) + message.encode()
	s.sendall(data)

def get_input(s: socket):
	data = s.recv(100).decode()
	print(data)

def command_loop():

	s = connect()

	p = poll()

	p.register(stdin.fileno(), POLLIN)
	p.register(s.fileno(), POLLIN)

	while True:
		events = p.poll(-1)

		for fd, _ in events:
			if fd == stdin.fileno():
				send_input(s)
			elif fd == s.fileno():
				get_input(s)

command_loop()