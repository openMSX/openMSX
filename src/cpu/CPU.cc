// $Id$

#include "CPU.hh"

namespace openmsx {

byte CPU::ZSTable[256];
byte CPU::ZSXYTable[256];
byte CPU::ZSPXYTable[256];
byte CPU::ZSPTable[256];
word CPU::DAATable[0x800];

CPU::BreakPoints CPU::breakPoints;
bool CPU::breaked = false;
bool CPU::continued = false;
bool CPU::step = false;
bool CPU::paused = false;

word CPU::start_pc;


CPU::CPU()
{
	// Avoid initialising twice.
	static bool alreadyInit = false;
	if (alreadyInit) return;
	alreadyInit = true;

	for (int i = 0; i < 256; ++i) {
		byte zFlag = (i == 0) ? Z_FLAG : 0;
		byte sFlag = i & S_FLAG;
		byte xFlag = i & X_FLAG;
		byte yFlag = i & Y_FLAG;
		byte vFlag = V_FLAG;
		for (int v = 128; v != 0; v >>= 1) {
			if (i & v) vFlag ^= V_FLAG;
		}
		ZSTable[i]    = zFlag | sFlag;
		ZSXYTable[i]  = zFlag | sFlag | xFlag | yFlag;
		ZSPXYTable[i] = zFlag | sFlag | xFlag | yFlag | vFlag;
		ZSPTable[i]   = zFlag | sFlag |                 vFlag;
	}

	for (int x = 0; x < 0x800; ++x) {
		bool nf = x & 0x400;
		bool hf = x & 0x200;
		bool cf = x & 0x100;
		byte a = x & 0xFF;
		byte hi = a / 16;
		byte lo = a & 15;
		byte diff;
		if (cf) {
			diff = ((lo <= 9) && !hf) ? 0x60 : 0x66;
		} else {
			if (lo >= 10) {
				diff = (hi <= 8) ? 0x06 : 0x66;
			} else {
				if (hi >= 10) {
					diff = hf ? 0x66 : 0x60;
				} else {
					diff = hf ? 0x06 : 0x00;
				}
			}
		}
		byte res_a = nf ? a - diff : a + diff;
		byte res_f = ZSPXYTable[res_a] | (nf ? N_FLAG : 0);
		if (cf || ((lo <= 9) ? (hi >= 10) : (hi >= 9))) {
			res_f |= C_FLAG;
		}
		if (nf ? (hf && (lo <= 5)) : (lo >= 10)) {
			res_f |= H_FLAG;
		}
		DAATable[x] = (res_a << 8) + res_f;
	}
}

CPU::~CPU()
{
	// TODO deleting bp still crashes for some reason, I suspect
	// destruction order of MSXCPU and Interpreter. MSXCPU shouldn't
	// be a singleton anyway.
	for (BreakPoints::const_iterator it = breakPoints.begin();
	     it != breakPoints.end(); ++it) {
		//delete it->second;
	}
}

void CPU::insertBreakPoint(std::auto_ptr<BreakPoint> bp_)
{
	BreakPoint* bp = bp_.release();
	breakPoints.insert(std::make_pair(bp->getAddress(), bp));
	exitCPULoop();
}

void CPU::removeBreakPoint(const BreakPoint& bp)
{
	std::pair<BreakPoints::iterator, BreakPoints::iterator> range =
		breakPoints.equal_range(bp.getAddress());
	for (BreakPoints::iterator it = range.first; it != range.second; ++it) {
		if (it->second == &bp) {
			delete &bp;
			breakPoints.erase(it);
			break;
		}
	}
	exitCPULoop();
}

const CPU::BreakPoints& CPU::getBreakPoints() const
{
	return breakPoints;
}

void CPU::setPaused(bool paused_)
{
	paused = paused_;
	exitCPULoop();
}

} // namespace openmsx

