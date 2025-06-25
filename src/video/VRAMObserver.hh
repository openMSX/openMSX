#ifndef VRAMOBSERVER_HH
#define VRAMOBSERVER_HH

#include "EmuTime.hh"

namespace openmsx {

/** Interface that can be registered at VRAMWindow,
  * to be called when the contents of the VRAM inside that window change.
  */
class VRAMObserver {
public:
	/** Informs the observer of a change in VRAM contents.
	  * This update is sent just before the change,
	  * so the subcomponent can update itself to the given time
	  * based on the old contents.
	  * @param offset Offset of byte that will change,
	  *               relative to window base address.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateVRAM(unsigned offset, EmuTime time) = 0;

	/** Informs the observer that the entire VRAM window will change.
	  * This update is sent just before the change,
	  * so the subcomponent can update itself to the given time
	  * based on the old contents.
	  * This happens if the base/index masks are changed,
	  * or if the window becomes disabled.
	  * TODO: Separate enable/disable from window move?
	  * @param enabled Will the window be enabled after the change?
	  *     If the observer keeps a cache which is based on VRAM contents,
	  *     it is only necessary to flush the cache if the new window
	  *     is enabled, because no reads are allowed from disabled windows.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateWindow(bool enabled, EmuTime time) = 0;

protected:
	~VRAMObserver() = default;
};

} // namespace openmsx

#endif
