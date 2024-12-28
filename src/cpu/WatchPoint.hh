#ifndef WATCHPOINT_HH
#define WATCHPOINT_HH

#include "BreakPointBase.hh"
#include "CommandException.hh"
#include "MSXMultiDevice.hh"

#include "narrow.hh"
#include "one_of.hh"
#include "strCat.hh"
#include "unreachable.hh"

#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
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
	static std::pair<TclObject, TclObject> parseAddress(Interpreter& interp, const TclObject& a)
	{
		auto len = a.getListLength(interp);
		if (len != one_of(1u, 2u)) {
			throw CommandException("Invalid address: must be a single address, or a begin/end pair");
		}
		auto begin = a.getListIndex(interp, 0);
		auto end = (len == 2) ? a.getListIndex(interp, 1) : TclObject{};
		return {begin, end};
	}

public:
	WatchPoint()
		: id(++lastId) {}
	WatchPoint(Interpreter& interp, TclObject command_, TclObject condition_,
	           Type type_, TclObject address, bool enabled_, bool once_, unsigned newId = -1)
		: BreakPointBase(std::move(command_), std::move(condition_), enabled_, once_)
		, id((newId == unsigned(-1)) ? ++lastId : newId)
		, type(type_)
	{
		setAddress(interp, address);
	}

	struct clone_tag {};
	WatchPoint(clone_tag, const WatchPoint& wp)
		: BreakPointBase(wp)
		, id(wp.id), type(wp.type)
		, beginAddrStr(wp.beginAddrStr), endAddrStr(wp.endAddrStr)
		, beginAddr(wp.beginAddr), endAddr(wp.endAddr)
	{
		assert(ios.empty());
		assert(!registered);
	}

	[[nodiscard]] unsigned getId() const { return id; }
	[[nodiscard]] std::string getIdStr() const { return strCat(prefix, id); }

	[[nodiscard]] Type getType() const { return type; }
	void setType(Type t) { type = t; }
	void setType(const TclObject& t) {
		setType(parseType(t.getString()));
	}

	[[nodiscard]] auto getBeginAddress() const { return beginAddr; }
	[[nodiscard]] auto getEndAddress()   const { return endAddr; }
	[[nodiscard]] auto getBeginAddressString() const { return beginAddrStr; }
	[[nodiscard]] auto getEndAddressString()   const { return endAddrStr; }
	void setBeginAddressString(Interpreter& interp, const TclObject& s) {
		beginAddrStr = s;
		evaluateAddress(interp);
	}
	void setEndAddressString(Interpreter& interp, const TclObject& s) {
		endAddrStr = s;
		evaluateAddress(interp);
	}
	void setAddress(Interpreter& interp, const TclObject& a) {
		std::tie(beginAddrStr, endAddrStr) = parseAddress(interp, a);
		evaluateAddress(interp);
	}

	std::string evaluateAddress(Interpreter& interp) {
		assert(!registered);
		try {
			auto begin = beginAddrStr.eval(interp).getInt(interp); // may throw
			auto end = endAddrStr.getString().empty()
			         ? begin
				 : endAddrStr.eval(interp).getInt(interp); // may throw
			if (end < begin) {
				throw CommandException("begin may not be larger than end: ", begin, " > ", end);
			}
			auto max = rangeForType(type);
			if ((begin < 0) || (end >= int(max))) {
				throw CommandException("address outside of range [0, ", max , ')');
			}
			beginAddr = narrow<uint16_t>(begin);
			endAddr   = narrow<uint16_t>(end);
			return {}; // success
		} catch (MSXException& e) {
			beginAddr = endAddr = {};
			return e.getMessage();
		}
	}

	void registerIOWatch(MSXMotherBoard& motherBoard, std::span<MSXDevice*, 256> devices);
	void unregisterIOWatch(std::span<MSXDevice*, 256> devices);

private:
	void doReadCallback(MSXMotherBoard& motherBoard, unsigned port);
	void doWriteCallback(MSXMotherBoard& motherBoard, unsigned port, unsigned value);

private:
	unsigned id;
	Type type = Type::WRITE_MEM;
	TclObject beginAddrStr;
	TclObject endAddrStr;
	std::optional<uint16_t> beginAddr; // begin and end address are inclusive (IOW range = [begin, end])
	std::optional<uint16_t> endAddr;

	std::vector<std::unique_ptr<MSXWatchIODevice>> ios;
	bool registered = false; // for debugging only

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
