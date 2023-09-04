# direct templates routes and src are automatically responded by the webserver
# client networking is done single threaded for simplicity but does significantly slow down performance
# http service mainly routes backend services that include cookies, 
# request headers and miscellaneous stuff for scaling reasons


import socket
import json

def parse_request(request_string):
    request_lines = request_string.strip().split('\n')
    
    # Extract request method and path
    method, path, _ = request_lines[0].split()
    
    # Extract connection type
    connection = None
    for line in request_lines:
        if line.startswith('Connection:'):
            connection = line.split(': ', 1)[1]
            break
    
    # Extract cookies
    cookies = {}
    for line in request_lines:
        if line.startswith('Cookie:'):
            cookies_str = line.split(': ', 1)[1]
            cookies_list = cookies_str.split('; ')
            for cookie in cookies_list:
                key, value = cookie.split('=')
                cookies[key] = value
    
    # Create and return the dictionary
    request_info = {
        'path': path,
        'method': method,
        'connection': connection,
        'cookies': cookies
    }
    
    return request_info

def fetch(path):
    try:
        with open(path, 'r') as fd:
            buffer = fd.read()
            return buffer
    except FileNotFoundError:
        print(f"File '{path}' not found.")
        return None
    except Exception as e:
        print(f"An error occurred: {e}")
        return None

class http_service:
    def __init__(self, host, port):
        self.socket_fd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket_fd.connect((host, port))
        self.routes = {}

    def __del__(self):
        self.socket_fd.close()

    def route(self, path):
        def decorator(func):
            self.routes[path] = func
            return func
        return decorator

    def process(self, path):
        if path in self.routes:
            return self.routes[path]

    def run(self):
        while (True):
            print("listening for packets")
            data = self.socket_fd.recv(8192).decode("utf-8")
            print(data)
            
            response = fetch("/home/s5msep1ol/Desktop/webserver/py_lib/templates/index.html")

            self.socket_fd.send(response.encode())
            print("packet sent")