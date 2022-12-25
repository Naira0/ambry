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

def send_input(s: socket.socket):
	message = input()
	data = len(message).to_bytes(4, byteorder) + message.encode()
	s.sendall(data)

def get_input(s: socket.socket):
	data = s.recv(5)

	status_code = data[0]
	res_len = int.from_bytes(data[1:5], byteorder)

	data = s.recv(res_len)

	print(f"{status_code}; {data.decode()}")

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