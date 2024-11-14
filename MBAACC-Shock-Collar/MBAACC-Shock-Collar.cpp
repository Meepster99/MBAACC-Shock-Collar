
#include <stdio.h>
#include <windows.h>
#include <iostream>
#include <string>
#include <iostream>
#include <fstream>
#include <tlhelp32.h>
#include <wininet.h>
#include <cstdlib>
#include <csignal>
#include "resource.h"
#pragma comment(lib, "wininet.lib")
#include "../Shared/Shared.h"

#include "../Shared/version.h"

static_assert(sizeof(void*) == 4, "program must be compiled in 32 bit mode");

CollarManager collarManager;
Pipe pipe;

/*

a note.
while i could put this var into a shared memory section and launch calls from the game, i would rather
not risk the making of that request causing lag. so ill do some shared memory bs

*/

bool meltyInjected = false;

std::wstring getDLLPath() {
	wchar_t buffer[1024];
	if (!GetTempPathW(1024, buffer)) {
		printf(RED "Failed to get temp path\n" RESET);
		return L"";
	}
	std::wstring dllPath = std::wstring(buffer) + std::wstring(L"\\MBAACC-Shock-Collar-DLL.dll");
	return dllPath;
}

bool writeDLL() {

	std::wstring dllPath = getDLLPath();

	HINSTANCE hInstance = GetModuleHandle(NULL);

	HRSRC hRes = FindResource(hInstance, MAKEINTRESOURCE(IDR_DLL1), L"DLL");
	if (!hRes) {
		printf(RED "Failed to FindResource %d\n" RESET, GetLastError());
		return false;
	}

	HGLOBAL hData = LoadResource(hInstance, hRes);
	if (!hData) {
		printf(RED "Failed to LoadResource\n" RESET);
		return false;
	}
	void* pData = LockResource(hData);
	if (!pData) {
		printf(RED "Failed to LockResource\n" RESET);
		return false;
	}

	DWORD dwSize = SizeofResource(hInstance, hRes);

	std::ofstream outFile(dllPath, std::ios::binary);
	if (outFile) {
		outFile.write(reinterpret_cast<char*>(pData), dwSize);
	} else {
		printf(RED "Failed to open dll file\n" RESET);
		return false;
	}	

	return true;
}

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

	std::wstring dllPath = getDLLPath(); //	 std::wstring(buffer) + std::wstring(L"\\MBAACC-Shock-Collar-DLL.dll");

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

void SetConsoleSize(int width, int height) {
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hConsole == INVALID_HANDLE_VALUE) {
		return;
	}


	SMALL_RECT rect;
	rect.Left = 0;
	rect.Top = 0;
	rect.Right = width - 1;
	rect.Bottom = height - 1;
	SetConsoleWindowInfo(hConsole, TRUE, &rect);

	COORD bufferSize;
	bufferSize.X = width;
	bufferSize.Y = height;
	SetConsoleScreenBufferSize(hConsole, bufferSize);

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

	printf("MBAACC Shock Collar Version: %s%s" RESET ". " YELLOW "<3\n" RESET, !BLEEDING ? RED : "", GIT_VERSION);

	printf(WHITE "Press " CYAN "'R'" RESET " to reload the config file. Click " CYAN "<here>" RESET " to open it. Make sure to save!\n");
	
	printf("Press " CYAN "'1'" RESET " or " CYAN "'2'" RESET " to send a min strength pulse. Hold " CYAN "Shift" RESET " for max strength.\n");

	printf("\n");

	printf("Melty Status: %s%s\n\n" RESET, meltyInjected ? CYAN : RED, meltyInjected ? "Injected" : "Not Found");

	collarManager.displayStatus();

}

HANDLE exitEvent = NULL;

void onExit() {
	// uninject the dll, hopefully, to make my compilation easier
	SetEvent(exitEvent);

	//Sleep(1000);

	CloseHandle(exitEvent);
}

void sigtermHandle(int sig) {
	onExit();
	std::exit(sig);
}

