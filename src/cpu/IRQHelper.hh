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
		  */
		IRQHelper();
		
		/** Destroy this IRQHelper.
		  * Make sure interrupt is released.
		  */
		~IRQHelper();

		/** Set the interrupt request on the bus.
		  */
		inline void set() {
			if (!request) {
				request = true;
				cpu.raiseIRQ();
			}
		}

		/** Reset the interrupt request on the bus.
		  */
		inline void reset() {
			if (request) {
				request = false;
				cpu.lowerIRQ();
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
		MSXCPU& cpu;
};

} // namespace openmsx

#endif
