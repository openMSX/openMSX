// $Id$

#ifndef __MSXDISKROMPATCH_HH__
#define __MSXDISKROMPATCH_HH__

#include "MSXRomPatchInterface.hh"
#include "MSXConfig.hh"
#include "config.h"
#include "CPU.hh"
#include "FileOpener.hh"
#include "ConsoleSource/Command.hh"


//TODO replace DiskImage with fdc/FDCBackEnd,
//     move "disk" console command to FDCBackEnd

class MSXDiskRomPatch: public MSXRomPatchInterface, private Command
{
	class NoSuchSectorException : public MSXException {
		public:
			NoSuchSectorException(const std::string &desc) : MSXException(desc) {}
	};
	class DiskIOErrorException  : public MSXException {
		public:
			DiskIOErrorException(const std::string &desc) : MSXException(desc) {}
	};
	public:
	class DiskImage
	{
		public:
			DiskImage(std::string filename,std::string defaultSize);
			~DiskImage();

			void readSector(byte* to, int sector);
			void writeSector(const byte* from, int sector);

		private:
			int nbSectors;
			IOFILETYPE *file;
	};

		MSXDiskRomPatch();
		virtual ~MSXDiskRomPatch();

		virtual void patch() const;

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
		static const byte bootSectorData[]; // size is 196

		/**
		 * stop drives
		 */
		void DRVOFF(CPU::CPURegs& regs) const;
		static const int A_DRVOFF;

		// drive letters
		static const int A = 0;
		static const int B = 1;
		static const int LAST_DRIVE = B+1;

		// files for drives
		MSXDiskRomPatch::DiskImage* disk[LAST_DRIVE];

		static const int SECTOR_SIZE = 512;


		// Disk Command
		virtual void execute(const std::vector<std::string> &tokens);
		virtual void help   (const std::vector<std::string> &tokens);
		virtual void tabCompletion(std::vector<std::string> &tokens);
};

#endif // __MSXDISKROMPATCH_HH__
