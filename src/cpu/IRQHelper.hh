// $Id$

#ifndef __IRQHELPER_HH__
#define __IRQHELPER_HH__

// forward declarations
class MSXCPU;


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
		inline void set();

		/** Reset the interrupt request on the bus.
		  */
		inline void reset();

		/** Get the interrupt state.
		  * @return true iff interrupt request is active.
		  */
		inline bool getState();

	private:
		bool request;
		MSXCPU *cpu;
};

#endif

