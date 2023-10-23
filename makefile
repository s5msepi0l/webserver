# the compiler: gcc for C program, define as g++ for C++
  CC = g++
 
  # compiler flags:
  #  -g     - this flag adds debugging information to the executable file
  #  -Wall  - this flag is used to turn on most compiler warnings
  CFLAGS  = -std=c++20
 
  # The build target 
  TARGET = main
 
  all: $(TARGET)
 
  $(TARGET): $(TARGET).cpp
		$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).cpp
		./$(TARGET)
 
  clean:
		$(RM) $(TARGET)
