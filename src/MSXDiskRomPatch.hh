// $Id$

#ifndef __MSXDISKROMPATCH_HH__
#define __MSXDISKROMPATCH_HH__

#include "MSXRomPatchInterface.hh"

class MSXDiskRomPatch: public MSXRomPatchInterface
{
	public:
		MSXDiskRomPatch();
		virtual ~MSXDiskRomPatch();

		virtual void patch(int address);
	
	private:

		/**
		 * read/write sectors
		 */
		void PHYDIO();

		/**
		 * check disk
		 */
		void DSKCHG();

		/**
		 * get disk format
		 */
		void GETDPB();

		/**
		 * format a disk
		 */
		void DSKFMT();

		/**
		 * stop drives
		 */
		void DRVOFF();
};

#endif // __MSXDISKROMPATCH_HH__
