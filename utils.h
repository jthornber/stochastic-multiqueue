#ifndef SMQ_UTILS_H
#define SMQ_UTILS_H

#include <memory>

//----------------------------------------------------------------

namespace smq {
	template <typename T, typename... Ts>
	std::unique_ptr<T> make_unique(Ts &&... params) {
		return std::unique_ptr<T>(new T(std::forward<Ts>(params)...));
	}
}

//----------------------------------------------------------------

#endif
