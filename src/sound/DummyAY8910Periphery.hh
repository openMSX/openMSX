#ifndef DUMMYAY8910PERIPHERY_HH
#define DUMMYAY8910PERIPHERY_HH

#include "AY8910Periphery.hh"

namespace openmsx {

class DummyAY8910Periphery final : public AY8910Periphery
{
public:
	[[nodiscard]] static DummyAY8910Periphery& instance()
	{
		static DummyAY8910Periphery oneInstance;
		return oneInstance;
	}

	[[nodiscard]] byte readA(EmuTime /*time*/) override { return 255; }
	[[nodiscard]] byte readB(EmuTime /*time*/) override { return 255; }
	void writeA(byte /*value*/, EmuTime /*time*/) override {}
	void writeB(byte /*value*/, EmuTime /*time*/) override {}

private:
	DummyAY8910Periphery() = default;
	~DummyAY8910Periphery() = default;
};

}; // namespace openmsx

#endif
