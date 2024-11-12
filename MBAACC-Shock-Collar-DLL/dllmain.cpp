
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

void updateBattleSceneCallback() {

	/*if (pipe.clientHandle == NULL) {
		__asm {
			int 3;
		}
	}*/

	// todo, chip damage shouldnt shock you!
	// make shield shock opponent

	static int playerHealth[2] = { 11400, 11400 };
	static byte bounceCounts[2] = { 0, 0 };

	for (int player = 0; player < 2; player++) {

		DWORD playerAddr = 0x00555130 + (player * 0xAFC);

		int tempPlayerHealth = *(int*)(playerAddr + 0xBC);
		if (tempPlayerHealth != playerHealth[player]) {
			
			if (tempPlayerHealth > playerHealth[player]) {
				playerHealth[player] = tempPlayerHealth;
				continue;
			}
			
			int healthDelta = playerHealth[player] - tempPlayerHealth;

			playerHealth[player] = tempPlayerHealth;
			
			DWORD attackDataPointer = *(DWORD*)(playerAddr + 0x1FC);
			if (attackDataPointer == 0) { // todo, i should probs do something to prevent attack pointer reuse on grab, despite it being funny
				continue;
			}

			if (*(BYTE*)(playerAddr + 0x17B)) { // we are blocking, continue
				continue;
			}

			PipePacket packet;

			uint32_t hitFlags = *(uint32_t*)(attackDataPointer + 0x3C);

			packet.player = player;
			packet.counterhit = (*(BYTE*)(playerAddr + 0x194)) == 0;
			packet.screenshake = !!(hitFlags & 0b01000000);

			BYTE tempBounce = *(BYTE*)(playerAddr + 0x17E);
			packet.bounce = tempBounce > bounceCounts[player];
			bounceCounts[player] = tempBounce;

			// todo, need crit



			uint16_t damage = *(uint16_t*)(attackDataPointer + 0x44);		
			
			// todo, grab the proation and then multiply it by this value!
			DWORD local_154 = 0;
			local_154 = *(BYTE*)0x0055df0f; // 00478bda
			local_154 *= 0x20C; // 00478be7
			local_154 += 0x00557db8; // 00478bed

			// recreate local_150
			DWORD local_150 = 0x00555134 + ((*(DWORD*)(local_154)) * 0xAFC);
			unsigned correctionValue = *(DWORD*)(local_154 + 0x20);
			if (correctionValue == 0) {
				correctionValue = 100;
			}

			damage = round(((float)damage) * ((float)correctionValue * 0.01f));

			packet.setStrength(damage);

			pipe.push(packet);
		}
	}

	// making length proportional to hitstop (or something?) might be a good idea

}

// -----

__declspec(naked) void _naked_updateBattleSceneCallback() {
	
	PUSH_ALL;
	updateBattleSceneCallback();
	POP_ALL;

	__asm _emit 0xB8;
	__asm _emit 0x01;
	__asm _emit 0x00;
	__asm _emit 0x00;
	__asm _emit 0x00;

	__asm _emit 0x5E;

	__asm {
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

	patchJump(0x004540b8, _naked_updateBattleSceneCallback);

	// doing this in here is not ideal, but unloading the dll while inside dll code is a huge risk. at least something being blocking is actually helpful now
	// this is so nice, but wouldnt be feasable on larger projects

	PipePacket temp;

	HANDLE hEvent = OpenEvent(
		EVENT_ALL_ACCESS,
		FALSE,           
		L"MBAACCSHOCKCOLLAREXIT"
	);

	if (hEvent == NULL) {
		temp.__unused = GetLastError();
		pipe.push(temp);
		return;
	}

	DWORD result = WaitForSingleObject(hEvent, INFINITE); 
	CloseHandle(hEvent);

	
	temp.__unused = 42;
	pipe.push(temp);

	BYTE code[5] = { 0xB8, 0x01, 0x00, 0x00, 0x00 };
	patchMemcpy(0x004540b8, &code, 5);
	
	if (pipe.clientHandle) {
		CloseHandle(pipe.clientHandle);
		pipe.clientHandle = NULL;
	}

	FreeLibraryAndExitThread(GetModuleHandle(L"MBAACC-Shock-Collar-DLL.dll"), 0);
	
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

