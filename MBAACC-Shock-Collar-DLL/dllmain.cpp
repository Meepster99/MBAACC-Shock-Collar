

#include <ws2tcpip.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

		
#include "../Shared/Shared.h"
#include "../Shared/DebugInfo.h"


Pipe pipe;

/*

some thoughts
counter hits, last arcs, and maybe ADs should hurt more?

*/

// no clue why, but adding this logging in made everything,,, much much slower?

void __stdcall ___log(const char* msg) {
	const char* ipAddress = "127.0.0.1";
	unsigned short port = 17474;

	int msgLen = strlen(msg);

	const char* message = msg;

	WSADATA wsaData;
	int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0) {
		return;
	}

	SOCKET sendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sendSocket == INVALID_SOCKET) {
		WSACleanup();
		return;
	}

	sockaddr_in destAddr;
	destAddr.sin_family = AF_INET;
	destAddr.sin_port = htons(port);
	if (inet_pton(AF_INET, ipAddress, &destAddr.sin_addr) <= 0) {
		closesocket(sendSocket);
		WSACleanup();
		return;
	}

	int sendResult = sendto(sendSocket, message, msgLen, 0, (sockaddr*)&destAddr, sizeof(destAddr));
	if (sendResult == SOCKET_ERROR) {
		closesocket(sendSocket);
		WSACleanup();
		return;
	}

	closesocket(sendSocket);
	WSACleanup();
}

void __stdcall log(const char* format, ...) {
	char buffer[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, 1024, format, args);
	___log(buffer);
	va_end(args);
}

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

typedef struct RawInput {
	BYTE dir = 0;
	BYTE btn = 0;
	BYTE what = 0; // FN1 is 1, FN2 is 2
	void set(int playerIndex) {
		DWORD baseControlsAddr = *(DWORD*)0x76E6AC;
		if (baseControlsAddr == 0) {
			return;
		}

		dir = *(BYTE*)(baseControlsAddr + 0x18 + (playerIndex * 0x14));
		btn = *(BYTE*)(baseControlsAddr + 0x24 + (playerIndex * 0x14));
		what = *(BYTE*)(baseControlsAddr + 0x25 + (playerIndex * 0x14));
	}
} RawInput;

RawInput playerInputs[4];

PlayerData* players[4] = {
	(PlayerData*)(0x00555130 + (0 * 0xAFC)),
	(PlayerData*)(0x00555130 + (1 * 0xAFC)),
	(PlayerData*)(0x00555130 + (2 * 0xAFC)),
	(PlayerData*)(0x00555130 + (3 * 0xAFC)),
};

// -----

/*
void updateBattleSceneCallback() {

	DWORD playerAddr;
	static int playerHealth[2] = { 11400, 11400 };
	static BYTE bounceCounts[2] = { 0, 0 };
	static unsigned prevCorrection[2] = { 100, 100 };
	static bool prevNotInCombo[2] = {true, true};

	// you can reduce until one frame AFTER. that is why this exists. additionally, im only sending a max of one shock per frame, so a queue isnt needed
	// i hope that the flag is set long enough though, i will need a queue if otherwise
	// testing needs to be done on this.
	static std::optional<PipePacket> packetQueue[2];

	for (int player = 0; player < 2; player++) {
		playerAddr = 0x00555130 + (player * 0xAFC);
		BYTE reduceStatus = *(BYTE*)(playerAddr + 0x348);

		if (reduceStatus != 2 && packetQueue[player].has_value()) {
		//if(packetQueue[player].has_value()) {
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

			BYTE effectType = *(BYTE*)(attackDataPointer + 0x30);

			packet.player = player;
			packet.counterhit = (*(BYTE*)(playerAddr + 0x194)) == 0;
			packet.screenshake = !!(hitFlags & 0b01000000);
			packet.bounce = bounceDamage;
			packet.reduceFail = reduceStatus == 1;
			packet.electricAttack = effectType == 3;
		
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
*/

