
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

bool sendPostRequest(const std::string& headers, const std::string& body) {

	const std::string url = "https://api.openshock.app/1/shockers/control";
	const std::string baseurl = "api.openshock.app";
	const std::string endpoint = "/1/shockers/control";
	
	
	HINTERNET hInternet, hConnect, hRequest;
	DWORD bytesRead;

	// Initialize WinINet
	hInternet = InternetOpen(L"WinINet Example", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
	if (hInternet == NULL) {
		printf("InternetOpen Error: %d\n", GetLastError());
		return false;
	}

	// Connect to the server
	hConnect = InternetConnect(hInternet, L"api.openshock.app", INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
	if (hConnect == NULL) {
		printf("InternetConnect Error: %d\n", GetLastError());
		InternetCloseHandle(hInternet);
		return false;
	}

	hRequest = HttpOpenRequest(hConnect, L"POST", L"/1/shockers/control", NULL, NULL, NULL, INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE, 0);
	if (hRequest == NULL) {
		printf("HttpOpenRequest Error: %d\n", GetLastError());
		InternetCloseHandle(hConnect);
		InternetCloseHandle(hInternet);
		return false;
	}
	
	bool res = HttpSendRequestA(hRequest, headers.c_str(), headers.size(), (LPVOID)body.c_str(), body.size());
	if (!res) {
		printf("HttpSendRequest Error: %d\n", GetLastError());
		InternetCloseHandle(hConnect);
		InternetCloseHandle(hInternet);
		return false;
	}

	
	char buffer[4096];
	std::string responseData = "";

	while (InternetReadFile(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
		responseData.append(buffer, bytesRead);
	}

	//std::cout << "Response Data: " << responseData << std::endl;
	
	DWORD dwStatusCode = 0;
	DWORD dwSize = sizeof(dwStatusCode);
	res = HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &dwStatusCode, &dwSize, NULL);
	if (!res || dwStatusCode != 200) {
		std::cout << "Response Data: " << responseData << std::endl;
		printf("res: %d http: %d\n", res, dwStatusCode);
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
	printf("\tID: %s\n", id[0] == '\0' ? "???" : censorString(id));
	printf("\tmin shock: %3d\n", minShock);
	printf("\tmax shock: %3d\n", maxShock);
	printf("\tshock type: %s\n", getShockTypeName(shockType));
}

// -----

void CollarManager::setToken(const char* token_) {
	strncpy_s(token, token_, 255);
}

void CollarManager::displayStatus() {

	printf("CollarManager Status:\n");
	
	printf("Token: %s\n\n", token[0] == '\0' ? "???" : censorString(token));

	printf("Collar 1:\n");
	collar1.displayStatus();
	printf("\n");
	
	printf("Collar 2:\n");
	collar2.displayStatus();
	printf("\n");
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
			"p1MinShock : 10 # min shock\n"
			"p1MaxShock : 20 # max shock\n"
			"p1Type: Shock # can be either Shock, Sound, or Vibrate\n"
			"\n"
			"# P2 Shocker ID\n"
			"p2ID : put it here please :)\n"
			"p2MinShock : 10 # min shock\n"
			"p2MaxShock : 20 # max shock\n"
			"p2Type: Shock # can be either Shock, Sound, or Vibrate\n"
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

}

bool CollarManager::sendShock(int player, int strength, int duration) {
	
	// strength is a float from 0-1 which will be converted into the range of minstrength - maxstrength
	// ill do that in melty, or somewhere else

	// duration is a min of 300
	// i should look at xrds code

	strength = CLAMP(strength, 0.0f, 1.0f);

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

	bool res = sendPostRequest(headers, body);


	if (!res) {

		printf("\n\n" RED);

		printf("body: %s\n\n", body.c_str());

		printf("headers: %s\n\n", headers.c_str());

		printf(RESET);

	}

	return res;
}