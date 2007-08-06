// $Id$

#ifndef ADVRAM_HH
#define ADVRAM_HH

#include "MSXDevice.hh"

namespace openmsx {

class EmuTime;
class VDP;
class VDPVRAM;

/** Implementation of direct cpu access to VRAM.  ADVram (Accesso
  * Direito &agrave; Vram is a rare hardware modification that allows the
  * CPU to access the video ram in the same way as ordinary ram.
  */
class ADVram : public MSXDevice
{
public:
	ADVram(MSXMotherBoard& motherBoard, const XMLElement& config,
	       const EmuTime& time);

	/** This method is called on reset.  Reset the mapper register and
	 * the planar bit, if the device is configured with an enable bit
	 * then that bit is reset as well.
	 */
	virtual void reset(const EmuTime& time);

	/** Read a byte from an IO port, change mode bits.  The planar bit
	 * and possibly the enable bit are set according to address lines
	 * that are normally ignored for IO reads.  Returns 255.
	 */
	virtual byte readIO(word port, const EmuTime& time);

	/** Write a byte to a given IO port, set mapper register.  */
	virtual void writeIO(word port, byte value, const EmuTime& time);

	/** Read a byte from a location in the video ram at a certain
	 * time.  If the device is enabled then the value returned comes
	 * from the video ram, otherwise it returns 255.
	 */
	virtual byte readMem(word address, const EmuTime& time);

	/** Write a given byte at a certain time to a given location in
	 * the video ram.  If the device is enabled then the write is
	 * redirected to the video ram, if it is not, nothing happens.
	 */
	virtual void writeMem(word address, byte value, const EmuTime& time);

private:
	virtual void init(const HardwareConfig& hwConf);
	inline unsigned calcAddress(word address) const;

	VDP* vdp;
	VDPVRAM* vram;
	/** Bit mask applied to logical addresses, reflects vram size.  */
	unsigned mask;
	/** Base address of selected page in vram.  */
	unsigned baseAddr;
	/** True if ADVram device is enabled.  */
	bool enabled;
	/** True if ADVram device can be switched on or off by performing
		IO-reads.  */
	bool hasEnable;
	/** True if address bits are shuffled as is appropriate for screen
		7 and higher.  */
	bool planar;
};

} // namespace openmsx

#endif // ADVRAM_HH
