#include "GameTextRenderer.h"
#include "Config.h"
#include "Diagnostics.h"
#include "Hooks.h"
#include "W2App.h"
#include <cstring>

void GameTextRenderer::install() {
	constructTextbox = (BitmapTextbox *(__fastcall *)(DWORD, int, BitmapTextbox *, int, int))
		_ScanPattern("ConstructTextbox", "\x6A\xFF\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x64\x89\x25\x00\x00\x00\x00\x51\x8B\x44\x24\x1C\x53\x8B\x5C\x24\x1C\x55\x8B\x6C\x24\x1C\x56\x57\x8B\xF1\x89\x45\x08\x8D\x7D\x04\x57\x8D\x4C\x24\x30\x51", "???????xx????xxxx????xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

	setTextboxText = (void *(__stdcall *)(BitmapTextbox *, char *, int, int, int, int *, int *, int))
		_ScanPattern("SetTextboxText", "\x6A\xFF\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x8B\x44\x24\x18\x64\x89\x25\x00\x00\x00\x00\x83\xEC\x10\x53\x8B\x5C\x24\x24\x3B\x83\x00\x00\x00\x00\x55", "???????xx????xxxxxxxx????xxxxxxxxxx????x");

	addrDrawBitmap2D = _ScanPattern("DrawBitmap2D", "\x8B\x81\x00\x00\x00\x00\x3D\x00\x00\x00\x00\x56\x8B\x74\x24\x0C\x7D\x79\x57\x8B\x39\x83\xC7\xD8\x78\x70\x89\x39\x8D\x7C\x0F\x04\x89\xBC\x81\x00\x00\x00\x00\x8B\xB9\x00\x00\x00\x00\x8B\x84\xB9\x00\x00\x00\x00\x83\xC7\x01\x85\xC0\x89\xB9\x00\x00\x00\x00\x74\x49", "??????x????xxxxxxxxxxxxxxxxxxxxxxxx????xx????xxx????xxxxxxx????x");
	addrDrawTextbox3D = _ScanPattern("DrawTextbox3D", "\x8B\x81\x00\x00\x00\x00\x3D\x00\x00\x00\x00\x56\x8B\x74\x24\x0C\x7D\x7D\x53\x57\x8B\x39\x83\xC7\xCC\x33\xDB\x3B\xFB\x7C\x6E\x89\x39\x8D\x7C\x0F\x04\x89\xBC\x81\x00\x00\x00\x00\x8B\xB9\x00\x00\x00\x00\x8B\x84\xB9\x00\x00\x00\x00\x83\xC7\x01\x3B\xC3\x89\xB9\x00\x00\x00\x00\x74\x47\x8B\x4C\x24\x10", "??????x????xxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xx????xxx????xxxxxxx????xxxxxx");

	Diagnostics::log("text renderer installed: construct=0x%X set=0x%X drawBitmap2D=0x%X drawTextbox3D=0x%X",
		(DWORD)constructTextbox, (DWORD)setTextboxText, addrDrawBitmap2D, addrDrawTextbox3D);
}

void GameTextRenderer::reset() {
	if (!textboxes.empty()) {
		Diagnostics::log("text renderer reset: boxes=%u ddDisplay=0x%X", static_cast<unsigned int>(textboxes.size()), textboxesDdDisplay);
	}
	textboxes.clear();
	textboxesDdDisplay = 0;
}

GameTextRenderer::TextStyle GameTextRenderer::getDefaultStyle() {
	return {
		Config::getTextColor(),
		Config::getTextBackground(),
		Config::getTextFrame(),
		Config::getTextOpacity()
	};
}

GameTextRenderer::BitmapTextbox *GameTextRenderer::ensureTextbox(size_t slot) {
	DWORD ddDisplay = W2App::getAddrDdDisplay();
	if (!constructTextbox || !ddDisplay) {
		return nullptr;
	}

	if (textboxesDdDisplay && textboxesDdDisplay != ddDisplay) {
		reset();
	}

	while (textboxes.size() <= slot) {
		auto textbox = std::make_unique<BitmapTextbox>();
		std::memset(textbox.get(), 0, sizeof(BitmapTextbox));
		constructTextbox(ddDisplay, 0, textbox.get(), 32, Config::getTextFont());
		textboxesDdDisplay = ddDisplay;
		if (textboxes.empty()) {
			Diagnostics::log("first textbox constructed: box=0x%X ddDisplay=0x%X", (DWORD)textbox.get(), ddDisplay);
		}
		textboxes.push_back(std::move(textbox));
	}

	return textboxes[slot].get();
}

void *GameTextRenderer::buildTextBitmap(size_t slot, const std::string &text, int *width, int *height) {
	return buildTextBitmap(slot, text, width, height, getDefaultStyle());
}

void *GameTextRenderer::buildTextBitmap(size_t slot, const std::string &text, int *width, int *height, const TextStyle &style) {
	if (text.empty() || !setTextboxText) {
		return nullptr;
	}

	auto *textbox = ensureTextbox(slot);
	if (!textbox) {
		return nullptr;
	}

	auto mutableText = text;
	return setTextboxText(textbox, mutableText.data(), style.textColor, style.backgroundColor, style.frameColor, width, height, style.opacity);
}

bool GameTextRenderer::measureText(size_t slot, const std::string &text, int *width, int *height) {
	return measureText(slot, text, width, height, getDefaultStyle());
}

bool GameTextRenderer::measureText(size_t slot, const std::string &text, int *width, int *height, const TextStyle &style) {
	int measuredWidth = 0;
	int measuredHeight = 0;
	void *bitmap = buildTextBitmap(slot, text, &measuredWidth, &measuredHeight, style);
	if (width) {
		*width = measuredWidth;
	}
	if (height) {
		*height = measuredHeight;
	}
	return bitmap && measuredWidth > 0 && measuredHeight > 0;
}

bool GameTextRenderer::drawText3D(DWORD gameScene, size_t slot, int fixedX, int fixedY, const std::string &text) {
	if (!gameScene || text.empty() || !setTextboxText || !addrDrawTextbox3D) {
		return false;
	}

	auto *textbox = ensureTextbox(slot);
	if (!textbox) {
		return false;
	}

	int width = 0;
	int height = 0;
	auto mutableText = text;
	TextStyle style = getDefaultStyle();
	void *bitmap = setTextboxText(textbox, mutableText.data(), style.textColor, style.backgroundColor, style.frameColor, &width, &height, style.opacity);
	if (!bitmap || width <= 0 || height <= 0) {
		return false;
	}

	return callDrawBitmap3D(gameScene, fixedX, fixedY, bitmap, width, height) != 0;
}

bool GameTextRenderer::drawText3DCentered(DWORD gameScene, size_t slot, int fixedX, int fixedY, const std::string &text) {
	if (!gameScene || text.empty() || !setTextboxText || !addrDrawTextbox3D) {
		return false;
	}

	auto *textbox = ensureTextbox(slot);
	if (!textbox) {
		return false;
	}

	int width = 0;
	int height = 0;
	auto mutableText = text;
	TextStyle style = getDefaultStyle();
	void *bitmap = setTextboxText(textbox, mutableText.data(), style.textColor, style.backgroundColor, style.frameColor, &width, &height, style.opacity);
	if (!bitmap || width <= 0 || height <= 0) {
		return false;
	}

	return callDrawBitmap3D(gameScene, fixedX - (width * 0x10000) / 2, fixedY, bitmap, width, height) != 0;
}

bool GameTextRenderer::drawText2D(DWORD gameScene, size_t slot, int fixedX, int fixedY, const std::string &text) {
	return drawText2D(gameScene, slot, fixedX, fixedY, text, getDefaultStyle());
}

bool GameTextRenderer::drawText2D(DWORD gameScene, size_t slot, int fixedX, int fixedY, const std::string &text, const TextStyle &style) {
	if (!gameScene || !addrDrawBitmap2D) {
		return false;
	}

	int width = 0;
	int height = 0;
	void *bitmap = buildTextBitmap(slot, text, &width, &height, style);
	if (!bitmap || width <= 0 || height <= 0) {
		return false;
	}

	return callDrawBitmap2D(gameScene, fixedX, fixedY, bitmap, width, height) != 0;
}

int GameTextRenderer::callDrawBitmap2D(DWORD gameScene, int fixedX, int fixedY, void *bitmap, int width, int height) {
	int retv = 0;
	_asm {
		mov ecx, gameScene
		mov edx, fixedY
		push 0
		push height
		push width
		push 0
		push bitmap
		push fixedX
		push 10
		call addrDrawBitmap2D
		mov retv, eax
	}
	return retv;
}

int GameTextRenderer::callDrawBitmap3D(DWORD gameScene, int fixedX, int fixedY, void *bitmap, int width, int height) {
	int retv = 0;
	int sortKey = (fixedY >> 15) + 0x0D0200;
	_asm {
		mov ecx, gameScene
		mov edx, fixedY
		push 0
		push height
		push width
		push bitmap
		push fixedX
		push sortKey
		call addrDrawTextbox3D
		mov retv, eax
	}
	return retv;
}
