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

		virtual void patch(CPU::CPURegs& regs) const;

	private:
		/**
		 * read/write sectors
		 */
		void PHYDIO(CPU::CPURegs& regs) const;
		//static const int A_PHYDIO;
		/**
		 * check disk
		 */
		void DSKCHG(CPU::CPURegs& regs) const;
		//static const int A_DSKCHG;

		/**
		 * get disk format
		 */
		void GETDPB(CPU::CPURegs& regs) const;
		//static const int A_GETDPB;

		/**
		 * format a disk
		 */
		void DSKFMT(CPU::CPURegs& regs) const;
		//static const int A_DSKFMT;
		static const byte bootSectorData[]; // size is 196

		/**
		 * stop drives
		 */
		void DRVOFF(CPU::CPURegs& regs) const;
		//static const int A_DRVOFF;

		DiskImageManager* diskImageManager;
};

#endif // __MSXDISKROMPATCH_HH__
