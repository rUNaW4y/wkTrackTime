#include "CTaskTeam.h"
#include "../Hooks.h"
#include "../TrackTimeOverlay.h"

namespace {
	void *(__stdcall *origSetTextboxText)(void *box, char *text, int text_color, int color1, int color2, int *width, int *height, int opacity) = nullptr;
	DWORD addrDrawTeamBarPatchRet = 0;

	void * __stdcall hookCaptureTeamBarStyle(TeamBarStruct *teamBar, void *box, char *text, int textColor, int color1, int color2, int *width, int *height, int opacity) {
		if (teamBar) {
			TrackTimeOverlay::observeTeamStyle(teamBar->team_number_dword4, teamBar->highlight_dword1C, textColor, color1, color2);
		}

		return origSetTextboxText ? origSetTextboxText(box, text, textColor, color1, color2, width, height, opacity) : nullptr;
	}

	void __declspec(naked) hookDrawTeamBarPatch() {
		_asm {
			push 0x10000
			lea esi,dword ptr ss:[esp+0x1C]
			push esi
			lea esi,dword ptr ss:[esp+0x24]
			push esi
			mov esi,dword ptr ds:[ecx+0x7324]
			mov ecx,dword ptr ds:[ecx+0x7328]
			push esi
			push ecx
			push edx
			mov edx,dword ptr ds:[edi+0x10]
			lea eax,dword ptr ss:[ebp+0x4114]
			push eax
			push edx
			push edi
			call hookCaptureTeamBarStyle
			jmp addrDrawTeamBarPatchRet
		}
	}
}

void CTaskTeam::install() {
	DWORD addrSetTextboxText = _ScanPattern("SetTextboxText", "\x6A\xFF\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x8B\x44\x24\x18\x64\x89\x25\x00\x00\x00\x00\x83\xEC\x10\x53\x8B\x5C\x24\x24\x3B\x83\x00\x00\x00\x00\x55", "???????xx????xxxxxxxx????xxxxxxxxxx????x");
	origSetTextboxText = reinterpret_cast<void *(__stdcall *)(void *, char *, int, int, int, int *, int *, int)>(addrSetTextboxText);

	DWORD addrDrawTeamBar = _ScanPattern("DrawTeamBar", "\x83\xEC\x14\x53\x55\x56\x57\x8B\xF9\x8B\x47\x20\x8B\x77\x04\x8B\x4F\x0C\x99\x2B\xC2\xD1\xF8\xC1\xE0\x10\x01\x44\x24\x2C\x8B\xC6\x69\xC0\x00\x00\x00\x00", "??????xxxxxxxxxxxxxxxxxxxxxxxxxxxx????");
	DWORD addrDrawTeamBarPatch = addrDrawTeamBar + 0x9C;
	addrDrawTeamBarPatchRet = addrDrawTeamBar + 0xCA;
	_HookAsm(addrDrawTeamBarPatch, reinterpret_cast<DWORD>(&hookDrawTeamBarPatch));
}
