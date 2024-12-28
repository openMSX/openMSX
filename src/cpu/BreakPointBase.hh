#ifndef BREAKPOINTBASE_HH
#define BREAKPOINTBASE_HH

#include "CommandException.hh"
#include "GlobalCliComm.hh"
#include "TclObject.hh"

#include "ScopedAssign.hh"
#include "strCat.hh"

namespace openmsx {

class Interpreter;

/** CRTP base class for CPU break and watch points.
 */
template<typename Derived>
class BreakPointBase
{
public:
	[[nodiscard]] unsigned getId() const { return id; }
	[[nodiscard]] std::string getIdStr() const { return strCat(Derived::prefix, id); }

	[[nodiscard]] TclObject getCondition() const { return condition; }
	[[nodiscard]] TclObject getCommand()   const { return command; }
	[[nodiscard]] bool isEnabled() const { return enabled; }
	[[nodiscard]] bool onlyOnce() const { return once; }

	void setCondition(const TclObject& c) { condition = c; }
	void setCommand(const TclObject& c) { command = c; }
	void setEnabled(Interpreter& interp, const TclObject& e) {
		setEnabled(e.getBoolean(interp)); // may throw
	}
	void setEnabled(bool e) { enabled = e; }
	void setOnce(Interpreter& interp, const TclObject& o) {
		setOnce(o.getBoolean(interp)); // may throw
	}
	void setOnce(bool o) { once = o; }

	bool checkAndExecute(GlobalCliComm& cliComm, Interpreter& interp) {
		if (!enabled) return false;
		if (executing) {
			// no recursive execution
			return false;
		}
		ScopedAssign sa(executing, true);
		if (isTrue(cliComm, interp)) {
			try {
				command.executeCommand(interp, true); // compile command
			} catch (CommandException& e) {
				cliComm.printWarning(e.getMessage());
			}
			return onlyOnce();
		}
		return false;
	}

protected:
	BreakPointBase() : id(++lastId) {}
	unsigned id;

private:
	// Note: we require GlobalCliComm here because breakpoint objects can
	// be transferred to different MSX machines, and so the MSXCliComm
	// object won't remain valid.
	[[nodiscard]] bool isTrue(GlobalCliComm& cliComm, Interpreter& interp) const {
		if (condition.getString().empty()) {
			// unconditional bp
			return true;
		}
		try {
			return condition.evalBool(interp);
		} catch (CommandException& e) {
			cliComm.printWarning(e.getMessage());
			return false;
		}
	}

private:
	TclObject command{"debug break"};
	TclObject condition;
	bool enabled = true;
	bool once = false;
	bool executing = false;

	static inline unsigned lastId = 0;
};

} // namespace openmsx

#endif
