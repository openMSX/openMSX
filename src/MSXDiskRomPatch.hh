// $Id$

#ifndef __MSXDISKROMPATCH_HH__
#define __MSXDISKROMPATCH_HH__

#include <string>
#include "MSXRomPatchInterface.hh"

namespace openmsx {

class DoubleSidedDrive;

class MSXDiskRomPatch : public MSXRomPatchInterface
{
public:
	MSXDiskRomPatch();
	virtual ~MSXDiskRomPatch();

	virtual void patch(CPU::CPURegs& regs);

private:
	/**
	 * read/write sectors
	 */
	void PHYDIO(CPU::CPURegs& regs);
	
	/**
	 * check disk
	 */
	void DSKCHG(CPU::CPURegs& regs);

	/**
	 * get disk format
	 */
	void GETDPB(CPU::CPURegs& regs);

	/**
	 * format a disk
	 */
	void DSKFMT(CPU::CPURegs& regs);
	static const byte bootSectorData[]; // size is 196

	/**
	 * stop drives
	 */
	void DRVOFF(CPU::CPURegs& regs);

	static const int LAST_DRIVE = 2;
	DoubleSidedDrive* drives[LAST_DRIVE];
};

} // namespace openmsx

#endif // __MSXDISKROMPATCH_HH__
