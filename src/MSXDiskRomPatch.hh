// $Id$

#ifndef __MSXDISKROMPATCH_HH__
#define __MSXDISKROMPATCH_HH__

#include "MSXRomPatchInterface.hh"
#include "msxconfig.hh"
#include "config.h"

#include <iostream>
#include <fstream>

#ifdef HAVE_FSTREAM_TEMPL
#define IOFILETYPE std::fstream<byte>
#else
#define IOFILETYPE std::fstream
#endif

class MSXDiskRomPatch: public MSXRomPatchInterface
{
	public:
		MSXDiskRomPatch();
		virtual ~MSXDiskRomPatch();

		virtual void patch() const;

	class File
	{
		public:
			File(std::string filename);
			~File();

			std::string filename;
			int size;

			void seek(int location);
			void read(byte* to, int count);
			void write(const byte* from, int count);

		private:
			IOFILETYPE *file;

	};

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

		static const int A = 0;
		static const int B = 1;
		static const int LastDrive = B+1;

		MSXDiskRomPatch::File* disk[MSXDiskRomPatch::LastDrive];
};

#endif // __MSXDISKROMPATCH_HH__
