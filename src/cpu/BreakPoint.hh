#ifndef BREAKPOINT_HH
#define BREAKPOINT_HH

#include "BreakPointBase.hh"
#include "CommandException.hh"

#include "strCat.hh"

#include <cstdint>
#include <optional>

namespace openmsx {

class BreakPoint final : public BreakPointBase
{
public:
	static constexpr std::string_view prefix = "bp#";

public:
	BreakPoint()
		: id(++lastId) {}
	BreakPoint(Interpreter& interp, TclObject address_, TclObject command_, TclObject condition_, bool enabled_, bool once_)
		: BreakPointBase(std::move(command_), std::move(condition_), enabled_, once_)
		, id(++lastId)
		, addrStr(std::move(address_))
	{
		evaluateAddress(interp);
	}

	[[nodiscard]] unsigned getId() const { return id; }
	[[nodiscard]] std::string getIdStr() const { return strCat(prefix, id); }

	[[nodiscard]] std::optional<uint16_t> getAddress() const { return address; }
	[[nodiscard]] TclObject getAddressString() const { return addrStr; }
	void setAddress(Interpreter& interp, const TclObject& addr) {
		addrStr = addr;
		evaluateAddress(interp);
	}

	std::string evaluateAddress(Interpreter& interp) {
		try {
			auto tmp = addrStr.eval(interp).getInt(interp); // may throw
			if ((tmp < 0) || (tmp > 0xffff)) {
				throw CommandException("address outside of range");
			}
			address = uint16_t(tmp);
			return {}; // success
		} catch (MSXException& e) {
			address = {};
			return e.getMessage();
		}
	}

private:
	unsigned id;
	TclObject addrStr;
	std::optional<uint16_t> address = 0;

	static inline unsigned lastId = 0;
};

} // namespace openmsx

#endif
