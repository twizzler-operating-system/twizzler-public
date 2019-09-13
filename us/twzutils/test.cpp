#include <err.h>
#include <iostream>

#include <ncurses.h>

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

	if(!initscr()) /* Start curses mode 		  */
		err(1, "initscr");
	printw("Hello World !!!"); /* Print Hello World		  */

	refresh(); /* Print it on to the real screen */

	getch(); /* Wait for user input */

	endwin(); /* End curses mode		  */

	foo();
	return 0;
}
