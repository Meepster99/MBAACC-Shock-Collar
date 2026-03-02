
#include "DebugInfo.h"
//#include "DirectX.h"
#include <string>
//#include "dllmain.h"
#include "Shared.h"

typedef DWORD IDirect3DTexture9;

extern bool hasTextureAddr(DWORD test);
CommandData* getCommandDataFromInput(PlayerData* PD, const char input[20]);

// for reasons unknown to all above and below, this shit wont work with bools
int verboseShowPlayers = 0; // show player stuff
int verboseShowEffects = 0; // show effect stuff
int verboseShowUnknown = 0; // show styuff if i dont know what it is
int verboseShowPatternState = 0;
int verboseShowPos = 0;
int verboseShowVel = 0;
int verboseShowAccel = 0;
int verboseShowUntech = 0;
int verboseShowDamage = 0;
int verboseShowJumpcancel = 0;

DWORD _naked_getCancelStatusObj;
DWORD _naked_getCancelStatusSpecial;
DWORD _naked_getCancelStatusOutput;
__declspec(naked, noinline) void _naked_getCancelStatus() {

	PUSH_ALL;

	__asm {
		//mov ecx, 00055134h;
		mov ecx, _naked_getCancelStatusObj;
		push _naked_getCancelStatusSpecial; // does this work?
		//int 3;
	};
	emitCall(0x00463330);
	__asm {
		add esp, 4;
		mov _naked_getCancelStatusOutput, eax;
	};

	POP_ALL;

	ASM_RET;
}


DWORD _naked_canInputOccurObj;
DWORD _naked_canInputOccurCmd;
DWORD _naked_canInputOccurRes;
__declspec(naked, noinline) void _naked_canInputOccur() {


	PUSH_ALL;

	__asm {
		mov eax, _naked_canInputOccurCmd;
		mov ecx, _naked_canInputOccurObj;
		push 00000000h;
		push 00000000h;
	}
	emitCall(0x0046cc40);
	__asm {
		add esp, 8h;
		mov _naked_canInputOccurRes, eax;
	}

	POP_ALL;

	ASM_RET;



}

void EffectData::describe(char* buffer, int bufLen) {

	DWORD offset = ((DWORD)&exists);
	int index = -1;
	const char* entityString = "P";
	if (offset >= 0x0067BDE8) {
		index = (offset - 0x0067BDE8) / 0x33C;
		entityString = "E";
	} else {
		index = (offset - 0x00555130) / 0xAFC;
	}

	int bufferOffset = 0;
	//bufferOffset = snprintf(buffer, bufLen, "%s%d P%d S%d\n(%d,%d)\nUNTCH%d\n", entityString, index, subObj.pattern, subObj.state, subObj.xPos, subObj.yPos, subObj.totalUntechTime);

	bufferOffset += snprintf(buffer + bufferOffset, bufLen - bufferOffset, "%s%d: ", entityString, index);

	if (verboseShowPatternState) {
		bufferOffset += snprintf(buffer + bufferOffset, bufLen - bufferOffset, "P%d S%d\n", subObj.pattern, subObj.state);
	}

	if (verboseShowPos) {
		bufferOffset += snprintf(buffer + bufferOffset, bufLen - bufferOffset, "p:(%d,%d)\n", subObj.xPos, subObj.yPos);
	}

	if (verboseShowVel) {
		bufferOffset += snprintf(buffer + bufferOffset, bufLen - bufferOffset, "v:(%d,%d)\n", subObj.xVel, subObj.yVel);
	}

	if (verboseShowAccel) {
		bufferOffset += snprintf(buffer + bufferOffset, bufLen - bufferOffset, "a:(%d,%d)\n", subObj.xAccel, subObj.yAccel);
	}

	if (verboseShowUntech) {
		bufferOffset += snprintf(buffer + bufferOffset, bufLen - bufferOffset, "UNTCH%d\n", subObj.totalUntechTime);
	}

	if (verboseShowDamage && subObj.attackDataPtr != NULL) {
		bufferOffset += snprintf(buffer + bufferOffset, bufLen - bufferOffset, "DMG%d PROR%d\n", subObj.attackDataPtr->damage, subObj.attackDataPtr->proration);
	}

	if (verboseShowJumpcancel) {
		bufferOffset += snprintf(buffer + bufferOffset, bufLen - bufferOffset, "Jump:%d\n", subObj.comboJumpCancel);
	}

	return;

	bufferOffset += snprintf(buffer + bufferOffset, bufLen - bufferOffset, "idk%d\n", playerAuxDataArr[1].inactionableFrames);
	
	BYTE stance = 0;
	BYTE canMove = 0;
	BYTE cancelNorm = 0;
	BYTE cancelSpec = 0;
	DWORD flagset1 = 0;
	DWORD flagset2 = 0;
	DWORD omfgomfg = -1;
	if(subObj.animationDataPtr != NULL && subObj.animationDataPtr->stateData != NULL) {
		stance = subObj.animationDataPtr->stateData->stance;
		canMove = subObj.animationDataPtr->stateData->canMove;
		cancelNorm = subObj.animationDataPtr->stateData->cancelNormal;
		cancelSpec = subObj.animationDataPtr->stateData->cancelSpecial;
		flagset1 = subObj.animationDataPtr->stateData->flagset1;
		flagset2 = subObj.animationDataPtr->stateData->flagset2;	 
		//omfgInput = (DWORD)&subObj;
		//omfg();
		//omfgomfg = getCancelStatus(index, "2c");
	}

	bufferOffset += snprintf(buffer + bufferOffset, bufLen - bufferOffset, "IDK: %02X %02X %02X %02X\n", stance, canMove, cancelNorm, cancelSpec);

	bufferOffset += snprintf(buffer + bufferOffset, bufLen - bufferOffset, "IDK: %08X\n", flagset1);
	bufferOffset += snprintf(buffer + bufferOffset, bufLen - bufferOffset, "IDK: %08X\n", flagset2);

	bufferOffset += snprintf(buffer + bufferOffset, bufLen - bufferOffset, "IDK: %08X\n", omfgomfg);

}

