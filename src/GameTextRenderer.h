#ifndef WKMINE_TIMER_GAMETEXTRENDERER_H
#define WKMINE_TIMER_GAMETEXTRENDERER_H

#include <string>
#include <vector>
#include <memory>

typedef unsigned long DWORD;

class GameTextRenderer {
public:
	struct TextStyle {
		int textColor = 0;
		int backgroundColor = 0;
		int frameColor = 0;
		int opacity = 0;
	};

	class BitmapTextbox {
	public:
		DWORD width_dword0;
		DWORD height_dword4;
		DWORD fontid_dword8;
		DWORD bitmap_dwordC;
		DWORD dword10;
		DWORD dword14;
		DWORD dword18;
		DWORD message_dword1C;
		DWORD dword20;
		DWORD dword24;
		DWORD dword28;
		DWORD dword2C;
		DWORD dword30;
		DWORD dword34;
		DWORD dword38;
		DWORD dword3C;
		DWORD dword40;
		DWORD dword44;
		DWORD dword48;
		DWORD dword4C;
		DWORD dword50;
		DWORD dword54;
		DWORD dword58;
		DWORD dword5C;
		DWORD dword60;
		DWORD dword64;
		DWORD dword68;
		DWORD dword6C;
		DWORD dword70;
		DWORD dword74;
		DWORD dword78;
		DWORD dword7C;
		DWORD dword80;
		DWORD dword84;
		DWORD dword88;
		DWORD dword8C;
		DWORD dword90;
		DWORD dword94;
		DWORD dword98;
		DWORD dword9C;
		DWORD dwordA0;
		DWORD dwordA4;
		DWORD dwordA8;
		DWORD dwordAC;
		DWORD dwordB0;
		DWORD dwordB4;
		DWORD dwordB8;
		DWORD dwordBC;
		DWORD dwordC0;
		DWORD dwordC4;
		DWORD dwordC8;
		DWORD dwordCC;
		DWORD dwordD0;
		DWORD dwordD4;
		DWORD dwordD8;
		DWORD dwordDC;
		DWORD dwordE0;
		DWORD dwordE4;
		DWORD dwordE8;
		DWORD dwordEC;
		DWORD dwordF0;
		DWORD dwordF4;
		DWORD dwordF8;
		DWORD dwordFC;
		DWORD dword100;
		DWORD dword104;
		DWORD dword108;
		DWORD dword10C;
		DWORD dword110;
		DWORD dword114;
		DWORD dword118;
		DWORD textcolor_dword11C;
		DWORD width_dword120;
		DWORD height_dword124;
		DWORD color1_dword128;
		DWORD color2_dword12C;
		DWORD opacity_dword130;
		DWORD dword134;
		DWORD dword138;
		DWORD dword13C;
		DWORD dword140;
		DWORD dword144;
		DWORD dword148;
		DWORD dword14C;
		DWORD dword150;
		DWORD dword154;
	};

	static void install();
	static void reset();
	static TextStyle getDefaultStyle();
	static bool measureText(size_t slot, const std::string &text, int *width, int *height);
	static bool measureText(size_t slot, const std::string &text, int *width, int *height, const TextStyle &style);
	static bool drawText3D(DWORD gameScene, size_t slot, int fixedX, int fixedY, const std::string &text);
	static bool drawText3DCentered(DWORD gameScene, size_t slot, int fixedX, int fixedY, const std::string &text);
	static bool drawText2D(DWORD gameScene, size_t slot, int fixedX, int fixedY, const std::string &text);
	static bool drawText2D(DWORD gameScene, size_t slot, int fixedX, int fixedY, const std::string &text, const TextStyle &style);

private:
	static BitmapTextbox *ensureTextbox(size_t slot);
	static void *buildTextBitmap(size_t slot, const std::string &text, int *width, int *height);
	static void *buildTextBitmap(size_t slot, const std::string &text, int *width, int *height, const TextStyle &style);
	static int callDrawBitmap2D(DWORD gameScene, int fixedX, int fixedY, void *bitmap, int width, int height);
	static int callDrawBitmap3D(DWORD gameScene, int fixedX, int fixedY, void *bitmap, int width, int height);

	static inline BitmapTextbox *(__fastcall *constructTextbox)(DWORD DDDisplay, int EDX, BitmapTextbox *Dst, int length, int fontid) = nullptr;
	static inline void *(__stdcall *setTextboxText)(BitmapTextbox *box, char *text, int textColor, int color1, int color2, int *width, int *height, int opacity) = nullptr;
	static inline DWORD addrDrawBitmap2D = 0;
	static inline DWORD addrDrawTextbox3D = 0;
	static inline std::vector<std::unique_ptr<BitmapTextbox>> textboxes;
	static inline DWORD textboxesDdDisplay = 0;
};

#endif // WKMINE_TIMER_GAMETEXTRENDERER_H
