// $Id$

#include "openmsx.hh"
#include "MSXTapePatch.hh"
#include "msxconfig.hh"
#include "CPU.hh"
#include "MSXCPU.hh"


const byte MSXTapePatch::TapeHeader[];

MSXTapePatch::MSXTapePatch()
{
	// TODO: move consts towards class
	addr_list.push_back(0x00E1);
	addr_list.push_back(0x00E4);
	addr_list.push_back(0x00E7);
	addr_list.push_back(0x00EA);
	addr_list.push_back(0x00ED);
	addr_list.push_back(0x00F0);
	addr_list.push_back(0x00F3);

	std::string name("tapepatch");
	std::string filename;
	file=0;

	try {
		MSXConfig::Config *config =
			MSXConfig::instance()->getConfigById(name);
		filename = config->getParameter("filename");
		insertTape(filename);
	} catch (MSXException e) {
		PRT_DEBUG("No correct tape insertion!");
	}
}

MSXTapePatch::~MSXTapePatch()
{
}

void MSXTapePatch::patch() const
{
	PRT_DEBUG("void MSXTapePatch::patch() const");
	
	// TODO: get CPU[PC] of patch instruction
	CPU::CPURegs& R = MSXCPU::instance()->getCPURegs();

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
			assert(false);
	}
}
void MSXTapePatch::insertTape(std::string filename)
{
	PRT_DEBUG("Loading file " << filename << " as tape ...");
	file = new FILETYPE(filename.c_str(), std::ios::in|std::ios::out);
	if (!file) {
		PRT_DEBUG("Loading file failed");
	}
}

void MSXTapePatch::TAPION(CPU::CPURegs& R) const
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

	if (file == 0) {
		PRT_DEBUG("TAPION : No tape file opened ?");
		R.AF.B.l |= CPU::C_FLAG;
		return;
	}

	PRT_DEBUG("TAPION : Looking for header...");
	byte buffer[10];

	// in case of failure
	R.AF.B.l |= CPU::C_FLAG;

	//fmsx does some positioning stuff first so to be compatible...
	int filePosition=(file->tellg() & 7);
	if (filePosition) {
		PRT_DEBUG("TAPION : filePosition " << filePosition);
		file->seekg(8-filePosition , ios::cur);
		if (file->fail()) {
			PRT_DEBUG("TAPION : Read error");
			//rewind the tape
			file->seekg(0, ios::beg);
			return;
		}
	}

	do {
		PRT_DEBUG("TAPION : file->read(buffer,8); ");
		file->read(buffer,8);
		if (file->fail()){
			PRT_DEBUG("TAPION : Read error");
			return;
		} else if (!memcmp(buffer,TapeHeader,8)) {
			PRT_DEBUG("TAPION : OK");
			R.nextIFF1=R.IFF1=R.IFF2=false;
			R.AF.B.l &= ~CPU::C_FLAG;
			return;
		} 
	} while (!file->fail());

	PRT_DEBUG("TAPION : No header found");
	//rewind the tape
	file->seekg(0, ios::beg);
}

void MSXTapePatch::TAPIN(CPU::CPURegs& R) const
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
	byte buffer;
	PRT_DEBUG("TAPIN");
	R.AF.B.l |= CPU::C_FLAG;

	if (file) {
		file->get(buffer);
		if (! file->fail()) {
			R.AF.B.h = buffer;
			R.AF.B.l &= ~CPU::C_FLAG; 
		}
	}
}

void MSXTapePatch::TAPIOF(CPU::CPURegs& R) const
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
	R.nextIFF1 = R.IFF1 = R.IFF2 = true;	// ei
}

void MSXTapePatch::TAPOON(CPU::CPURegs& R) const
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

	R.AF.B.l |= CPU::C_FLAG;

	if (file) {
		// again some stuff from fmsx about positioning
		int filePosition = (file->tellg() & 7);
		if (filePosition) {
			file->seekg(8-filePosition , ios::cur);
		}
		if (!file->fail()) { 
			file->write(TapeHeader,8);
			R.AF.B.l &= ~CPU::C_FLAG;
			R.nextIFF1 = R.IFF1 = R.IFF2 = false;	// di
		}   
	}
}

void MSXTapePatch::TAPOUT(CPU::CPURegs& R) const
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
	R.AF.B.l |= CPU::C_FLAG;

	if (file) {
		file->put(R.AF.B.h);
		R.AF.B.l &= ~CPU::C_FLAG;
	}
}

void MSXTapePatch::TAPOOF(CPU::CPURegs& R) const
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
	R.nextIFF1 = R.IFF1 = R.IFF2 = true;	// ei
}

void MSXTapePatch::STMOTR(CPU::CPURegs& R) const
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
