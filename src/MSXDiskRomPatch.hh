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

		/**
		 * check disk
		 */
		void DSKCHG() const;

		/**
		 * get disk format
		 */
		void GETDPB() const;

		/**
		 * format a disk
		 */
		void DSKFMT() const;

		/**
		 * stop drives
		 */
		void DRVOFF() const;
};

#endif // __MSXDISKROMPATCH_HH__
