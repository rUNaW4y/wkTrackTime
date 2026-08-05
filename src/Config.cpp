#include "Config.h"
#include "Debugf.h"
#include <windows.h>
#include <cstdlib>
#include <format>

namespace fs = std::filesystem;

namespace {
	typedef unsigned long long QWORD;
#define MAKELONGLONG(lo,hi) ((LONGLONG(DWORD(lo) & 0xffffffff)) | LONGLONG(DWORD(hi) & 0xffffffff) << 32 )
#define QV(V1, V2, V3, V4) MAKEQWORD(V4, V3, V2, V1)
#define MAKEQWORD(LO2, HI2, LO1, HI1) MAKELONGLONG(MAKELONG(LO2,HI2),MAKELONG(LO1,HI1))

	int readProfileInt(const char *section, const char *key, int fallback, const std::string &path) {
		char buff[64] = {};
		GetPrivateProfileStringA(section, key, "", buff, sizeof(buff), path.c_str());
		if (buff[0] == '\0') {
			return fallback;
		}

		char *end = nullptr;
		long value = std::strtol(buff, &end, 0);
		if (end == buff) {
			return fallback;
		}
		return static_cast<int>(value);
	}

	QWORD GetModuleVersion(HMODULE hModule) {
		char WApath[MAX_PATH];
		DWORD h;
		GetModuleFileNameA(hModule, WApath, MAX_PATH);
		DWORD Size = GetFileVersionInfoSizeA(WApath, &h);
		if (Size) {
			void *Buf = malloc(Size);
			GetFileVersionInfoA(WApath, h, Size, Buf);
			VS_FIXEDFILEINFO *Info;
			DWORD Is;
			if (VerQueryValueA(Buf, "\\", (LPVOID*)&Info, (PUINT)&Is)) {
				if (Info->dwSignature == 0xFEEF04BD) {
					auto version = MAKELONGLONG(Info->dwFileVersionLS, Info->dwFileVersionMS);
					free(Buf);
					return version;
				}
			}
			free(Buf);
		}
		return 0;
	}
}

void Config::readConfig() {
	char wabuff[MAX_PATH];
	GetModuleFileNameA(0, (LPSTR)&wabuff, sizeof(wabuff));
	waDir = fs::path(wabuff).parent_path();
	auto inipath = (waDir / iniFile).string();

	moduleEnabled = readProfileInt("general", "EnableModule", 1, inipath) != 0;
	runtimeModuleEnabled = readProfileInt("general", "StartEnabled", moduleEnabled ? 1 : 0, inipath) != 0;
	useOffsetCache = readProfileInt("general", "UseOffsetCache", 1, inipath) != 0;
	ignoreVersionCheck = readProfileInt("general", "IgnoreVersionCheck", 0, inipath) != 0;
	overlayEnabled = readProfileInt("general", "EnableOverlay", 1, inipath) != 0;

	primaryTimerOffset = readProfileInt("general", "PrimaryTimerOffset", 0x188, inipath);
	secondaryTimerOffset = readProfileInt("general", "SecondaryTimerOffset", 0x18C, inipath);
	timerTicksPerSecond = readProfileInt("general", "TimerTicksPerSecond", 1000, inipath);
	overlayLeftPixels = readProfileInt("general", "OverlayLeftPixels", -150, inipath);
	overlayBottomPixels = readProfileInt("general", "OverlayBottomPixels", 72, inipath);
	overlayLineSpacingPixels = readProfileInt("general", "OverlayLineSpacingPixels", 2, inipath);
	overlayPlayerSpacingPixels = readProfileInt("general", "OverlayPlayerSpacingPixels", 4, inipath);
	turnEndGraceFrames = readProfileInt("general", "TurnEndGraceFrames", 3, inipath);

	textColor = readProfileInt("general", "TextColor", 6, inipath);
	textBackground = readProfileInt("general", "TextBackground", 21, inipath);
	textFrame = readProfileInt("general", "TextFrame", 54, inipath);
	textFont = readProfileInt("general", "TextFont", 1, inipath);
	textOpacity = readProfileInt("general", "TextOpacity", 0x10000, inipath);
}

