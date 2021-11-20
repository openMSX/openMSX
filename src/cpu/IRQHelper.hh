#ifndef IRQHELPER_HH
#define IRQHELPER_HH

#include "Probe.hh"
#include "MSXMotherBoard.hh"
#include "serialize.hh"
#include <string>

namespace openmsx {

class MSXCPU;
class DeviceConfig;

/** Helper class for doing interrupt request (IRQ) administration.
  * IRQ is either enabled or disabled; when enabled it contributes
  * one to the CPU IRQ count, when disabled zero.
  * Calling set() in enabled state does nothing;
  * neither does calling reset() in disabled state.
  */

// policy class for IRQ source
class IRQSource
{
protected:
	explicit IRQSource(MSXCPU& cpu);
	void raise();
	void lower();
private:
	MSXCPU& cpu;
};

// supports <irq_connected> tag in hardware config
class OptionalIRQ
{
protected:
	OptionalIRQ(MSXCPU& cpu, const DeviceConfig& config);
	void raise();
	void lower();
private:
	MSXCPU& cpu;
	enum Type { NotConnected, Maskable, NonMaskable };
	const Type type;
};


// generic implementation
template<typename SOURCE> class IntHelper : public SOURCE
{
public:
	IntHelper(const IntHelper&) = delete;
	IntHelper& operator=(const IntHelper&) = delete;

	/** Create a new IntHelper.
	  * Initially there is no interrupt request on the bus.
	  */
	template<typename ...Args>
	IntHelper(MSXMotherBoard& motherboard, const std::string& name,
	          Args&& ...args)
		: SOURCE(motherboard.getCPU(), std::forward<Args>(args)...)
		, request(motherboard.getDebugger(), name,
		          "Outgoing IRQ signal.", false)
	{
	}

	/** Destroy this IntHelper.
	  * Resets interrupt request if it is active.
	  */
	~IntHelper() {
		reset();
	}

	/** Set the interrupt request on the bus.
	  */
	inline void set() {
		if (!request) {
			request = true;
			SOURCE::raise();
		}
	}

	/** Reset the interrupt request on the bus.
	  */
	inline void reset() {
		if (request) {
			request = false;
			SOURCE::lower();
		}
	}

	/** Convenience function: calls set() or reset().
	  */
	inline void set(bool s) {
		if (s) {
			set();
		} else {
			reset();
		}
	}

	/** Get the interrupt state.
	  * @return true iff interrupt request is active.
	  */
	[[nodiscard]] inline bool getState() const {
		return request;
	}

	template<typename Archive>
	void serialize(Archive& ar, unsigned /*version*/)
	{
		bool pending = request;
		ar.serialize("pending", pending);
		if constexpr (Archive::IS_LOADER) {
			set(pending);
		}
	}

private:
	Probe<bool> request;
};

// convenience types
using IRQHelper         = IntHelper<IRQSource>;
using OptionalIRQHelper = IntHelper<OptionalIRQ>;

} // namespace openmsx

#endif
