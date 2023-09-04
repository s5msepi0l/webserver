#include <iostream>
#include <chrono>
#include "threadpool.h"

void func(int num) {
    std::cout << "Hello number: " << num << std::endl;
}

int main() {
	
    nofetch_threadpool<void (*)(int)> pool(func, 10);
    for (int i = 0; i<100000; i++) {
    	pool.exec(i);
    }
	
}