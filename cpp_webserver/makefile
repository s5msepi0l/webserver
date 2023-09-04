options = "-O2"

.PHONY: run clean

run: main
	./main

main: main.cpp
	g++ -o main main.cpp $(options)

clean:
	rm -f main