// $Id$

#ifndef __IRQHELPER_HH__
#define __IRQHELPER_HH__

#include "MSXCPU.hh"

namespace openmsx {

/** Helper class for doing interrupt request (IRQ) administration.
  * IRQ is either enabled or disabled; when enabled it contributes
  * one to the CPU IRQ count, when disabled zero.
  * Calling set() in enabled state does nothing;
  * neither does calling reset() in disabled state.
  */
class IRQHelper
{
public:
	/** Create a new IRQHelper.
	  * Initially there is no interrupt request on the bus.
	  * @param nmi true iff non-maskable interrupts should be triggered.
	  */
	IRQHelper(bool nmi = false);
	
	/** Destroy this IRQHelper.
	  * Resets interrupt request if it is active.
	  */
	~IRQHelper();

	/** Set the interrupt request on the bus.
	  */
	inline void set() {
		if (!request) {
			request = true;
			if (nmi) cpu.raiseNMI(); else cpu.raiseIRQ();
		}
	}

	/** Reset the interrupt request on the bus.
	  */
	inline void reset() {
		if (request) {
			request = false;
			if (nmi) cpu.lowerNMI(); else cpu.lowerIRQ();
		}
	}

	/** Get the interrupt state.
	  * @return true iff interrupt request is active.
	  */
	inline bool getState() const {
		return request;
	}

private:
	bool nmi;
	bool request;
	MSXCPU& cpu;
};

} // namespace openmsx

#endif
