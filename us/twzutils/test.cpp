#include <iostream>

void foo()
{
	std::cout << "Trying an exception!\n";
	throw "Exception Message";
	std::cout << "Should not get here\n";
}

int main()
{
	std::cout << "Hello from C++!\n";
	try {
		foo();
	} catch(const char *msg) {
		std::cerr << "Caught: " << msg << "\n";
	}
	foo();
	return 0;
}
