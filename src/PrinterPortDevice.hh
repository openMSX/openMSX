// $Id$

#ifndef __PRINTERPORTDEVICE_HH__
#define __PRINTERPORTDEVICE_HH__

#include "openmsx.hh"

class PrinterPortDevice
{
	public:
		/**
		 * Returns the STATUS signal
		 *  false = low  = ready
		 *  true  = high = not ready
		 */
		virtual bool getStatus() = 0;

		/**
		 * Sets the strobe signal.
		 *  false = low
		 *  true  = high
		 * Normal high, a short pulse (low, high) means data is valid
		 */ 
		virtual void setStrobe(bool strobe) = 0;

		/**
		 * Sets the data signals.
		 * Always use strobe to see wheter data is valid
		 */
		virtual void writeData(byte data) = 0;
};

#endif

