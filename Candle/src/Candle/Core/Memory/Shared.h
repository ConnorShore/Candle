#pragma once

#include <memory>

namespace Cande {

	// TODO: Create custom implementation
	template<typename T>
	using Shared = std::shared_ptr<T>;

}