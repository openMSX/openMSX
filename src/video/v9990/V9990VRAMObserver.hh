// $Id$

#ifndef __V9990VRAMOBSERVER_HH__
#define __V9990VRAMOBSERVER_HH__

namespace openmsx {

class EmuTime;

/** Interface class for V9990 VRAM Observers. A VRAM observer will be
  * notified when the contents of the VRAM within a certain window
  * changes.
  *
  * The interface to the VRAM for VRAMObservers
  */

class V9990VRAMObserver {
public:
	V9990VRAMObserver();
	V9990VRAMObserver(unsigned startAddress, unsigned size);
	virtual ~V9990VRAMObserver() {}
	
	/** Informs the observer of the VRAM content change.
	  * @param offset  Offset in observed VRAM window of the byte that changed.
	  */
	virtual void updateVRAM(unsigned offset/*, const EmuTime& time*/) = 0;
	
	/** Checks whether address is within observed VRAM window
	  * @param address Address to check
	  */
	bool isInWindow(unsigned address) {
		unsigned tmp = address - startAddress;
		return tmp < size;
	}

private:
	/** VRAM Window start address
	  */
	unsigned startAddress;
	
	/** VRAM Window size
	  */
	unsigned size;
};

}

#endif // __V9990VRAMOBSERVER_HH__
