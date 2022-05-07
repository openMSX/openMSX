#ifndef CHECKEDRAM_HH
#define CHECKEDRAM_HH

#include "Ram.hh"
#include "TclCallback.hh"
#include "CacheLine.hh"
#include "Observer.hh"
#include "openmsx.hh"
#include <vector>
#include <bitset>

namespace openmsx {

class DeviceConfig;
class MSXCPU;
class Setting;

/**
 * This class keeps track of which bytes in the Ram have been written to. It
 * can be used for debugging MSX programs, because you can see if you are
 * trying to read/execute uninitialized memory. Currently all normal RAM
 * (MSXRam) and all normal memory mappers (MSXMemoryMappers) use CheckedRam. On
 * the turboR, only the normal memory mapper runs via CheckedRam. The RAM
 * accessed in DRAM mode or via the ROM mapper are unchecked! Note that there
 * is basically no overhead for using CheckedRam over Ram, thanks to Wouter.
 */
class CheckedRam final : private Observer<Setting>
{
public:
	CheckedRam(const DeviceConfig& config, const std::string& name,
	           static_string_view description, unsigned size);
	~CheckedRam();

	[[nodiscard]] byte read(unsigned addr);
	[[nodiscard]] byte peek(unsigned addr) const { return ram[addr]; }
	void write(unsigned addr, byte value);

	[[nodiscard]] const byte* getReadCacheLine(unsigned addr) const;
	[[nodiscard]] byte* getWriteCacheLine(unsigned addr) const;
	[[nodiscard]] byte* getRWCacheLines(unsigned addr, unsigned size) const;

	[[nodiscard]] unsigned getSize() const { return ram.getSize(); }
	void clear();

	/**
	 * Give access to the unchecked Ram. No problem to use it, but there
	 * will just be no checking done! Keep in mind that you should use this
	 * consistently, so that the initialized-administration will be always
	 * up to date!
	 */
	[[nodiscard]] Ram& getUncheckedRam() { return ram; }

	// TODO
	//void serialize(Archive auto& ar, unsigned version);

private:
	void init();

	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

private:
	std::vector<bool> completely_initialized_cacheline;
	std::vector<std::bitset<CacheLine::SIZE>> uninitialized;
	Ram ram;
	MSXCPU& msxcpu;
	TclCallback umrCallback;
};

} // namespace openmsx

#endif
