
#include <stdio.h>
#include <windows.h>
#include <iostream>
#include <string>
#include <iostream>
#include <fstream>
#include <tlhelp32.h>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")
#include "../Shared/Shared.h"

static_assert(sizeof(void*) == 4, "program must be compiled in 32 bit mode");

CollarManager collarManager;
Pipe pipe;

/*

a note.
while i could put this var into a shared memory section and launch calls from the game, i would rather
not risk the making of that request causing lag. so ill do some shared memory bs

*/

bool meltyInjected = false;

DWORD getPID(const wchar_t* name) {
	DWORD pid = 0;

	// Create toolhelp snapshot.
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 process;
	ZeroMemory(&process, sizeof(process));
	process.dwSize = sizeof(process);

	// Walkthrough all processes.
	if (Process32First(snapshot, &process)) {
		do {
			// Compare process.szExeFile based on format of name, i.e., trim file path
			// trim .exe if necessary, etc.
			if (std::wcscmp((process.szExeFile), name) == 0) {
				pid = process.th32ProcessID;
				break;
			}
		} while (Process32Next(snapshot, &process));
	}

	CloseHandle(snapshot);

	if (pid != 0) {
		return pid;
	}

	return 0;
}

bool inject() {

	DWORD pid = getPID(L"MBAA.exe");

	if (pid == 0) {
		return false;
	}

	// dlls need the FULL PATH or else they fail. silently. i hate that.

	wchar_t buffer[1024];
	if (!GetCurrentDirectoryW(MAX_PATH, buffer)) {
		return false;
	}

	std::wstring dllPath = std::wstring(buffer) + std::wstring(L"\\MBAACC-Shock-Collar-DLL.dll");

	size_t dllPathSize = (dllPath.length() + 1) * sizeof(wchar_t);

	std::ifstream f(dllPath);

	if (!f.good()) {
		printf(RED "failed to find dll\n" RESET);
		return false;
	}

	HANDLE injectorProcHandle = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_CREATE_THREAD, false, pid);
	if (injectorProcHandle == NULL) {
		printf(RED "OpenProcess failed\n" RESET);
		return false;
	}

	PVOID dllNameAdr = VirtualAllocEx(injectorProcHandle, NULL, dllPathSize, MEM_COMMIT, PAGE_READWRITE);
	if (dllNameAdr == NULL) {
		printf(RED "VirtualAllocEx failed\n" RESET);
		return false;
	}

	if (WriteProcessMemory(injectorProcHandle, dllNameAdr, dllPath.c_str(), dllPathSize, NULL) == 0) {
		printf(RED "WriteProcessMemory failed\n" RESET);
		return false;
	}

	HANDLE tHandle = CreateRemoteThread(injectorProcHandle, 0, 0, (LPTHREAD_START_ROUTINE)(void*)GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryW"), dllNameAdr, 0, 0);
	if (tHandle == NULL) {
		printf(RED "CreateRemoteThread failed\n" RESET);
		return false;
	}

	return true;
}


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

	printf(WHITE "Press " CYAN "'R'" RESET " to reload the config file(shockSettings.txt).make sure to save.\n" RESET);

	printf("Press " CYAN "'1'" RESET " or " CYAN "'2'" RESET " to send a min strength pulse to either of them.\n");
	printf("Hold " CYAN "shift" RESET " to send a max strength pulse instead\n");

	printf("\n");

	collarManager.displayStatus();

	printf("Melty Status: %s%s\n\n" RESET, meltyInjected ? CYAN : RED, meltyInjected ? "Injected" : "Not Found");

}

int main() {
	
	timeBeginPeriod(1);

	// stop pausing the window when someone clicks on it
	HANDLE hInput;
	DWORD prev_mode;
	hInput = GetStdHandle(STD_INPUT_HANDLE);
	GetConsoleMode(hInput, &prev_mode);
	SetConsoleMode(hInput, prev_mode & ENABLE_EXTENDED_FLAGS);

	// why does altering the cursor info somehow enable colors.
	HANDLE hConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_CURSOR_INFO cursorInfo;
	GetConsoleCursorInfo(hConsoleHandle, &cursorInfo);
	cursorInfo.bVisible = false;
	SetConsoleCursorInfo(hConsoleHandle, &cursorInfo);
	SetConsoleMode(hConsoleHandle, 7);

	printf(RESET);

	KeyState rKey('R');
	KeyState oneKey('1');
	KeyState twoKey('2');
	KeyState shiftKey(VK_SHIFT);

	collarManager.readSettings();
	renderConsole();

	pipe.initServer();

	unsigned tick = 0;

	while (true) {
		tick++;

		if (tick % 32 == 0) {
			if (meltyInjected) {
				if (getPID(L"MBAA.exe") == 0) {
					meltyInjected = false;
					renderConsole();
				}
			} else {
				meltyInjected = inject();
				if (meltyInjected) {
					renderConsole();
				}
			}
		}

		if (rKey.keyDown()) {
			printf(CYAN "reloading config\n" RESET);
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

			printf(CYAN "sending shock request\r" RESET);

			bool res = collarManager.sendShock(collarIndex, shockValue, 300);

			if (res) {
				printf(GREEN "%s collar %d with a strength of %d\n" RESET, getShockTypeVerb(shockType), collarIndex + 1, shockValue);
			} else {
				printf(RED "failed to send instruction to collar %d\n" RESET, collarIndex + 1);
			}

		}

		// long story short, sending more than one request a frame, caused,, issues to put it lightly 

		std::optional<PipePacket> playerPackets[2] = { std::optional<PipePacket>(), std::optional<PipePacket>() };

		std::optional<PipePacket> recvData = pipe.pop();
		while (recvData.has_value()) {
			
			PipePacket temp = recvData.value();

			if (!playerPackets[temp.player].has_value()) {
				playerPackets[temp.player] = temp;
				continue;
			}
			
			playerPackets[temp.player].value().strength = MAX(playerPackets[temp.player].value().strength, temp.strength);
			playerPackets[temp.player].value().length = MAX(playerPackets[temp.player].value().length, temp.length);

			recvData = pipe.pop();
		}

		for (int i = 0; i < 2; i++) {
			if (playerPackets[i].has_value()) {
				collarManager.sendShock(playerPackets[i].value());
			}
		}

		Sleep(1);

	};

		
	return 0;
}

