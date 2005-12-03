// $id:$
#include "MSXDevice.hh"
#include "ADVram.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "MSXMotherBoard.hh"

#include "openmsx.hh"

namespace openmsx {
class EmuTime;

ADVram::ADVram(MSXMotherBoard& motherBoard, const XMLElement& config,
							 const EmuTime& time)
	: MSXDevice(motherBoard, config, time), 
		thevdp(0),
		thevram(0),
		base_addr(0),
		planar(false)
{}

ADVram::~ADVram()
{
	if (thevdp) {
		getMotherBoard().releaseDevice(thevdp);
		thevdp=0;
	}
}

void ADVram::powerUp(const EmuTime& time)
{
	if (&time);
	if (! thevdp) {
		/* TODO: get the id for the vdp from the config-file
			 XMLElement::Children vdpConfig;
			 config.getChildren("vdp", vdpConfig);
		*/
		std::string vdpid="VDP";
		MSXDevice *d=getMotherBoard().lockDevice(vdpid);
		thevdp=dynamic_cast<VDP *>(d);
	}
	thevram=&(thevdp->getVRAM());
	mask=thevram->getSize();
	if (mask==192*1024)
		mask=128*1024;
	--mask;
}

byte ADVram::readIO(word port, const EmuTime& /*time*/ )
{
	// ADVram only gets 'read's from 0x9A
	planar=((port&0x100) != 0);
	return 0xFF;
}

void ADVram::writeIO(word /*port*/ , byte value, const EmuTime& /*time*/ )
{
	// set mapper register 
  base_addr=(value&0x07)<<14;
}

byte ADVram::readMem(word address, const EmuTime& time)
{
  int addr = (address & 0x3FFF) | base_addr;
  if (planar)
		addr = ((addr&1)<<16) + (addr>>1);
  
	return thevram->cpuRead(addr&mask, time);
}

void ADVram::writeMem(word address, byte value, const EmuTime& time)
{
  int addr = (address & 0x3FFF) | base_addr;
  if (planar)
		addr = ((addr&1)<<16) + (addr>>1);
  
	thevram->cpuWrite(addr&mask, value, time);
}


}
