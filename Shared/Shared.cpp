
#include "Shared.h"

std::string strip(const std::string& s) {
	std::string res = s;
	std::regex trim_re("^\\s+|\\s+$");
	res = std::regex_replace(res, trim_re, "");
	return res;
}

void lowerString(std::string& s) {
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
}

int safeStoi(const std::string& s) {
	int res = -1;
	try {
		res = std::stoi(s);
	} catch (...) {
		return -1;
	}
	return res;
}

float safeStof(const std::string& s) {
	float res = -1;
	try {
		res = std::stof(s);
	} catch (...) {
		return -1;
	}
	return res;
}

std::string censorString(const std::string& s) {
	std::string res = s;

	for (size_t i = 8; i < res.size(); i++) {
		if (res[i] == '-') {
			continue;
		}
		res[i] = '*';
	}

	return res;
}

bool sendPostRequest(const std::string& headers, const std::string& body, bool quiet) {

	const std::string url = "https://api.openshock.app/1/shockers/control";
	const std::string baseurl = "api.openshock.app";
	const std::string endpoint = "/1/shockers/control";
	
	
	HINTERNET hInternet, hConnect, hRequest;
	DWORD bytesRead;

	hInternet = InternetOpen(L"WinINet Example", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
	if (hInternet == NULL) {
		if (!quiet) {
			printf("InternetOpen Error: %d\n", GetLastError());
		}
		return false;
	}

	hConnect = InternetConnect(hInternet, L"api.openshock.app", INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
	if (hConnect == NULL) {
		if (!quiet) {
			printf("InternetConnect Error: %d\n", GetLastError());
		}
		InternetCloseHandle(hInternet);
		return false;
	}

	hRequest = HttpOpenRequest(hConnect, L"POST", L"/1/shockers/control", NULL, NULL, NULL, INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE, 0);
	if (hRequest == NULL) {
		if (!quiet) {
			printf("HttpOpenRequest Error: %d\n", GetLastError());
		}
		InternetCloseHandle(hConnect);
		InternetCloseHandle(hInternet);
		return false;
	}
	
	bool res = HttpSendRequestA(hRequest, headers.c_str(), headers.size(), (LPVOID)body.c_str(), body.size());
	if (!res) {
		if (!quiet) {
			printf("HttpSendRequest Error: %d\n", GetLastError());
		}
		InternetCloseHandle(hConnect);
		InternetCloseHandle(hInternet);
		return false;
	}
	
	char buffer[4096];
	std::string responseData = "";

	while (InternetReadFile(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
		responseData.append(buffer, bytesRead);
	}

	DWORD dwStatusCode = 0;
	DWORD dwSize = sizeof(dwStatusCode);
	res = HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &dwStatusCode, &dwSize, NULL);
	if (!res || dwStatusCode != 200) {
		if (!quiet) {
			printf("Response Data: %s\n", responseData.c_str());
			printf("res: %d http: %d\n", res, dwStatusCode);
		}
		InternetCloseHandle(hRequest);
		InternetCloseHandle(hConnect);
		InternetCloseHandle(hInternet);
		return false;
	}

	InternetCloseHandle(hRequest);
	InternetCloseHandle(hConnect);
	InternetCloseHandle(hInternet);

	return true;
}

// -----

void Collar::setID(const char* id_) {
	strncpy_s(id, id_, 255);
}

void Collar::displayStatus() {
	printf("\tstatus: %s%s\n" RESET, online ? GREEN : RED, online ? "online" : "unknown");
	printf("\tID: %s\n", id[0] == '\0' ? "???" : id);
	printf("\tmin shock: %3d%s\n", (int)minShock, (int)minShock == 69 ? CYAN " :3" RESET : "");
	printf("\tmax shock: %3d%s\n", (int)maxShock, (int)maxShock == 69 ? CYAN " :3" RESET : "");
	printf("\tshock type: %s\n", getShockTypeName(shockType));
}

// -----

void CollarManager::setToken(const char* token_) {
	strncpy_s(token, token_, 255);
}

void CollarManager::displayModifiers(std::optional<PipePacket> packet) {
	
	// Terve!

	if (packet.has_value()) {
		PipePacket p = packet.value();
		printf(CLEARHORIZONTAL "Max Damage Value      : %4d %4d\n", (int)maxDamageVal, p.getStrength());
		printf(CLEARHORIZONTAL "Counter Hit Modifier  : %4.2f %s\n", counterHitMod, p.counterhit ? RED "TERVE!" RESET : "");
		printf(CLEARHORIZONTAL "Screen Shake Modifier : %4.2f %s\n", screenShakeMod, p.screenshake ? RED "TERVE!" RESET : "");
		printf(CLEARHORIZONTAL "Bounce Modifier       : %4.2f %s\n", bounceMod, p.bounce ? RED "TERVE!" RESET : "");
		printf(CLEARHORIZONTAL "Reduce Fail Modifier  : %4.2f %s\n", reduceFailMod, p.reduceFail ? RED "TERVE!" RESET : "");
	} else {
		printf(CLEARHORIZONTAL "Max Damage Value      : %4d\n", (int)maxDamageVal);
		printf(CLEARHORIZONTAL "Counter Hit Modifier  : %4.2f\n", counterHitMod);
		printf(CLEARHORIZONTAL "Screen Shake Modifier : %4.2f\n", screenShakeMod);
		printf(CLEARHORIZONTAL "Bounce Modifier       : %4.2f\n", bounceMod);
		printf(CLEARHORIZONTAL "Reduce Fail Modifier  : %4.2f\n", reduceFailMod);
	}
	
	printf("\n");
}

void CollarManager::displayStatus() {

	printf("CollarManager Status:\n");
	
	printf("\tToken: %s\n\n", token[0] == '\0' ? "???" : censorString(token).c_str());

	printf("Collar 1:\n");
	collar1.displayStatus();
	printf("\n");
	
	printf("Collar 2:\n");
	collar2.displayStatus();
	printf("\n");

	displayModifiers();
	
}

void CollarManager::readSettings(int depth) {

	if (depth >= 2) { // safeguard for recursive loop if cannot make the file
		return;
	}

	std::ifstream inFile("shockSettings.txt");

	if (!inFile) {
		
		printf("couldnt find file, making an example one\n");

		inFile.close();

		std::ofstream outFile("shockSettings.txt");

		std::string exampleFile =
			"# your openshock api token\n"
			"token : put it here please :)\n"
			"\n"
			"# P1 Shocker ID\n"
			"p1ID : put it here please :)\n"
			"p1MinShock : 10 # min shock. shock values are 0-100\n"
			"p1MaxShock : 20 # max shock\n"
			"p1Type: Shock # can be either Shock, Sound, or Vibrate\n"
			"\n"
			"# P2 Shocker ID\n"
			"p2ID : put it here please :)\n"
			"p2MinShock : 10 # min shock\n"
			"p2MaxShock : 20 # max shock\n"
			"p2Type: Shock # can be either Shock, Sound, or Vibrate\n"
			"\n"
			"# misc settings\n"
			"maxDamageVal : 3000 # this damage value will give maxShock.\n"
			"\n"
			"# these values should be decimals in the range of[0.0, 0.5)\n"
			"# they will increase the shock strength by moving the minShockValue up that amount.\n"
			"# heres a formula for you, inputs range from damage = [0.0, maxDamageVal)\n"
			"# output range is[0.0, 1.0)\n"
			"\n"
			"# Shock(damage) = (1 - val)(damage / maxDamageVal) + val\n"
			"# where val is the MAX of any triggered values\n"
			"\n"
			"counterHit : 0.1\n"
			"screenShake : 0.1\n"
			"bounce : 0.1\n"
			"reduceFail : 0.1\n"
			;

		outFile << exampleFile;

		outFile.close();

		readSettings(depth + 1);

		return;
	}

	std::regex reg("(.*?):(.*)");
	std::smatch matches;

	std::string line;
	while (std::getline(inFile, line)) {
		
		size_t pos = line.find('#');

		if (pos != std::string::npos) {
			line.erase(pos);
		}

		if (std::regex_match(line, matches, reg)) {
			std::string key = strip(matches[1].str());
			std::string val = strip(matches[2].str());

			lowerString(key);

			if (key == "token") {
				setToken(val.c_str());
				continue;
			}

			if (key.size() <= 2) {
				continue;
			}

			int playerIndex = -1;

			if (key[1] == '1') {
				playerIndex = 0;
			} else if (key[1] == '2') {
				playerIndex = 1;
			} else {

				if (key == "counterhit") {
					counterHitMod = CLAMP(safeStof(val.c_str()), 0.0f, 1.0f);
				} else if (key == "screenshake") {
					screenShakeMod = CLAMP(safeStof(val.c_str()), 0.0f, 1.0f);
				} else if (key == "bounce") {
					bounceMod = CLAMP(safeStof(val.c_str()), 0.0f, 1.0f);
				} else if (key == "maxdamageval") {
					maxDamageVal = MAX(safeStof(val.c_str()), 0.0f);
				} else if (key == "reducefail") {
					reduceFailMod = CLAMP(safeStof(val.c_str()), 0.0f, 1.0f);
				}

				continue;
			}

			key = key.substr(2);

			if (key == "id") {
				collars[playerIndex].setID(val.c_str());
			} else if (key == "minshock") {
				collars[playerIndex].minShock = safeStoi(val.c_str());
				collars[playerIndex].minShock = CLAMP(collars[playerIndex].minShock, 0, 100);
			} else if (key == "maxshock") {
				collars[playerIndex].maxShock = safeStoi(val.c_str());
				collars[playerIndex].maxShock = CLAMP(collars[playerIndex].maxShock, 0, 100);
			} else if (key == "type") {
				
				lowerString(val);

				if (val == "shock") {
					collars[playerIndex].shockType = ShockType::Shock;
				} else if (val == "sound") {
					collars[playerIndex].shockType = ShockType::Sound;
				} else if (val == "vibrate") {
					collars[playerIndex].shockType = ShockType::Vibrate;
				} else {
					collars[playerIndex].shockType = ShockType::Shock;
				}

			}
		}
	}

	inFile.close();

	for (int i = 0; i < 2; i++) {

		int min = collars[i].minShock;
		int max = collars[i].maxShock;

		collars[i].minShock = MIN(min, max);
		collars[i].maxShock = MAX(min, max);

		ShockType backup = collars[i].shockType;
		collars[i].shockType = ShockType::Stop;

		collars[i].online = sendShock(i, 0, 0, true);

		collars[i].shockType = backup;
	}

	

}

bool CollarManager::sendShock(int player, int strength, int duration, bool quiet) {
	
	// strength is a float from 0-1 which will be converted into the range of minstrength - maxstrength
	// ill do that in melty, or somewhere else

	// duration is a min of 300
	// i should look at xrds code

	strength = CLAMP(strength, collars[player].minShock, collars[player].maxShock);
	duration = CLAMP(duration, 300, 1000);

	std::string body = R"([
	{
		"id": ")" + std::string(collars[player].id) + R"(",
		"type": ")" + std::string(getShockTypeName(collars[player].shockType)) + R"(",
		"intensity": )" + std::to_string(10) + R"(,
		"duration": )" + std::to_string(300) + R"(,
		"exclusive": true
	}
])";

	std::string headers = "accept: application/json\r\nOpenShockToken: " + std::string(token) + "\r\nContent-Type: application/json\r\n";

	bool res = sendPostRequest(headers, body, quiet);

	if (!res) {

		if (!quiet) {
			printf("\n\n" RED);

			printf("body: %s\n\n", body.c_str());

			printf("headers: %s\n\n", headers.c_str());

			printf(RESET);
		}

	}

	return res;
}

