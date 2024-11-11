
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

// -----

void Collar::setID(const char* id_) {
	strncpy_s(id, id_, 255);
}

void Collar::sendShock(int shockValue) {

	shockValue = CLAMP(shockValue, minShock, maxShock);

	HINTERNET hInternet, hConnect;
	DWORD bytesRead;
	char buffer[1024];

	hInternet = InternetOpen(L"HTTP Example", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
	if (hInternet == NULL) {
		std::cerr << "InternetOpen failed\n";
		return;
	}

	hConnect = InternetOpenUrlA(hInternet, "http://example.com", NULL, 0, INTERNET_FLAG_RELOAD, 0);
	if (hConnect == NULL) {
		std::cerr << "InternetOpenUrlA failed\n";
		InternetCloseHandle(hInternet);
		return;
	}

	while (InternetReadFile(hConnect, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
		std::cout.write(buffer, bytesRead);
	}

	InternetCloseHandle(hConnect);
	InternetCloseHandle(hInternet);


}

void Collar::displayStatus() {
	printf("\tID: %s\n", id[0] == '\0' ? "???" : id);
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
	
	printf("Token: %s\n", token[0] == '\0' ? "???" : token);

	printf("P1 Collar:\n");
	collar1.displayStatus();

	printf("P2 Collar:\n");
	collar2.displayStatus();
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

