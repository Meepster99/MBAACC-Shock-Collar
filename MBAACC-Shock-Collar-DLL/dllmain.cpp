
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

	DWORD playerAddr;
	static int playerHealth[2] = { 11400, 11400 };
	static BYTE bounceCounts[2] = { 0, 0 };
	static unsigned prevCorrection[2] = { 100, 100 };
	static bool prevNotInCombo[2] = {true, true};

	// you can reduce until one frame AFTER. that is why this exists. additionally, im only sending a max of one shock per frame, so a queue isnt needed
	// i hope that the flag is set long enough though, i will need a queue if otherwise
	static std::optional<PipePacket> packetQueue[2];

	for (int player = 0; player < 2; player++) {
		playerAddr = 0x00555130 + (player * 0xAFC);
		BYTE reduceStatus = *(BYTE*)(playerAddr + 0x348);

		if (reduceStatus != 2 && packetQueue[player].has_value()) {
			pipe.push(packetQueue[player].value());
		}

		packetQueue[player].reset();
	}

	for (int player = 0; player < 2; player++) {

		// check 004770f0 in ghidra for an explanation
		playerAddr = 0x00555130 + ((player) * 0xAFC);

		bool temp = *(bool*)(playerAddr + 0x64);

		if(!prevNotInCombo[player] && temp) {
			prevCorrection[0] = 100;
			prevCorrection[1] = 100;
		}

		prevNotInCombo[player] = temp;
	}

	for (int player = 0; player < 2; player++) {

		playerAddr = 0x00555130 + (player * 0xAFC);

		BYTE tempBounce = *(BYTE*)(playerAddr + 0x17E);
		bool bounceDamage = tempBounce > bounceCounts[player];
		bounceCounts[player] = tempBounce;

		PipePacket packet;

		int tempPlayerHealth = *(int*)(playerAddr + 0xBC);
		if (tempPlayerHealth != playerHealth[player] || bounceDamage) {
			
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

			BYTE reduceStatus = *(BYTE*)(playerAddr + 0x348);
			if (reduceStatus == 2) { // successful reduce
				continue;
			}

			uint32_t hitFlags = *(uint32_t*)(attackDataPointer + 0x3C);

			packet.player = player;
			packet.counterhit = (*(BYTE*)(playerAddr + 0x194)) == 0;
			packet.screenshake = !!(hitFlags & 0b01000000);
			packet.bounce = bounceDamage;
			packet.reduceFail = reduceStatus == 1;
		
			// todo, need crit

			uint16_t damage = *(uint16_t*)(attackDataPointer + 0x44);
			
			damage = round(((float)damage) * ((float)prevCorrection[player] * 0.01f));

			packet.setStrength(damage);
			
			//pipe.push(packet);

			packetQueue[player] = packet;

			//packet.errorBit = 1;
			//packet.error = prevCorrection[player];
			//pipe.push(packet);
		}
	}

	for (int player = 0; player < 2; player++) {

		if (prevNotInCombo[1-player]) {
			continue;
		}

		// check 004770f0 in ghidra for an explanation
		playerAddr = 0x00555130 + 4 + ((player) * 0xAFC);

		int iVar4 = ((int)*(BYTE*)(playerAddr + 0x2F0)) * 0x20C;
		unsigned temp1 = (0x00557dd8 + iVar4);

		BYTE corVal = *(BYTE*)temp1;
		if (corVal == 0) {
			corVal = 100;
		}
		prevCorrection[1 - player] = corVal;
		
	}
	
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

