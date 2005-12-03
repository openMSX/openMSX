// $id:$
#ifndef ADVRAM_HH
#define ADVRAM_HH

#include "MSXDevice.hh"
#include "openmsx.hh"

namespace openmsx {
class EmuTime;
class VDP;
class VDPVRAM;
/** Implemntation of direct cpu access to VRAM.  ADVram (Accesso
 * Direito \`a Vram is a rare hardware modification that allows the
 * CPU to eccess the video ram in the same way as ordinary ram. 
 */

class ADVram : public MSXDevice
{
public:
	ADVram(MSXMotherBoard& motherBoard, const XMLElement& config,
				 const EmuTime& time);
	virtual ~ADVram();
	virtual void powerUp(const EmuTime& time);
	virtual byte readIO(word port, const EmuTime& time);
	virtual void writeIO(word port, byte value, const EmuTime& time);
	virtual byte readMem(word address, const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
private:
	VDP * thevdp;
	VDPVRAM * thevram;
	unsigned base_addr;
	unsigned mask;
	bool planar;
};


}
#endif // ADVRAM_HH
