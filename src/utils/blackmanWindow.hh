#ifndef BLACKMANWINDOW_HH
#define BLACKMANWINDOW_HH

#include "xrange.hh"

#include <cassert>
#include <cmath>
#include <map>
#include <numbers>
#include <span>
#include <vector>

// Returns the coefficients of the 'blackman' window of length 'n'.
//     https://en.wikipedia.org/wiki/Window_function#Blackman_window
inline std::span<const float> blackmanWindow(unsigned n)
{
	static std::map<unsigned, std::vector<float>> cache;
	auto [it, inserted] = cache.try_emplace(n);
	auto& window = it->second;

	if (inserted) {
		// parameter values for the 'exact' blackman window
		static constexpr double alpha0 = 3969.0 / 9304.0;
		static constexpr double alpha1 = 4620.0 / 9304.0;
		static constexpr double alpha2 =  715.0 / 9304.0;

		window.resize(n);
		for (auto i : xrange(n)) {
			window[i] = static_cast<float>(
				  alpha0
				- alpha1 * std::cos(2 * std::numbers::pi * i / (n - 1))
				+ alpha2 * std::cos(4 * std::numbers::pi * i / (n - 1)));
		}
	}

	assert(window.size() == n);
	return window;
}

#endif
