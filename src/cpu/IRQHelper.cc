// $Id$

#include "IRQHelper.hh"


namespace openmsx {

IRQHelper::IRQHelper()
	: request(false),
	  cpu(MSXCPU::instance())
{
}

IRQHelper::~IRQHelper()
{
	reset();
}

} // namespace openmsx
