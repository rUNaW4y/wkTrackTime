#ifndef WKTRACKTIME_CHAT_H
#define WKTRACKTIME_CHAT_H

#include <string>

class Chat {
private:
	static void __stdcall hookOnChatInput(int a3);
	static int __stdcall callOriginalOnChatInput(int a1, char *msg, int a3);

public:
	static int onChatInput(int a1, char *msg, int a3);
	static void callShowChatMessage(const std::string &msg, int color);
	static void install();
};

#endif // WKTRACKTIME_CHAT_H
