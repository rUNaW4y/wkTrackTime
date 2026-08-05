#include "Chat.h"
#include "Config.h"
#include "TrackTimeOverlay.h"
#include "Diagnostics.h"
#include "Hooks.h"
#include "Utils.h"
#include "W2App.h"
#include <algorithm>
#include <string>
#include <vector>

namespace {
	DWORD origOnChatInput = 0;
	void (__stdcall *addrShowChatMessage)(DWORD addrResourceObject, int color, char *msg, int unk) = nullptr;

	constexpr int ChatColorNotice = 6;

	bool handleTrackTimeCommand(const std::vector<std::string> &parts) {
		if (parts.empty()) {
			return false;
		}

		const std::string &command = parts[0];
		bool enableSplitCommand = command == "enable" && parts.size() >= 2 && parts[1] == "tracktime";
		bool disableSplitCommand = command == "disable" && parts.size() >= 2 && parts[1] == "tracktime";

		if (command == "enabletracktime" || enableSplitCommand) {
			if (!Config::isRuntimeModuleEnabled()) {
				Config::setRuntimeModuleEnabled(true);
				TrackTimeOverlay::reset();
				Diagnostics::log("chat command: enable tracktime");
				Chat::callShowChatMessage("wkTrackTime enabled", ChatColorNotice);
			} else {
				Chat::callShowChatMessage("wkTrackTime already enabled", ChatColorNotice);
			}
			return true;
		}

		if (command == "disabletracktime" || disableSplitCommand) {
			if (Config::isRuntimeModuleEnabled()) {
				Config::setRuntimeModuleEnabled(false);
				TrackTimeOverlay::reset();
				Diagnostics::log("chat command: disable tracktime");
				Chat::callShowChatMessage("wkTrackTime disabled", ChatColorNotice);
			} else {
				Chat::callShowChatMessage("wkTrackTime already disabled", ChatColorNotice);
			}
			return true;
		}

		if (command == "resettracktime") {
			TrackTimeOverlay::reset();
			Diagnostics::log("chat command: resettracktime");
			Chat::callShowChatMessage("wkTrackTime data reset", ChatColorNotice);
			return true;
		}

		return false;
	}
}

int __stdcall Chat::callOriginalOnChatInput(int a1, char *msg, int a3) {
	_asm mov ecx, a1
	_asm mov eax, msg
	_asm push a3
	_asm call origOnChatInput
}

int Chat::onChatInput(int a1, char *msg, int a3) {
	(void)a1;
	(void)a3;

	if (!msg) {
		return 0;
	}

	std::string message(msg);
	if (message.length() <= 1 || !message.starts_with("/")) {
		return 0;
	}

	std::transform(message.begin(), message.end(), message.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	message.erase(0, 1);

	std::vector<std::string> parts;
	Utils::tokenize(message, " ", parts);
	if (parts.empty()) {
		return 0;
	}

	return handleTrackTimeCommand(parts) ? 1 : 0;
}

#pragma optimize("", off)
char *onchat_eax;
int onchat_ecx;
void __stdcall Chat::hookOnChatInput(int a3) {
	_asm mov onchat_eax, eax
	_asm mov onchat_ecx, ecx

	if (!onChatInput(onchat_ecx, onchat_eax, a3)) {
		callOriginalOnChatInput(onchat_ecx, onchat_eax, a3);
	}
}
#pragma optimize("", on)

void Chat::callShowChatMessage(const std::string &msg, int color) {
	DWORD ddGame = W2App::getAddrDdGame();
	if (!ddGame || !addrShowChatMessage) {
		return;
	}

	addrShowChatMessage(ddGame, color, const_cast<char *>(msg.c_str()), 1);
}

void Chat::install() {
	DWORD addrOnChatInput = _ScanPattern("OnChatInput", "\x81\xEC\x00\x00\x00\x00\x55\x56\x57\x8B\xF8\x8A\x07\x84\xC0\x8B\xF1\x0F\x84\x00\x00\x00\x00\x3C\x2F\x0F\x85\x00\x00\x00\x00\x8D\x44\x24\x40", "??????xxxxxxxxxxxxx????xxxx????xxxx");
	addrShowChatMessage = (void (__stdcall *)(DWORD, int, char *, int))
		_ScanPattern("ShowChatMessage", "\x81\xEC\x00\x00\x00\x00\x53\x55\x8B\xAC\x24\x00\x00\x00\x00\x80\xBD\x00\x00\x00\x00\x00\x8B\x85\x00\x00\x00\x00\x8B\x48\x24\x56\x8B\xB1\x00\x00\x00\x00\x57", "??????xxxxx????xx?????xx????xxxxxx????x");
	_HookDefault(OnChatInput);
	Diagnostics::log("chat installed: onChatInput=0x%X showChatMessage=0x%X", addrOnChatInput, reinterpret_cast<DWORD>(addrShowChatMessage));
}
