// $Id$

#ifndef IRQHELPER_HH
#define IRQHELPER_HH

#include "MSXCPU.hh"

namespace openmsx {

/** Helper class for doing interrupt request (IRQ) administration.
  * IRQ is either enabled or disabled; when enabled it contributes
  * one to the CPU IRQ count, when disabled zero.
  * Calling set() in enabled state does nothing;
  * neither does calling reset() in disabled state.
  */

// policy class for IRQ source
class IRQSource {
protected:
	IRQSource(MSXCPU& cpu_) : cpu(cpu_) {}
	inline void raise() { cpu.raiseIRQ(); }
	inline void lower() { cpu.lowerIRQ(); }
private:
	MSXCPU& cpu;
};

// policy class for NMI source
class NMISource {
protected:
	NMISource(MSXCPU& cpu_) : cpu(cpu_) {}
	inline void raise() { cpu.raiseNMI(); }
	inline void lower() { cpu.lowerNMI(); }
private:
	MSXCPU& cpu;
};

// policy class that can dynamically switch between IRQ and NMI
class DynamicSource {
public:
	enum IntType { IRQ, NMI };
	void setIntType(IntType type_) { type = type_; }
protected:
	DynamicSource(MSXCPU& cpu_) : cpu(cpu_), type(IRQ) {}
	inline void raise() {
		if (type == IRQ) cpu.raiseIRQ(); else cpu.raiseNMI();
	}
	inline void lower() {
		if (type == NMI) cpu.lowerIRQ(); else cpu.lowerNMI();
	}
private:
	MSXCPU& cpu;
	IntType type;
};

// generic implementation
template <typename SOURCE> class IntHelper : public SOURCE
{
public:
	/** Create a new IntHelper.
	  * Initially there is no interrupt request on the bus.
	  */
	IntHelper(MSXCPU& cpu)
		: SOURCE(cpu), request(false) {
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

	/** Get the interrupt state.
	  * @return true iff interrupt request is active.
	  */
	inline bool getState() const {
		return request;
	}

private:
	bool request;
};

// convenience types
typedef IntHelper<IRQSource> IRQHelper;
typedef IntHelper<NMISource> NMIHelper;
typedef IntHelper<DynamicSource> IRQNMIHelper;

} // namespace openmsx

#endif