bool Config::isModuleEnabled() {
	return moduleEnabled;
}

bool Config::isRuntimeModuleEnabled() {
	return runtimeModuleEnabled;
}

void Config::setRuntimeModuleEnabled(bool enabled) {
	runtimeModuleEnabled = enabled;
}

int Config::waVersionCheck() {
	if (ignoreVersionCheck) {
		return 1;
	}

	auto version = GetModuleVersion((HMODULE)0);
	char versionstr[64];
	_snprintf_s(versionstr, _TRUNCATE, "Detected game version: %u.%u.%u.%u", PWORD(&version)[3], PWORD(&version)[2], PWORD(&version)[1], PWORD(&version)[0]);
	debugf("%s\n", versionstr);

	std::string tversion = getFullStr();
	char buff[512];
	if (version < QV(3, 8, 0, 0)) {
		_snprintf_s(buff, _TRUNCATE, "wkTrackTime is not compatible with WA versions older than 3.8.0.0.\n\n%s", versionstr);
		MessageBoxA(0, buff, tversion.c_str(), MB_OK | MB_ICONERROR);
		return 0;
	}
	if (version >= QV(3, 9, 0, 0)) {
		_snprintf_s(buff, _TRUNCATE, "wkTrackTime is not compatible with WA versions 3.9.x.x and newer.\n\n%s", versionstr);
		MessageBoxA(0, buff, tversion.c_str(), MB_OK | MB_ICONERROR);
		return 0;
	}
	if (version == QV(3, 8, 0, 0) || version == QV(3, 8, 1, 0)) {
		return 1;
	}

	_snprintf_s(buff, _TRUNCATE, "wkTrackTime is not designed to work with your WA version and may malfunction.\n\nTo disable this warning set IgnoreVersionCheck=1 in wkTrackTime.ini.\n\n%s", versionstr);
	return MessageBoxA(0, buff, tversion.c_str(), MB_OKCANCEL | MB_ICONWARNING) == IDOK;
}

const std::filesystem::path &Config::getWaDir() {
	return waDir;
}

std::string Config::getVersionStr() {
	return "v0.1.0";
}

std::string Config::getBuildStr() {
	return __DATE__ " " __TIME__;
}

std::string Config::getModuleStr() {
	return moduleName;
}

std::string Config::getFullStr() {
	return getModuleStr() + " " + getVersionStr() + " (build: " + getBuildStr() + ")";
}

bool Config::isUseOffsetCache() {
	return useOffsetCache;
}

std::string Config::getWaVersionAsString() {
	char buff[32];
	auto version = GetModuleVersion(0);
	sprintf_s(buff, "%u.%u.%u.%u", PWORD(&version)[3], PWORD(&version)[2], PWORD(&version)[1], PWORD(&version)[0]);
	return buff;
}

const std::string &Config::getCacheFile() {
	return cacheFile;
}

bool Config::isOverlayEnabled() {
	return overlayEnabled;
}

int Config::getPrimaryTimerOffset() {
	return primaryTimerOffset;
}

int Config::getSecondaryTimerOffset() {
	return secondaryTimerOffset;
}

int Config::getTimerTicksPerSecond() {
	return timerTicksPerSecond;
}

int Config::getOverlayLeftPixels() {
	return overlayLeftPixels;
}

int Config::getOverlayBottomPixels() {
	return overlayBottomPixels;
}

int Config::getOverlayLineSpacingPixels() {
	return overlayLineSpacingPixels;
}

int Config::getOverlayPlayerSpacingPixels() {
	return overlayPlayerSpacingPixels;
}

int Config::getTurnEndGraceFrames() {
	return turnEndGraceFrames;
}

int Config::getTextColor() {
	return textColor;
}

int Config::getTextBackground() {
	return textBackground;
}

int Config::getTextFrame() {
	return textFrame;
}

int Config::getTextFont() {
	return textFont;
}

int Config::getTextOpacity() {
	return textOpacity;
}
