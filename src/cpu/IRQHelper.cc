// $Id$

#include "IRQHelper.hh"

namespace openmsx {

IRQHelper::IRQHelper(MSXCPU& cpu_, bool nmi)
	: nmi(nmi)
	, request(false)
	, cpu(cpu_)
{
}

IRQHelper::~IRQHelper()
{
	reset();
}

} // namespace openmsx
