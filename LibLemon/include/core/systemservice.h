#pragma once

#include <core/msghandler.h>

namespace Lemon{
	static const char* systemServiceSocketAddress = "systemsvc";

	enum{
		LemonSystemLaunchUserSession,
	};

	enum{
		LemonSystemResponseUnknownUser,
		LemonSystemResponseInvalidPassword,
	};

	struct SessionCommand{
		unsigned short cmd;
		union {
			struct{
				int id;
				short passwordLength;
				char password[];
			};
		};
	};

	struct SessionResponse{
		unsigned short code;
	};
}