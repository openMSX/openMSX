// $Id$

#include "MSXTapePatch.hh"
#include "CommandController.hh"
#include "MSXConfig.hh"
#include "Config.hh"
#include "libxmlx/xmlx.hh"
#include "File.hh"


namespace openmsx {

MSXCasCLI msxCasCLI;

MSXCasCLI::MSXCasCLI()
{
	CommandLineParser::instance().registerOption("-cas", this);
	CommandLineParser::instance().registerFileClass("cassetteimages", this);
}

bool MSXCasCLI::parseOption(const string &option,
                            list<string> &cmdLine)
{
	parseFileType(getArgument(option, cmdLine));
	return true;
}
const string& MSXCasCLI::optionHelp() const
{
	static const string text(
		"Put tape image in CAS format specified in argument "
		"in virtual cassette player");
	return text;
}

void MSXCasCLI::parseFileType(const string &filename_)
{
	string filename(filename_); XML::Escape(filename);
	ostringstream s;
	s << "<?xml version=\"1.0\"?>";
	s << "<msxconfig>";
	s << " <config id=\"cas\">";
	s << "  <parameter name=\"filename\">" << filename << "</parameter>";
	s << " </config>";
	s << "</msxconfig>";

	UserFileContext context;
	MSXConfig::instance().loadStream(context, s);
}
const string& MSXCasCLI::fileTypeHelp() const
{
	static const string text("Tape image in fMSX CAS format");
	return text;
}


/*
 TODO: Decided what to do with following variables
 
 FCA4H LOWLIM: DEFB 31H

 This variable is used to hold the minimum allowable start bit
 duration as determined by the TAPION standard routine.

 FCA5H WINWID: DEFB 22H

 This variable is used to hold the LO/HI cycle discrimination
 duration as determined by the TAPION standard routine.
*/

// TapeHeader used to be fMSX compatible
static const byte TapeHeader[8] = { 0x1F,0xA6,0xDE,0xBA,0xCC,0x13,0x7D,0x74 };

MSXTapePatch::MSXTapePatch()
{
	file = NULL;

	MSXConfig& conf = MSXConfig::instance();
	if (conf.hasConfigWithId("cas")) {
		Config *config = conf.getConfigById("cas");
		const string &filename = config->getParameter("filename");
		try {
			insertTape(config->getContext(), filename);
		} catch (MSXException& e) {
			throw FatalError("Couldn't load tape image: " + filename);
		}
	} else {
		// no image specified
	}
	CommandController::instance().registerCommand(this, "cas");
}

MSXTapePatch::~MSXTapePatch()
{
	CommandController::instance().unregisterCommand(this, "cas");
	delete file;
}

void MSXTapePatch::patch(CPU::CPURegs& R)
{
	switch (R.PC.w-2) {
		case 0x00E1:
			TAPION(R);
			break;
		case 0x00E4:
			TAPIN(R);
			break;
		case 0x00E7:
			TAPIOF(R);
			break;
		case 0x00EA:
			TAPOON(R);
			break;
		case 0x00ED:
			TAPOUT(R);
			break;
		case 0x00F0:
			TAPOOF(R);
			break;
		case 0x00F3:
			STMOTR(R);
			break;
		default:
			// patch not for MSXTapePatch
			break;
	}
}

void MSXTapePatch::insertTape(FileContext &context,
                              const string &filename)
{
	ejectTape();
	PRT_DEBUG("Loading file " << filename << " as tape ...");
	try {
		file = new File(context.resolve(filename));
	} catch (FileException &e) {
		PRT_DEBUG("Loading file failed");
		file = NULL;
	}
}

void MSXTapePatch::ejectTape()
{
	PRT_DEBUG("Ejecting tape from virtual tapedeck...");
	delete file;
	file = NULL;
}

void MSXTapePatch::TAPION(CPU::CPURegs& R)
{
	/*
	   Address... 1A63H
	   Name...... TAPION
	   Entry..... None
	   Exit...... Flag C if CTRL-STOP termination
	   Modifies.. AF, BC, DE, HL, DI

	   Standard routine to turn the cassette motor on, read the
	   cassette until a header is found and then determine the baud
	   rate. Successive cycles are read from the cassette and the
	   length of each one measured (1B34H). When 1,111 cycles have
	   been found with less than 35 æs variation in their lengths a
	   header has been located.

	   The next 256 cycles are then read (1B34H) and averaged to
	   determine the cassette HI cycle length. This figure is
	   multiplied by 1.5 and placed in LOWLIM where it defines the
	   minimum acceptable length of a 0 start bit. The HI cycle length
	   is placed in WINWID and will be used to discriminate between LO
	   and HI cycles.
	 */

	if (!file) {
		PRT_DEBUG("TAPION: No tape file opened");
		R.AF.B.l |= CPU::C_FLAG;
		return;
	}

	PRT_DEBUG("TAPION: Looking for header...");

	// go forward to multiple of 8 bytes
	try {
		int filePos = file->getPos();
		file->seek((filePos + 7) & ~7);
	} catch (FileException &e) {
		R.AF.B.l |= CPU::C_FLAG;
		return;
	}

	try {
		while (true) {
			byte buffer[10];
			file->read(buffer, 8);
			if (!memcmp(buffer, TapeHeader, 8)) {
				PRT_DEBUG("TAPION: OK");
				R.di();
				R.AF.B.l &= ~CPU::C_FLAG;
				return;
			} 
		}
	} catch (FileException &e) {
		PRT_DEBUG("TAPION : No header found");
		//rewind the tape
		file->seek(0);
	}
}

void MSXTapePatch::TAPIN(CPU::CPURegs& R)
{
	/*
	   Address... 1ABCH
	   Name...... TAPIN
	   Entry..... None
	   Exit...... A=Byte read, Flag C if CTRL-STOP or I/O error
	   Modifies.. AF, BC, DE, L

	   Standard routine to read a byte of data from the
	   cassette.  The cassette is first read continuously until
	   a start bit is found. This is done by locating a
	   negative transition, measuring the following cycle
	   length (1B1FH) and comparing this to see if it is
	   greater than LOWLIM.

	   Each of the eight data bits is then read by counting the
	   number of transitions within a fixed period of time
	   (1B03H). If zero or one transitions are found it is a 0
	   bit, if two or three are found it is a 1 bit. If more
	   than three transitions are found the routine terminates
	   with Flag C as this is presumed to be a hardware error
	   of some sort. After the value of each bit has been
	   determined a further one or two transitions are read
	   (1B23H) to retain synchronization. With an odd
	   transition count one more will be read, with an even
	   transition count two more.
	 */
	PRT_DEBUG("TAPIN");

	if (!file) {
		R.AF.B.l |= CPU::C_FLAG;
		return;
	}
	
	try {
		file->read(&R.AF.B.h, 1);
		R.AF.B.l &= ~CPU::C_FLAG;
	} catch (FileException &e) {
		R.AF.B.l |= CPU::C_FLAG;
	}
}

void MSXTapePatch::TAPIOF(CPU::CPURegs& R)
{
	/*
	   Address... 19E9H
	   Name...... TAPIOF
	   Entry..... None
	   Exit...... None
	   Modifies.. EI

	   Standard routine to stop the cassette motor after data
	   has been read from the cassette. The motor relay is
	   opened via the PPI Mode Port. Note that interrupts,
	   which must be disabled during cassette data transfers
	   for timing reasons, are enabled as this routine
	   terminates.
	 */
	PRT_DEBUG("TAPIOF");
	R.AF.B.l &= ~CPU::C_FLAG;
	R.ei();
}

void MSXTapePatch::TAPOON(CPU::CPURegs& R)
{
	/*
	   Address... 19F1H
	   Name...... TAPOON
	   Entry..... A=Header length switch
	   Exit...... Flag C if CTRL-STOP termination
	   Modifies.. AF, BC, HL, DI

	   Standard routine to turn the cassette motor on, wait 550
	   ms for the tape to come up to speed and then write a
	   header to the cassette. A header is a burst of HI cycles
	   written in front of every data block so the baud rate
	   can be determined when the data is read back.

	   The length of the header is determined by the contents of
	   register A: 00H=Short header, NZ=Long header. The BASIC
	   cassette statements "SAVE", "CSAVE" and "BSAVE" all
	   generate a long header at the start of the file, in front
	   of the identification block, and thereafter use short
	   headers between data blocks. The number of cycles in the
	   header is also modified by the current baud rate so as to
	   keep its duration constant:


	   1200 Baud SHORT ... 3840 Cycles ... 1.5 Seconds
	   1200 Baud LONG ... 15360 Cycles ... 6.1 Seconds
	   2400 Baud SHORT ... 7936 Cycles ... 1.6 Seconds
	   2400 Baud LONG ... 31744 Cycles ... 6.3 Seconds


	   After the motor has been turned on and the delay has
	   expired the contents of HEADER are multiplied by two
	   hundred and fifty- six and, if register A is non-zero, by
	   a further factor of four to produce the cycle count. HI
	   cycles are then generated (1A4DH) until the count is
	   exhausted whereupon control transfers to the BREAKX
	   standard routine. Because the CTRL-STOP key is only
	   examined at termination it is impossible to break out
	   part way through this routine.
	 */
	PRT_DEBUG("TAPOON");

	if (!file) {
		R.AF.B.l |= CPU::C_FLAG;
		return;
	}
	
	try {
		// go forward to multiple of 8 bytes
		int filePos = file->getPos();
		file->seek((filePos + 7) & ~7);
	
		file->write(TapeHeader, 8);
		R.AF.B.l &= ~CPU::C_FLAG;
		R.di();
	} catch (FileException &e) {
		R.AF.B.l |= CPU::C_FLAG;
	}
	
}

void MSXTapePatch::TAPOUT(CPU::CPURegs& R)
{
	/*
	   Address... 1A19H
	   Name...... TAPOUT
	   Entry..... A=Data byte
	   Exit...... Flag C if CTRL-STOP termination
	   Modifies.. AF, B, HL

	   Standard routine to write a single byte of data to the
	   cassette. The MSX ROM uses a software driven FSK
	   (Frequency Shift Keyed) method for storing information
	   on the cassette. At the 1200 baud rate this is identical
	   to the Kansas City Standard used by the BBC for the
	   distribution of BASICODE programs.

	   At 1200 baud each 0 bit is written as one complete 1200
	   Hz LO cycle and each 1 bit as two complete 2400 Hz HI
	   cycles. The data rate is thus constant as 0 and 1 bits
	   have the same duration.  When the 2400 baud rate is
	   selected the two frequencies change to 2400 Hz and 4800
	   Hz but the format is otherwise unchanged.
	 */
	PRT_DEBUG("TAPOUT");
	if (!file) {
		R.AF.B.l |= CPU::C_FLAG;
		return;
	}

	try {
		file->write(&R.AF.B.h, 1);
		R.AF.B.l &= ~CPU::C_FLAG;
	} catch (FileException &e) {
		R.AF.B.l |= CPU::C_FLAG;
	}
}

void MSXTapePatch::TAPOOF(CPU::CPURegs& R)
{
	/*
	   Address... 19DDH
	   Name...... TAPOOF
	   Entry..... None
	   Exit...... None
	   Modifies.. EI

	   Standard routine to stop the cassette motor after data
	   has been written to the cassette. After a delay of 550
	   ms, on MSX machines with one wait state, control drops
	   into the TAPIOF standard routine.
	 */
	PRT_DEBUG("TAPOOF");
	R.AF.B.l &= ~CPU::C_FLAG;
	R.ei();
}

void MSXTapePatch::STMOTR(CPU::CPURegs& R)
{
	/*
	   Entry..... A=Motor ON/OFF code
	   Exit...... None
	   Modifies.. AF

	   Standard routine to turn the cassette motor relay on or
	   off via PPI Port C: 00H=Off, 01H=On, FFH=Reverse current
	   state.
	 */
	PRT_DEBUG("STMOTR");
	R.AF.B.l &= ~CPU::C_FLAG;
}


string MSXTapePatch::execute(const vector<string> &tokens)
	throw (CommandException)
{
	string result;
	if (tokens.size() != 2) {
		throw CommandException("Syntax error");
	}
	if (tokens[1]=="eject") {
		result += "Tape ejected\n";
		ejectTape();
	} else if (tokens[1] == "rewind") {
		result += "Tape rewinded\n";
		if (file) {
			file->seek(0);
		}
	} else {
		result += "Changing tape\n";
		UserFileContext context;
		insertTape(context, tokens[1]);
	}
	return result;
}

string MSXTapePatch::help(const vector<string> &tokens) const throw()
{
	return "tape eject      : remove tape from virtual player\n"
	       "tape <filename> : change the tape file\n";
}

void MSXTapePatch::tabCompletion(vector<string> &tokens) const throw()
{
	if (tokens.size()==2)
		CommandController::completeFileName(tokens);
}

} // namespace openmsx