void updateBattleSceneCallback() {

	// you can reduce until one frame after, which is why things are staggered

	static std::optional<PipePacket> packetQueue[4];

	for (int i = 0; i < 4; i++) {
		PlayerData* player = players[i];

		if (player->reduce != 2 && packetQueue[i].has_value()) {
			pipe.push(packetQueue[i].value());
		}

		packetQueue[i].reset();

		playerInputs[i].set(i);

		if (playerInputs[i].what & 0x02) { // FN2 
			PipePacket tempPacket;
			
			constexpr int shockLookup[4] = { 2, 3, 0, 1 };
			tempPacket.player = shockLookup[i];

			tempPacket.setStrength(9999);

			packetQueue[shockLookup[i]] = tempPacket;
		}

	
	}

	static BYTE prevThrowState[4] = { 0, 0, 0, 0 }; // tbh there has to be a better way to do rising edge bs
	static int playerHealth[4] = { 11400, 11400, 11400, 11400 };
	// i do not like proration.
	// instead, im just going to scale down shocks as time goes on
	static float shockMult[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	static int shockResetTimer[4] = {0, 0, 0, 0};

	for (int i = 0; i < 4; i++) {

		PlayerData* player = players[i];
		PipePacket packet;
		
		packet.player = i & 3;

		// simpler solution, more boring

		if (player->health < playerHealth[i]) {
			int delta = playerHealth[i] - player->health;

			packet.setStrength(delta);

			packet.counterhit = (player->counterhitState != 0);
			packet.reduceFail = (player->reduce == 1);

			packetQueue[i] = packet;
		}

		playerHealth[i] = player->health;

		/*
		if (!prevThrowState[i] && player->throwFlag) {
			packet.setStrength(1000); // i just,, ugh. ugh
			packetQueue[i] = packet;
			//log("throw");
		} else if (player->recievedHit) {
			if (player->recievingAttackPtr != NULL) {
				AttackData* atk = player->recievingAttackPtr;

				// proration is stored in the attacker. with 2v2, i have no idea how im going to track recieved bs
				// im just going to store my own thing here, its def not accurate. i do not care

				packet.setStrength((int)((float)atk->damage * shockMult[i]));

				packet.counterhit = (player->counterhitState != 0);
				packet.reduceFail = (player->reduce == 1);

				packetQueue[i] = packet;
				
				shockMult[i] *= 0.925f; // this number isnt real, and i made it up
				shockResetTimer[i] = 60;
			}
		}

		playerHealth[i] = player->health;
		prevThrowState[i] = player->throwFlag;
		
		if (shockResetTimer[i] > 0) {
			shockResetTimer[i]--;
			if (shockResetTimer[i] == 0) {
				shockMult[i] = 1.0f;
			}
		}
		*/

	}
}

// -----

__declspec(naked) void _naked_updateBattleSceneCallback1() {
	
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

__declspec(naked) void _naked_updateBattleSceneCallback2() {

	__asm _emit 0x55;

	__asm _emit 0x8B;
	__asm _emit 0xEC;

	__asm _emit 0x83;
	__asm _emit 0xE4;
	__asm _emit 0xF8;

	PUSH_ALL;
	updateBattleSceneCallback();
	POP_ALL;

	emitJump(0x0046dfd6);
	
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

	//patchJump(0x004540b8, _naked_updateBattleSceneCallback1);
	patchJump(0x0046dfd0, _naked_updateBattleSceneCallback2);

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

	// is this waitforsingleobj better or worse than peeking and then sleeping?
	DWORD result = WaitForSingleObject(hEvent, INFINITE); 
	CloseHandle(hEvent);
	
	temp.__unused = 42;
	pipe.push(temp);

	//BYTE code[5] = { 0xB8, 0x01, 0x00, 0x00, 0x00 };
	//patchMemcpy(0x004540b8, &code, 5);

	BYTE code[5] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4 };
	patchMemcpy(0x0046dfd0, &code, 5);
	
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
