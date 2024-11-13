#pragma once

#include <stdio.h>
#include <windows.h>
#include <iostream>
#include <string>
#include <iostream>
#include <fstream>
#include <regex>
#include <optional>
#include <wininet.h>
#include <timeapi.h>
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "winmm.lib")

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define CLAMP(value, min_val, max_val) MAX(MIN((value), (max_val)), (min_val))

#define PUSH_ALL __asm  \
{                       \
	__asm push esp		\
	__asm push ebp		\
	__asm push edi		\
	__asm push esi		\
	__asm push edx		\
	__asm push ecx		\
	__asm push ebx  	\
	__asm push eax		\
	__asm push ebp      \
	__asm mov ebp, esp  \
}

#define POP_ALL __asm  \
{                      \
	__asm pop ebp      \
	__asm pop eax	   \
	__asm pop ebx  	   \
	__asm pop ecx	   \
	__asm pop edx	   \
	__asm pop esi	   \
	__asm pop edi	   \
	__asm pop ebp	   \
	__asm pop esp	   \
}

#define emitByte(b) __asm _emit b;
#define emitWord(b) \
    __asm { _emit b & 0xFF } \
    __asm { _emit b >> 8 } 
#define emitDword(b) \
    __asm { _emit b & 0xFF } \
    __asm { _emit (b & 0xFF00) >> 8 } \
    __asm { _emit (b & 0xFF0000) >> 16 } \
    __asm { _emit (b & 0xFF000000) >> 24 } 

#define CONCATENATE_DETAIL(x, y) x##y
#define CONCATENATE(x, y) CONCATENATE_DETAIL(x, y)
#define LINE_NAME CONCATENATE(LINE, __LINE__)

#define emitCall(addr) \
    __asm { push LINE_NAME } \
	__asm { push addr } \
    __asm { ret } \
    __asm { LINE_NAME: }

#define emitJump(addr) \
	__asm { push addr } \
    __asm { ret }

#define RESET "\x1b[0m\x1b[97m" // an extra white is tacked on here just to make it the default color

#define RED "\x1b[91m"
#define GREEN "\x1b[92m"
#define YELLOW "\x1b[93m"
#define BLUE "\x1b[94m"
#define MAGENTA "\x1b[95m"
#define CYAN "\x1b[96m"
#define WHITE "\x1b[97m"


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
	Stop = 3,
};

const char* const shockTypeNames[4] = { "Shock", "Sound", "Vibrate", "Stop"};

inline const char* getShockTypeName(ShockType t) {
	int temp = static_cast<int>(t);

	if (temp < 0 || temp > 3) {
		return "???";
	}

	return shockTypeNames[temp];
}

const char* const shockTypeVerbs[4] = { "Zapped", "Made a sound for", "Vibrated", "Stopped" };

inline const char* getShockTypeVerb(ShockType t) {
	int temp = static_cast<int>(t);

	if (temp < 0 || temp > 3) {
		return "???";
	}

	return shockTypeVerbs[temp];
}

// -----

typedef struct PipePacket {
	PipePacket() {}

	inline void setStrength(int s) {
		strength = round(((float)s) / 28.0f);
		strength = CLAMP(strength, 0, 255);
	}
	inline int getStrength() {
		return strength * 28;
	}

	union {

		struct {
			uint8_t __unusedErrorBit : 1;
			uint8_t player : 1;
			uint8_t counterhit : 1;
			uint8_t screenshake : 1;
			uint8_t bounce : 1;
			uint8_t crit : 1; 
			uint8_t reduce : 1;
			uint8_t _unused4 : 1;
			
			uint8_t strength : 8; // move damage. this can be reduced to 

		};

		struct {
			uint16_t errorBit : 1;
			//uint16_t error : 15;
			uint32_t error;
			
		};

		uint16_t __unused;
	};
	

} PipePacket;

//static_assert(sizeof(PipePacket) == 2, "PipePacket was not the expected size ");

class Pipe {
public:

	Pipe() {}
	~Pipe();

	void initServer();
	void initClient();

	bool peek();
	std::optional<PipePacket> pop();
	void push(PipePacket data);


	HANDLE serverHandle = NULL;
	HANDLE clientHandle = NULL;

	static constexpr const char* pipeName = "\\\\.\\pipe\\MBAACCSHOCKCOLLARPIPE";

};

// -----

class Collar {
public:

	Collar() {}
	
	void setID(const char* id_);

	void displayStatus();

	char id[256] = { '\0' };
	
	float minShock = 0;
	float maxShock = 0;
	ShockType shockType = ShockType::Shock;
	bool online = false;

};

class CollarManager {
public:

	CollarManager() {}

	void setToken(const char* token_);

	bool sendShock(int player, int strength, int duration, bool quiet = false);
	void sendShock(PipePacket packet);

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

