#include "CPU.hh"
#include "serialize.hh"

namespace openmsx {

byte CPU::ZSTable[256];
byte CPU::ZSXYTable[256];
byte CPU::ZSPTable[256];
byte CPU::ZSPXYTable[256];
byte CPU::ZSPHTable[256];


CPU::CPU(bool r800)
	: R(r800)
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

	assert(isSameAfter());
	if (ar.versionBelow(version, 2)) {
		bool afterEI = false; // initialize to avoid warning
		ar.serialize("afterEI", afterEI);
		clearNextAfter();
		if (afterEI) setAfterEI();
	} else {
		ar.serialize("after", afterNext_);
	}
	copyNextAfter();

	ar.serialize("halt", HALT_);
}
INSTANTIATE_SERIALIZE_METHODS(CPU::CPURegs);

} // namespace openmsx