PatternData* EffectData::getPatternDataPtr(int p) {
	// doing this in a more normal way could never get me the results i wanted
	__try {
		HA6Data* ha6 = playerDataArr[0].subObj.charFileDataPtr->DataFile;
		if (!ha6) return 0;
		ArrayContainer<PatternData*>* patCont = ha6->patternContainer;
		if (!patCont || patCont->count < p) return 0;
		PatternData* pat = (patCont->array)[p];
		if (!pat) return 0;
		return pat;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return NULL;
	}
}

AnimationData* EffectData::getAnimationDataPtr(int p, int s) { 
	__try {
		PatternData* pattern = getPatternDataPtr(p);
		//DWORD temp = (DWORD)pattern->ptrToAnimationDataArr->animationDataArr;
		//return (AnimationData*)(temp + (0x54 * s));
		return &(pattern->animationDataContainer->array[s]);
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return NULL;
	}
}

// -----

PlayerData* playerDataArr = (PlayerData*)(0x00555130);
PlayerAuxData* playerAuxDataArr = (PlayerAuxData*)(0x00557DB8);
EffectData* effectDataArr = (EffectData*)(0x0067BDE8);

#pragma pack(push,1)
typedef struct LinkedListTest {
	LinkedListTest* nextLink;
	DWORD unknown1;
	DWORD unknown2;
	DWORD unknown3;
} LinkedListTest;
#pragma pack(pop)

static_assert(sizeof(LinkedListTest) == 0x10, "LinkedListTest should have size 0x10");

#pragma pack(push,1)
typedef struct UnknownMallocData { 
	union {
		struct { // i would really like to find the sprite index in here!
			UNUSED(4);
			BYTE type; // 004c0231
			UNUSED(3);
			UNUSED(0x80);
			IDirect3DTexture9* texture; // huge. actually so huge
			UNUSED(8);
		};
		DWORD allData[0x94 / 4];
	};
};
#pragma pack(pop)

#define CHECKOFFSET(v, n) static_assert(offsetof(UnknownMallocData, v) == n, "UnknownMallocData offset incorrect for " #v);

CHECKOFFSET(type, 4);
CHECKOFFSET(texture, 0x88);

static_assert(sizeof(UnknownMallocData) == 0x94, "UnknownMallocData should have size 0x94");

#undef CHECKOFFSET

#pragma pack(push,1)
typedef struct UnknownDrawData { // ghidra only lets you rerun autocreatestructure if you right click the variable NAME not type. omfg
	// this struct and its size are mostly guesses from ghidra, assume everything is incorrect
	// highkey, what do i need? i need a method of getting WHATS being drawn from this struct. nothing else. that saves me that 
	// map, its lookups, and is everything.
	union {
		struct {
			DWORD unknown1;
			DWORD unknown2;
			UNUSED(12);
			DWORD* unknown3;	
			UNUSED(60);
			struct UnknownHasTexture {
				UNUSED(12);
				IDirect3DTexture9* tex; // this is the [0x54] + 0xC i was getting addresses from 
			} *unknownHasTexture; // i could also name this UnknownHasTexture. 
			//UnknownHasTexture* unknownHasTexture; 
			DWORD* unknown4;
			DWORD unknown5;
			DWORD unknown6;
			DWORD unknown7;
			IDirect3DTexture9* tex;
			BYTE unknown9;
			BYTE unknown10;
			short unknown11;
			short unknown12;
		};
		struct {
			DWORD dwordData[0x6C / 4];
			UNUSED(6);
		};
		BYTE data[0x72];
	};
} UnknownDrawData;
#pragma pack(pop)

#define CHECKOFFSET(v, n) static_assert(offsetof(UnknownDrawData, v) == n, "UnknownDrawData offset incorrect for " #v);

CHECKOFFSET(unknownHasTexture, 0x54);
CHECKOFFSET(tex, 0x68);
CHECKOFFSET(unknown9, 0x6C);

static_assert(sizeof(UnknownDrawData) == 0x72, "UnknownDrawData should have size 0x72");

#undef CHECKOFFSET

int getComboCount() {
	// go read 00478c38
	// this only gets the combo count for p1, and doesnt reset properly without more code

	if (playerDataArr[1].subObj.notInCombo) {
		return 0;
	}

	DWORD iVar2 = *(BYTE*)(0x0055df0f);
	iVar2 *= 0x20C;
	int iVar1 = *(int*)(0x00557e20 + iVar2);

	DWORD res = *(DWORD*)(0x00557e28 + iVar1 * 0x18 + iVar2 + 4);

	return res;
}

