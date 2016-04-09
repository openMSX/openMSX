#include "CPURegs.hh"
#include "serialize.hh"

namespace openmsx {

// version 1: Initial version.
// version 2: Replaced 'afterEI' boolean with 'after' byte
//            (holds both afterEI and afterLDAI information).
template<typename Archive>
void CPURegs::serialize(Archive& ar, unsigned version)
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
		clearPrevious();
		if (afterEI) setCurrentEI();
		endInstruction();
	} else if (ar.versionBelow(version, 3)) {
		byte after = 0;
		ar.serialize("after", after);
		clearPrevious();
		prev_ = after; // flags still in same position
		endInstruction();
	} else {
		ar.serialize("previous", prev_);
	}

	ar.serialize("halt", HALT_);
}
INSTANTIATE_SERIALIZE_METHODS(CPURegs);

} // namespace openmsx
