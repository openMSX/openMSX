// $Id$

#ifndef __VRAMOBSERVER_HH__
#define __VRAMOBSERVER_HH__

class EmuTime;

/** Interface that can be registered at VRAMWindow,
  * to be called when the contents of the VRAM inside that window change.
  */
class VRAMObserver {
public:
	/** Informs the observer of a change in VRAM contents.
	  * @param address The address that will change.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateVRAM(int address, const EmuTime &time) = 0;

	/** Informs the observer that the entire VRAM window changed.
	  * This happens if the base/index masks are changed,
	  * TODO: or if the window becomes disabled?
	  * A typical observer will flush its cache.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateWindow(const EmuTime &time) = 0;
};

#endif //__VRAMOBSERVER_HH__

