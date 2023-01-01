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

def prepare_message(message: str):
	return len(message).to_bytes(4, byteorder) + message.encode()

def get_message(s: socket.socket):
	data = s.recv(5)

	status_code = data[0]

	res_len = int.from_bytes(data[1:5], byteorder)

	return (status_code, s.recv(res_len))

def get_db_name(s: socket.socket):
	s.sendall(prepare_message("working_db"))

	(status_code, data) = get_message(s)

	if (status_code != 0):
		return "No db open"

	return data.decode()

def send_input(s: socket.socket):
	name = get_db_name(s)
	message = input(f"{name} => ")
	s.sendall(prepare_message(message))

def get_input(s: socket.socket):
	(status_code, data) = get_message(s)
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