void CollarManager::sendShock(PipePacket packet, int shockDisplayCount) {

	if (packet.errorBit) {
		printf("data: %08X %d                                       \n", packet.error, packet.error);
		return;
	}

	int player = packet.player;
	float strength = packet.getStrength();
	float duration = 300;

	strength = (CLAMP(strength / maxDamageVal, 0.0f, 1.0f));

	float modVal = 0.0f;
	
	modVal = MAX(modVal, packet.counterhit  ? counterHitMod  : 0.0f);
	modVal = MAX(modVal, packet.screenshake ? screenShakeMod : 0.0f);
	modVal = MAX(modVal, packet.bounce      ? bounceMod      : 0.0f);
	modVal = MAX(modVal, packet.reduceFail  ? reduceFailMod  : 0.0f);

	modVal = CLAMP(modVal, 0.0f, 1.0f);

	strength = (1.0f - modVal) * strength + modVal;

	strength = (collars[player].maxShock - collars[player].minShock) * strength + collars[player].minShock;

	

	printf("\x1b[6A");
	displayModifiers(packet);
	//printf(CLEARHORIZONTAL "P%d S:%3d D:%3d M:%4.2f\r", player + 1, (int)strength, (int)duration, modVal);

	const char* const arrowColors[3] = { RED, DARKRED, MAGENTA };
	const char arrowSymbols[3] = { '-', '=', '~' };
	int shockModulo = shockDisplayCount % 3;

	printf(CLEARHORIZONTAL "%s%c>" RESET " %s collar %d with a strength of %d for %dms\r" RESET, arrowColors[shockModulo], arrowSymbols[shockModulo], getShockTypeVerb(collars[player].shockType), player + 1, (int)strength, (int)duration);

	
	sendShock(player, strength, duration, true);
}

