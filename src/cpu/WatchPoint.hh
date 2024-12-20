#ifndef WATCHPOINT_HH
#define WATCHPOINT_HH

#include "BreakPointBase.hh"
#include "MSXMultiDevice.hh"

#include <cassert>
#include <memory>

namespace openmsx {

class MSXWatchIODevice;

class WatchPoint final : public BreakPointBase
                       , public std::enable_shared_from_this<WatchPoint>
{
public:
	enum class Type { READ_IO, WRITE_IO, READ_MEM, WRITE_MEM };

	/** Begin and end address are inclusive (IOW range = [begin, end])
	 */
	WatchPoint(TclObject command_, TclObject condition_,
	           Type type_, unsigned beginAddr_, unsigned endAddr_,
	           bool once_, unsigned newId = -1)
		: BreakPointBase(std::move(command_), std::move(condition_), once_)
		, id((newId == unsigned(-1)) ? ++lastId : newId)
		, beginAddr(beginAddr_), endAddr(endAddr_), type(type_)
	{
		assert(beginAddr <= endAddr);
	}

	[[nodiscard]] unsigned getId()           const { return id; }
	[[nodiscard]] Type     getType()         const { return type; }
	[[nodiscard]] unsigned getBeginAddress() const { return beginAddr; }
	[[nodiscard]] unsigned getEndAddress()   const { return endAddr; }

	void registerIOWatch(MSXMotherBoard& motherBoard, std::span<MSXDevice*, 256> devices);
	void unregisterIOWatch(std::span<MSXDevice*, 256> devices);

private:
	void doReadCallback(MSXMotherBoard& motherBoard, unsigned port);
	void doWriteCallback(MSXMotherBoard& motherBoard, unsigned port, unsigned value);

private:
	unsigned id;
	unsigned beginAddr;
	unsigned endAddr;
	Type type;

	std::vector<std::unique_ptr<MSXWatchIODevice>> ios;

	static inline unsigned lastId = 0;

	friend class MSXWatchIODevice;
};

class MSXWatchIODevice final : public MSXMultiDevice
{
public:
	MSXWatchIODevice(const HardwareConfig& hwConf, WatchPoint& wp);

	[[nodiscard]] MSXDevice*& getDevicePtr() { return device; }

private:
	// MSXDevice
	[[nodiscard]] const std::string& getName() const override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

private:
	WatchPoint& wp;
	MSXDevice* device = nullptr;
};

} // namespace openmsx

#endif
