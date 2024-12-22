#ifndef WATCHPOINT_HH
#define WATCHPOINT_HH

#include "BreakPointBase.hh"
#include "CommandException.hh"
#include "MSXMultiDevice.hh"

#include "one_of.hh"
#include "strCat.hh"
#include "unreachable.hh"

#include <cassert>
#include <memory>
#include <string_view>

namespace openmsx {

class MSXWatchIODevice;

class WatchPoint final : public BreakPointBase
                       , public std::enable_shared_from_this<WatchPoint>
{
public:
	static constexpr std::string_view prefix = "wp#";

	enum class Type { READ_IO, WRITE_IO, READ_MEM, WRITE_MEM };

	static std::string_view format(Type type)
	{
		switch (type) {
		using enum Type;
		case READ_IO:   return "read_io";
		case WRITE_IO:  return "write_io";
		case READ_MEM:  return "read_mem";
		case WRITE_MEM: return "write_mem";
		}
		UNREACHABLE;
	}
	static Type parseType(std::string_view str)
	{
		using enum Type;
		if (str == "read_io")   return READ_IO;
		if (str == "write_io")  return WRITE_IO;
		if (str == "read_mem")  return READ_MEM;
		if (str == "write_mem") return WRITE_MEM;
		throw CommandException("Invalid type: ", str);
	}
	static unsigned rangeForType(Type type)
	{
		return (type == one_of(Type::READ_IO, Type::WRITE_IO)) ? 0x100 : 0x10000;
	}
	static std::pair<unsigned, unsigned> parseAddress(Interpreter& interp, const TclObject& a, Type type)
	{
		unsigned begin = a.getListIndex(interp, 0).getInt(interp);
		unsigned end = (a.getListLength(interp) == 2)
		             ? a.getListIndex(interp, 1).getInt(interp)
		             : begin;
		if (end < begin) {
			throw CommandException(
				"Not a valid range: end address may "
				"not be smaller than begin address.");
		}
		if (end >= rangeForType(type)) {
			throw CommandException("Invalid address: out of range");
		}
		return {begin, end};
	}

public:
	WatchPoint()
		: id(++lastId) {}
	WatchPoint(TclObject command_, TclObject condition_,
	           Type type_, unsigned beginAddr_, unsigned endAddr_,
	           bool once_, unsigned newId = -1)
		: BreakPointBase(std::move(command_), std::move(condition_), once_)
		, id((newId == unsigned(-1)) ? ++lastId : newId)
		, beginAddr(beginAddr_), endAddr(endAddr_), type(type_)
	{
		assert(beginAddr <= endAddr);
	}

	[[nodiscard]] unsigned getId() const { return id; }
	[[nodiscard]] std::string getIdStr() const { return strCat(prefix, id); }
	[[nodiscard]] Type     getType()         const { return type; }
	[[nodiscard]] unsigned getBeginAddress() const { return beginAddr; }
	[[nodiscard]] unsigned getEndAddress()   const { return endAddr; }

	void registerIOWatch(MSXMotherBoard& motherBoard, std::span<MSXDevice*, 256> devices);
	void unregisterIOWatch(std::span<MSXDevice*, 256> devices);

	void setType(const TclObject& t) {
		type = parseType(t.getString());
	}
	void setAddress(Interpreter& interp, const TclObject& a) {
		// TODO store value with given formatting
		std::tie(beginAddr, endAddr) = parseAddress(interp, a, type);
	}

private:
	void doReadCallback(MSXMotherBoard& motherBoard, unsigned port);
	void doWriteCallback(MSXMotherBoard& motherBoard, unsigned port, unsigned value);

private:
	unsigned id;
	unsigned beginAddr = 0; // begin and end address are inclusive (IOW range = [begin, end])
	unsigned endAddr = 0;
	Type type = Type::WRITE_MEM;

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
