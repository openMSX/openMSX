// $Id$

#ifndef __MSXTapePatch_HH__
#define __MSXTapePatch_HH__

#include "MSXRomPatchInterface.hh"
#include "Command.hh"
#include "FileOpener.hh"


class MSXTapePatch : public MSXRomPatchInterface, private Command
{
	public:
		MSXTapePatch();
		virtual ~MSXTapePatch();

		virtual void patch(CPU::CPURegs& regs);

	private:
		// TapeHeader used to be fMSX compatible
		static const byte TapeHeader[] = { 0x1F,0xA6,0xDE,0xBA,0xCC,0x13,0x7D,0x74 };
		IOFILETYPE* file;

		void insertTape(std::string filename);
		void ejectTape();

		// 0x00E1 Tape input ON
		void TAPION(CPU::CPURegs& R);

		// 0x00E4 Tape input
		void TAPIN (CPU::CPURegs& R);

		// 0x00E7 Tape input OFF
		void TAPIOF(CPU::CPURegs& R);

		// 0x00EA Tape output ON
		void TAPOON(CPU::CPURegs& R);

		// 0x00ED Tape output
		void TAPOUT(CPU::CPURegs& R);

		// 0x00F0 Tape output OFF
		void TAPOOF(CPU::CPURegs& R);

		// 0x00F3 Turn motor ON/OFF
		void STMOTR(CPU::CPURegs& R);

		// Tape Command
		virtual void execute(const std::vector<std::string> &tokens);
		virtual void help   (const std::vector<std::string> &tokens);
		virtual void tabCompletion(std::vector<std::string> &tokens);
};

#endif // __MSXTapePatch_HH__
