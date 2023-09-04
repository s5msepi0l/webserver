from client import http_service

def main():
	app = http_service("127.0.0.1", 6014)
	app.run()

if __name__ == "__main__":
	main()