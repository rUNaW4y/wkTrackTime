#include <windows.h>
#include <chrono>
#include <ctime>
#include <stdexcept>
#include "Config.h"
#include "Chat.h"
#include "Diagnostics.h"
#include "Hooks.h"
#include "W2App.h"
#include "entities/CTaskTeam.h"

void install() {
	srand(time(0) * GetCurrentProcessId());
	CTaskTeam::install();
	W2App::install();
	Chat::install();
}

bool LockGlobalInstance(LPCTSTR MutexName) {
	if (!CreateMutex(NULL, 0, MutexName)) return 0;
	if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;
	return 1;
}

char LocalMutexName[MAX_PATH];
bool LockCurrentInstance(LPCTSTR MutexName) {
	_snprintf_s(LocalMutexName, _TRUNCATE, "P%u/%s", GetCurrentProcessId(), MutexName);
	return LockGlobalInstance(LocalMutexName);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
	switch (ul_reason_for_call) {
		case DLL_PROCESS_ATTACH: {
			auto start = std::chrono::high_resolution_clock::now();
			decltype(start) finish;
			try {
				Config::readConfig();
				Diagnostics::initialize(Config::getWaDir());
				Diagnostics::log("process attach; moduleEnabled=%d", Config::isModuleEnabled() ? 1 : 0);
				if (Config::isModuleEnabled() && Config::waVersionCheck() && LockCurrentInstance("wkTrackTime")) {
					Hooks::loadOffsets();
					Diagnostics::log("install start");
					install();
					Hooks::saveOffsets();
					Diagnostics::log("install done");
				}
				finish = std::chrono::high_resolution_clock::now();
			} catch (std::exception &e) {
				finish = std::chrono::high_resolution_clock::now();
				Diagnostics::log("exception: %s", e.what());
				MessageBoxA(0, e.what(), Config::getFullStr().c_str(), MB_ICONERROR);
			}
			std::chrono::duration<double> elapsed = finish - start;
			printf("wkTrackTime startup took %lf seconds\n", elapsed.count());
		}
		break;
		case DLL_PROCESS_DETACH:
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		default:
			break;
	}
	return TRUE;
}
