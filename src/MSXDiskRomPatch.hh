// $Id$

#ifndef __MSXDISKROMPATCH_HH__
#define __MSXDISKROMPATCH_HH__

#include "MSXRomPatchInterface.hh"
#include "MSXConfig.hh"

// forward declarations
class DiskImageManager;


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

		DiskImageManager* diskImageManager;
};

#endif // __MSXDISKROMPATCH_HH__
