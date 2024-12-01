#include <winsock2.h>

class WSInit {
public:
	WSInit() {
		WSADATA wsa;

		if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
			
		}
	}
	~WSInit() {
		::WSACleanup();
	}
};

static WSInit i;