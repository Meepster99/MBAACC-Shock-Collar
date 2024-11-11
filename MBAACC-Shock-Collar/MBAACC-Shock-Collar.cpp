
#include <stdio.h>
#include <windows.h>
#include <iostream>
#include <string>
#include <iostream>
#include <fstream>

#include <wininet.h>
#pragma comment(lib, "wininet.lib")
#include "../Shared/Shared.h"

static_assert(sizeof(void*) == 4, "program must be compiled in 32 bit mode");

CollarManager collarManager;

void clearConsole() {
	COORD coord = { 0, 0 };
	DWORD dwWritten;
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	GetConsoleScreenBufferInfo(hConsole, &csbi);
	DWORD dwSize = csbi.dwSize.X * csbi.dwSize.Y;
	FillConsoleOutputCharacter(hConsole, ' ', dwSize, coord, &dwWritten);
	SetConsoleCursorPosition(hConsole, coord);
}

void renderConsole() {

	printf("\n-----\n\n");
	// clearConsole();
	
	printf("press 'R' to reload the config file. make sure to save.\n");
	printf("press 'S' to swap P1 and P2 shock settings.\n");

	collarManager.displayStatus();

	printf("Press '1' or '2' to send a max strength pulse to either of them.\n");

}

int main() {
	// stop pausing the window when someone clicks on it
	HANDLE hInput;
	DWORD prev_mode;
	hInput = GetStdHandle(STD_INPUT_HANDLE);
	GetConsoleMode(hInput, &prev_mode);
	SetConsoleMode(hInput, prev_mode & ENABLE_EXTENDED_FLAGS);

	KeyState rKey('R');
	KeyState oneKey('1');
	KeyState twoKey('2');

	collarManager.readSettings();
	renderConsole();

	while (true) {



		if (rKey.keyDown()) {
			collarManager.readSettings();
			renderConsole();
		}

		Sleep(100);

	};

		
	return 0;
}

