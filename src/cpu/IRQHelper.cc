// $Id$

#include "IRQHelper.hh"

namespace openmsx {

IRQHelper::IRQHelper(bool nmi)
	: nmi(nmi)
	, request(false)
	, cpu(MSXCPU::instance())
{
}

IRQHelper::~IRQHelper()
{
	reset();
}

} // namespace openmsx
