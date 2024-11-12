
#include "../Shared/Shared.h"

Pipe pipe;

/*

some thoughts
counter hits, last arcs, and maybe ADs should hurt more?

*/

void __stdcall patchMemcpy(auto dst, auto src, size_t n) {

	static_assert(sizeof(dst) == 4, "Type must be 4 bytes");
	static_assert(sizeof(src) == 4, "Type must be 4 bytes");

	LPVOID dest = reinterpret_cast<LPVOID>(dst);
	LPVOID source = reinterpret_cast<LPVOID>(src);

	DWORD oldProtect;
	VirtualProtect(dest, n, PAGE_EXECUTE_READWRITE, &oldProtect);
	memcpy(dest, source, n);
	VirtualProtect(dest, n, oldProtect, NULL);
}

void __stdcall patchJump(auto patchAddr_, auto newAddr_) {

	static_assert(sizeof(patchAddr_) == 4, "Type must be 4 bytes");
	static_assert(sizeof(newAddr_) == 4, "Type must be 4 bytes");

	DWORD patchAddr = (DWORD)(patchAddr_);
	DWORD newAddr = (DWORD)(newAddr_);

	BYTE callCode[] = { 0xE9, 0x00, 0x00, 0x00, 0x00 };
	DWORD funcOffset = newAddr - (patchAddr + 5);
	*(unsigned*)(&callCode[1]) = funcOffset;
	patchMemcpy(patchAddr, callCode, sizeof(callCode));
}

void __stdcall patchByte(auto addr, const BYTE byte) {
	static_assert(sizeof(addr) == 4, "Type must be 4 bytes");

	BYTE temp[] = { byte };

	patchMemcpy(addr, temp, 1);
}

// -----

void takeHitsHook() {

	/*if (pipe.clientHandle == NULL) {
		__asm {
			int 3;
		}
	}*/

	static int playerHealth[2] = { 11400, 11400 };

	static int d = 0;
	d = (d + 1) % 100;

	for (int player = 0; player < 2; player++) {
		int tempPlayerHealth = *(int*)(0x00555130 + (player * 0xAFC) + 0xBC);
		if (tempPlayerHealth != playerHealth[player]) {
			
			if (tempPlayerHealth > playerHealth[player]) {
				playerHealth[player] = tempPlayerHealth;
				continue;
			}
			
			playerHealth[player] = tempPlayerHealth;
			
			DWORD attackDataPointer = *(DWORD*)(0x00555130 + (player * 0xAFC) + 0x1FC);
			if (attackDataPointer != 0) {
				uint16_t damage = *(uint16_t*)(attackDataPointer + 0x44);
				// what is the highest single hitting thing in the game? this needs to be converted to a 0-100 value

				float damagePercent = MIN((float)(damage) / 3000.0f, 1.0f);

				pipe.send(player, 100 * damagePercent, 300);
			}
		}
	}

	// making length proportional to hitstop (or something?) might be a good idea

}

// -----

__declspec(naked) void _naked_takeHitsHook() {
	
	PUSH_ALL;
	takeHitsHook();
	POP_ALL;

	__asm {
		//add esp, 0180h;
		ret;
	}
}

// -----

void threadFunc() {

	pipe.initClient();

	/*

	00471880 sets the if blocking flag
	0046f854 sets the receiving attack ptr

	00471880 is called before the recv ptr thing
	for now,, im tired, im just doing this once a frame
	which makes me wonder why im still finding hook positions instead of just going to framedone

	honestly screw it, im just doing that

	*/

	patchJump(0x0046e262, _naked_takeHitsHook);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
	switch (ul_reason_for_call) {
		case DLL_PROCESS_ATTACH:
			CreateThread(0, 0, (LPTHREAD_START_ROUTINE)threadFunc, 0, 0, 0);
			break;
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
			break;
		case DLL_PROCESS_DETACH:
			if (pipe.clientHandle) {
				CloseHandle(pipe.clientHandle);
				pipe.clientHandle = NULL;
			}
			break;
	}
	return TRUE;
}

