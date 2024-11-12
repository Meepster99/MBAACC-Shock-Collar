
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

/*

a note.
while i could put this var into a shared memory section and launch calls from the game, i would rather
not risk the making of that request causing lag. so ill do some shared memory bs

*/

bool meltyInjected = false;

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

	clearConsole();
	
	printf(WHITE "press 'R' to reload the config file(shockSettings.txt). make sure to save.\n" RESET);
	printf("\n");

	collarManager.displayStatus();

	printf("Press '1' or '2' to send a min strength pulse to either of them.\n");
	printf("Hold shift to send a max strength pulse instead\n");

	printf("\n");

	printf("Melty Status: %s\n\n", meltyInjected ? "Injected" : "Not Found");

}

int main() {
	
	// stop pausing the window when someone clicks on it
	HANDLE hInput;
	DWORD prev_mode;
	hInput = GetStdHandle(STD_INPUT_HANDLE);
	GetConsoleMode(hInput, &prev_mode);
	SetConsoleMode(hInput, prev_mode & ENABLE_EXTENDED_FLAGS);

	printf(RESET);

	KeyState rKey('R');
	KeyState oneKey('1');
	KeyState twoKey('2');
	KeyState shiftKey(VK_SHIFT);

	collarManager.readSettings();
	renderConsole();

	while (true) {

		if (rKey.keyDown()) {
			collarManager.readSettings();
			renderConsole();
			continue;
		}

		bool oneKeyDown = oneKey.keyDown();
		bool twoKeyDown = twoKey.keyDown();

		if (oneKeyDown || twoKeyDown) {
			
			int collarIndex = oneKeyDown ? 0 : 1;

			renderConsole();

			bool isMaxShock = shiftKey.keyHeld();

			int shockValue = isMaxShock ? collarManager.collars[collarIndex].maxShock : collarManager.collars[collarIndex].minShock;
			ShockType shockType = collarManager.collars[collarIndex].shockType;

			bool res = collarManager.sendShock(collarIndex, shockValue, 300);

			if (res) {
				printf(GREEN "%s collar %d with a strength of %d\n" RESET, getShockTypeVerb(shockType), collarIndex + 1, shockValue);
			} else {
				printf(RED "failed to send instruction to collar %d\n" RESET, collarIndex + 1);
			}
	
			
		}

		Sleep(50);

	};

		
	return 0;
}

