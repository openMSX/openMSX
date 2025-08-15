#ifndef HAMMINGWINDOW_HH
#define HAMMINGWINDOW_HH

#include "MemBuffer.hh"
#include "xrange.hh"

#include <cassert>
#include <cmath>
#include <map>
#include <numbers>
#include <span>

// Returns the coefficients of the 'hamming' window of length 'n'.
//     https://en.wikipedia.org/wiki/Window_function#Hann_and_Hamming_windows
inline std::span<const float> hammingWindow(unsigned n)
{
	static std::map<unsigned, openmsx::MemBuffer<float>> cache;
	auto [it, inserted] = cache.try_emplace(n);
	auto& window = it->second;

	if (inserted) {
		window.resize(n);
		for (auto i : xrange(n)) {
			window[i] = float(0.53836 - 0.46164 * cos(2 * std::numbers::pi * i / (n - 1)));
		}
		return window;
	}

	assert(window.size() == n);
	return window;
}

#endif
