// $Id$

#ifndef __MSXTapePatch_HH__
#define __MSXTapePatch_HH__

#include "MSXRomPatchInterface.hh"
#include "Command.hh"
#include "CommandLineParser.hh"

class File;
class FileContext;


class MSXCasCLI : public CLIOption, public CLIFileType
{
	public:
		MSXCasCLI();
		virtual void parseOption(const std::string &option,
				std::list<std::string> &cmdLine);
		virtual const std::string& optionHelp() const;
		virtual void parseFileType(const std::string &filename);
		virtual const std::string& fileTypeHelp() const;
};


class MSXTapePatch : public MSXRomPatchInterface, private Command
{
	public:
		MSXTapePatch();
		virtual ~MSXTapePatch();

		virtual void patch(CPU::CPURegs& regs);

	private:
		File* file;

		void insertTape(FileContext *context,
		                const std::string &filename);
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
		virtual void execute(const std::vector<std::string> &tokens,
		                     const EmuTime &time);
		virtual void help   (const std::vector<std::string> &tokens) const;
		virtual void tabCompletion(std::vector<std::string> &tokens) const;
};

#endif // __MSXTapePatch_HH__