// -----

Pipe::~Pipe() {
	if (serverHandle != NULL) {
		CloseHandle(serverHandle);
	}

	if (clientHandle != NULL) {
		CloseHandle(clientHandle);
	}

}

void Pipe::initServer() {
	
	serverHandle = CreateNamedPipeA(
		Pipe::pipeName,
		PIPE_ACCESS_INBOUND,
		PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT,
		1, // one max client
		0, // out buffer size. i dont have an out buffer, so,, 0?
		sizeof(PipePacket) * 1024, // rn im saying my messages are 4 bytes each. i havent actually decided on the message format. could probs do one byte. if this fills up, melty will probs block, which is super bad
		0,
		NULL
	);

	if (serverHandle == INVALID_HANDLE_VALUE) {
		printf(RED "failed to create server pipe" RESET);
		serverHandle = NULL;
		return;
	}
}

void Pipe::initClient() {

	clientHandle = CreateFileA(
		Pipe::pipeName,
		GENERIC_WRITE,           
		0,                       
		NULL,                    
		OPEN_EXISTING,           
		0,                       
		NULL                     
	);

	if (clientHandle == INVALID_HANDLE_VALUE) {
		printf(RED "failed to create client pipe" RESET);
		clientHandle = NULL;
	}

}

bool Pipe::peek() {
	DWORD bytesAvailable = 0;
	bool res;
	
	res = PeekNamedPipe(serverHandle, NULL, 0, NULL, &bytesAvailable, NULL);
	if (!res) {
		DWORD err = GetLastError();

		if (err == ERROR_BROKEN_PIPE) {
			DisconnectNamedPipe(serverHandle);
			ConnectNamedPipe(serverHandle, NULL);
		} else if (err == ERROR_BAD_PIPE) {

		} else {
			printf(RED "unknown err %d\n" RESET, err);
		}

		return false;
	}

	return bytesAvailable != 0;
}

std::optional<PipePacket> Pipe::pop() {

	if (!peek()) {
		return std::optional<PipePacket>();
	}

	PipePacket res;
	
	DWORD bytesRead = 0;
	bool status = ReadFile(serverHandle, &res, sizeof(PipePacket), &bytesRead, NULL);
	if (!status || bytesRead == 0) {

		DWORD err = GetLastError();

		if (err == ERROR_BROKEN_PIPE) {
			DisconnectNamedPipe(serverHandle);
			ConnectNamedPipe(serverHandle, NULL);
		} else if (err == ERROR_BAD_PIPE) {

		} else {
			printf(RED "unknown err %d\n" RESET, err);
		}

		
		return std::optional<PipePacket>();
	}

	return res;
}

void Pipe::push(PipePacket data) {
	
	DWORD bytesWritten = 0;
	
	if (!WriteFile(clientHandle, &data, sizeof(PipePacket), &bytesWritten, NULL)) {
		printf(RED "failed to write to pipe err: %d\n" RESET, GetLastError());
	}

}

