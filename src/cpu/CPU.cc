// $Id$

#include "CPU.hh"
#include "serialize.hh"
#include <cassert>

namespace openmsx {

byte CPU::ZSTable[256];
byte CPU::ZSXYTable[256];
byte CPU::ZSPTable[256];
byte CPU::ZSPXYTable[256];
byte CPU::ZSPHTable[256];
word CPU::DAATable[0x800];


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
		ZSTable   [i] = zFlag | sFlag;
		ZSXYTable [i] = zFlag | sFlag | xFlag | yFlag;
		ZSPTable  [i] = zFlag | sFlag |                 vFlag;
		ZSPXYTable[i] = zFlag | sFlag | xFlag | yFlag | vFlag;
		ZSPHTable [i] = zFlag | sFlag |                 vFlag | H_FLAG;
	}
	assert(ZSTable   [  0] == ZS0);
	assert(ZSXYTable [  0] == ZSXY0);
	assert(ZSPTable  [  0] == ZSP0);
	assert(ZSPXYTable[  0] == ZSPXY0);
	assert(ZSTable   [255] == ZS255);
	assert(ZSXYTable [255] == ZSXY255);

	for (int x = 0; x < 0x800; ++x) {
		bool hf = (x & 0x400) != 0;
		bool nf = (x & 0x200) != 0;
		bool cf = (x & 0x100) != 0;
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
}

void CPU::setPaused(bool paused)
{
	getRegisters().setExtHALT(paused);
	exitCPULoopSync();
}


// version 1: Initial version.
// version 2: Replaced 'afterEI' boolean with 'after' byte
//            (holds both afterEI and afterLDAI information).
template<typename Archive>
void CPU::CPURegs::serialize(Archive& ar, unsigned version)
{
	ar.serialize("af",  AF_.w);
	ar.serialize("bc",  BC_.w);
	ar.serialize("de",  DE_.w);
	ar.serialize("hl",  HL_.w);
	ar.serialize("af2", AF2_.w);
	ar.serialize("bc2", BC2_.w);
	ar.serialize("de2", DE2_.w);
	ar.serialize("hl2", HL2_.w);
	ar.serialize("ix",  IX_.w);
	ar.serialize("iy",  IY_.w);
	ar.serialize("pc",  PC_.w);
	ar.serialize("sp",  SP_.w);
	ar.serialize("i",   I_);
	byte r = getR();
	ar.serialize("r",   r);  // combined R_ and R2_
	if (ar.isLoader()) setR(r);
	ar.serialize("im",  IM_);
	ar.serialize("iff1", IFF1_);
	ar.serialize("iff2", IFF2_);
	if (ar.versionBelow(version, 2)) {
		bool afterEI = false; // initialize to avoid warning
		ar.serialize("afterEI", afterEI);
		clearAfter();
		if (afterEI) setAfterEI();
	} else {
		ar.serialize("after", after_);
	}
	ar.serialize("halt", HALT_);
}
INSTANTIATE_SERIALIZE_METHODS(CPU::CPURegs);

} // namespace openmsx

