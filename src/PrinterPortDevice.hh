// $Id$

#ifndef __PRINTERPORTDEVICE_HH__
#define __PRINTERPORTDEVICE_HH__

#include "openmsx.hh"
#include "Pluggable.hh"


class PrinterPortDevice : public Pluggable
{
	public:
		/**
		 * Returns the STATUS signal
		 *  false = low  = ready
		 *  true  = high = not ready
		 */
		virtual bool getStatus(const EmuTime &time) = 0;

		/**
		 * Sets the strobe signal.
		 *  false = low
		 *  true  = high
		 * Normal high, a short pulse (low, high) means data is valid
		 */ 
		virtual void setStrobe(bool strobe, const EmuTime &time) = 0;

		/**
		 * Sets the data signals.
		 * Always use strobe to see wheter data is valid
		 */
		virtual void writeData(byte data, const EmuTime &time) = 0;


		virtual const std::string &getClass();
private:
		static const std::string className;
};

#endif
