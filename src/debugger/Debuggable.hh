#ifndef DEBUGGABLE_HH
#define DEBUGGABLE_HH

#include "narrow.hh"
#include "xrange.hh"

#include <cassert>
#include <cstdint>
#include <span>
#include <string_view>

namespace openmsx {

class Debuggable
{
public:
	Debuggable(const Debuggable&) = delete;
	Debuggable(Debuggable&&) = delete;
	Debuggable& operator=(const Debuggable&) = delete;
	Debuggable& operator=(Debuggable&&) = delete;

	[[nodiscard]] virtual unsigned getSize() const = 0;
	[[nodiscard]] virtual std::string_view getDescription() const = 0;
	[[nodiscard]] virtual uint8_t read(unsigned address) = 0;
	virtual void write(unsigned address, uint8_t value) = 0;

	virtual void readBlock(unsigned start, std::span<uint8_t> output) {
		// default implementation, subclasses may override it with a more efficient version
		assert(narrow<unsigned>(start + output.size()) <= getSize());
		for (auto i : xrange(output.size())) {
			output[i] = read(narrow_cast<unsigned>(start + i));
		}
	}

protected:
	Debuggable() = default;
	~Debuggable() = default;
};

} // namespace openmsx

#endif
