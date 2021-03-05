#pragma once

#include <vector>
#include <string_view>

namespace Lemon{
	namespace Filesystem{
		class ExecutableTarget{
		public:
			ExecutableTarget(const char* path);
			ExecutableTarget(const std::string& path);

			const std::string_view& Executable();
			const std::vector<std::string_view>& Arguments();

		private:
			std::string_view executable;
			std::vector<std::string_view> arguments;
		};
	}
}