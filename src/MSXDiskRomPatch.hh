// $Id$

#ifndef __MSXDISKROMPATCH_HH__
#define __MSXDISKROMPATCH_HH__

#include "MSXRomPatchInterface.hh"

class MSXDiskRomPatch: public MSXRomPatchInterface
{
	public:
		MSXDiskRomPatch();
		virtual ~MSXDiskRomPatch();

		virtual void patch() const;
	
	private:

		/**
		 * read/write sectors
		 */
		void PHYDIO() const;
		static const int A_PHYDIO = 0x4010;
		/**
		 * check disk
		 */
		void DSKCHG() const;
		static const int A_DSKCHG = 0x4013;

		/**
		 * get disk format
		 */
		void GETDPB() const;
		static const int A_GETDPB = 0x4016;

		/**
		 * format a disk
		 */
		void DSKFMT() const;
		static const int A_DSKFMT = 0x401C;

		/**
		 * stop drives
		 */
		void DRVOFF() const;
		static const int A_DRVOFF = 0x401F;
};

#endif // __MSXDISKROMPATCH_HH__
