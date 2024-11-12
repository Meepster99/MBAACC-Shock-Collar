#pragma once

#include <stdio.h>
#include <windows.h>
#include <iostream>
#include <string>
#include <iostream>
#include <fstream>
#include <regex>

#include <wininet.h>
#pragma comment(lib, "wininet.lib")

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define CLAMP(value, min_val, max_val) MAX(MIN((value), (max_val)), (min_val))

#define RESET "\033[0m\033[97m" // an extra white is tacked on here just to make it the default color

#define RED "\033[91m"
#define GREEN "\033[92m"
#define YELLOW "\033[93m"
#define BLUE "\033[94m"
#define MAGENTA "\033[95m"
#define CYAN "\033[96m"
#define WHITE "\033[97m"


class KeyState {
public:
	
	KeyState() {
		nKey = 0x00;
		nHeldKeyCounter = 0;
	}

	KeyState(int k) {
		nKey = k;
		nHeldKeyCounter = 0;
	}

	bool isFocused() {
		return GetConsoleWindow() == GetForegroundWindow();
	}

	bool keyHeld() {
		if (!isFocused()) {
			freqHeldCounter = 0;
			return false;
		}

		bool res = nKey != 0x0 && GetAsyncKeyState(nKey) & 0x8000;

		return res;
	}

	bool keyDown() {
		if (nKey == 0x0 || !isFocused()) {
			return false;
		}

		tempState = false;
		if (GetAsyncKeyState(nKey) & 0x8000)
			tempState = true;

		bool res = false;
		if (!prevState && tempState) {
			res = true;
		}

		prevState = tempState;

		return res;
	}

public:
	int nHeldKeyCounter;
	int freqHeldCounter = 0;
private:
	uint8_t nKey;
	bool prevState = false;
	bool tempState = false;
};

enum class ShockType {
	Shock = 0,
	Sound = 1,
	Vibrate = 2,
};

const char* const shockTypeNames[3] = { "Shock", "Sound", "Vibrate" };

inline const char* getShockTypeName(ShockType t) {
	int temp = static_cast<int>(t);

	if (temp < 0 || temp > 2) {
		return "???";
	}

	return shockTypeNames[temp];
}

const char* const shockTypeVerbs[3] = { "Zapped", "Made a sound for", "Vibrated" };

inline const char* getShockTypeVerb(ShockType t) {
	int temp = static_cast<int>(t);

	if (temp < 0 || temp > 2) {
		return "???";
	}

	return shockTypeVerbs[temp];
}

class Collar {
public:

	Collar() {}
	
	void setID(const char* id_);

	void displayStatus();

	char id[256] = { '\0' };
	
	int minShock = 0;
	int maxShock = 0;
	ShockType shockType = ShockType::Shock;

};

class CollarManager {
public:

	CollarManager() {}

	void setToken(const char* token_);

	bool sendShock(int player, int strength, int duration);

	void displayStatus();
	void readSettings(int depth = 0);

	char token[256] = { '\0' };

	union {
		Collar collars[2];
		struct {
			Collar collar1;
			Collar collar2;
		};
	};

};