BOOL WINAPI ConsoleHandler(DWORD signal) {
	if (signal == CTRL_CLOSE_EVENT) {
		onExit();
	}
	return TRUE;
}

int main() {
	
	//printf("%d\n", sizeof(PipePacket));
	//return 0;

	SetConsoleTitle(L"MBAACC-Shock-Collar");

	SetConsoleSize(85, 30);

	std::atexit(onExit);
	std::signal(SIGTERM, sigtermHandle); 
	SetConsoleCtrlHandler(ConsoleHandler, TRUE);

	// event needs to be created here so that its actually blockable.
	exitEvent = CreateEvent(
		NULL,
		FALSE,
		FALSE,
		L"MBAACCSHOCKCOLLAREXIT"
	);

	if (exitEvent == NULL) {
		printf(RED "failed to create exit event\n" RESET);
		Sleep(1000);
		return 0;
	}

	if (!writeDLL()) {
		Sleep(1000);
		return 0;
	}
	
	timeBeginPeriod(1);

	// stop pausing the window when someone clicks on it
	HANDLE hInput;
	DWORD prev_mode;
	hInput = GetStdHandle(STD_INPUT_HANDLE);
	GetConsoleMode(hInput, &prev_mode);
	SetConsoleMode(hInput, (prev_mode & ENABLE_EXTENDED_FLAGS) | ENABLE_MOUSE_INPUT);

	// why does altering the cursor info somehow enable colors.
	HANDLE hConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_CURSOR_INFO cursorInfo;
	GetConsoleCursorInfo(hConsoleHandle, &cursorInfo);
	cursorInfo.bVisible = false;
	SetConsoleCursorInfo(hConsoleHandle, &cursorInfo);
	SetConsoleMode(hConsoleHandle, 7);

	INPUT_RECORD inputRecord;
	DWORD events;

	printf(RESET);

	KeyState rKey('R');
	KeyState oneKey('1');
	KeyState twoKey('2');
	KeyState shiftKey(VK_SHIFT);
	KeyState lMouse(VK_LBUTTON);

	collarManager.readSettings();
	renderConsole();

	pipe.initServer();

	unsigned tick = 0;
	unsigned lastShockTick = 0;

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
			printf(CLEARHORIZONTAL CYAN "reloading config\r" RESET);
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
				printf(GREEN "%s collar %d with a strength of %d\r" RESET, getShockTypeVerb(shockType), collarIndex + 1, shockValue);
			} else {
				printf(RED "failed to send instruction to collar %d\r" RESET, collarIndex + 1);
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
			
			//playerPackets[temp.player].value().strength = MAX(playerPackets[temp.player].value().strength, temp.strength);
			
			recvData = pipe.pop();
		}

		for (int i = 0; i < 2; i++) {
			if (playerPackets[i].has_value()) {
				
				collarManager.sendShock(playerPackets[i].value());
				
				lastShockTick = tick;
			}
		}

		if (tick - lastShockTick == 500) {
			printf("\x1b[6A");
			collarManager.displayModifiers();
		}

		// readconsoleinput wasnt blocking while i was plugged in, but was on battery?
		if (PeekConsoleInput(hInput, &inputRecord, 1, &events) && events > 0) {
			ReadConsoleInput(hInput, &inputRecord, 1, &events);

			if (inputRecord.EventType == MOUSE_EVENT) {
				MOUSE_EVENT_RECORD& mer = inputRecord.Event.MouseEvent;

				if (mer.dwMousePosition.Y == 1 && (mer.dwMousePosition.X >= 42 && mer.dwMousePosition.X <= 49)) {
					if (lMouse.keyDown()) {
						wchar_t filenameBuffer[1024];
						GetCurrentDirectory(1024, filenameBuffer);
						std::wstring filePath = std::wstring(filenameBuffer) + L"\\shockSettings.txt";
						ShellExecute(NULL, L"open", L"notepad.exe", filePath.c_str(), NULL, SW_SHOWNORMAL);
					}
				}
			}
		}
	
		Sleep(1);

	};

		
	return 0;
}

