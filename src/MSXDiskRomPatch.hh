// $Id$

#ifndef __MSXDISKROMPATCH_HH__
#define __MSXDISKROMPATCH_HH__

#include "MSXRomPatchInterface.hh"
#include "msxconfig.hh"
#include "config.h"
#include "CPU.hh"
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
			bool bad();

		private:
			IOFILETYPE *file;

	};

	private:

		/**
		 * read/write sectors
		 */
		void PHYDIO(CPU::CPURegs& regs) const;
		static const int A_PHYDIO;
		/**
		 * check disk
		 */
		void DSKCHG(CPU::CPURegs& regs) const;
		static const int A_DSKCHG;

		/**
		 * get disk format
		 */
		void GETDPB(CPU::CPURegs& regs) const;
		static const int A_GETDPB;

		/**
		 * format a disk
		 */
		void DSKFMT(CPU::CPURegs& regs) const;
		static const int A_DSKFMT;

		/**
		 * stop drives
		 */
		void DRVOFF(CPU::CPURegs& regs) const;
		static const int A_DRVOFF;

		// drive letters
		static const int A = 0;
		static const int B = 1;
		static const int LastDrive = B+1;

		// files for drives
		MSXDiskRomPatch::File* disk[MSXDiskRomPatch::LastDrive];

		// disk geometry
		struct geometry_info
		{
			int sectors;
		};

		static struct geometry_info geometry[8];
		static const int sector_size;
};

#endif // __MSXDISKROMPATCH_HH__
