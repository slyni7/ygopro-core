/*
 * Copyright (c) 2023-2024, Edoardo Lolletti (edo9300) <edoardo762@gmail.com>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef BIT_H
#define BIT_H

#include <cstdint>

namespace bit {

template<typename T>
constexpr inline uint8_t popcnt(T val) {
	uint8_t ans = 0;
	while(val) {
		val &= val - 1;
		++ans;
	}
	return ans;
}

template<typename T, typename T2>
constexpr inline bool has_invalid_bits(T value, T2 allowed) {
	using BiggestInt = std::conditional_t<(sizeof(T) > sizeof(T2)), T, T2>;
	return (static_cast<BiggestInt>(value) & (~static_cast<BiggestInt>(allowed))) != static_cast<BiggestInt>(0);
}

}

#endif //BIT_H
