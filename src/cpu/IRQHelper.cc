// $Id$

#include "IRQHelper.hh"


namespace openmsx {

IRQHelper::IRQHelper()
{
	cpu = MSXCPU::instance();
	request = false;
}

IRQHelper::~IRQHelper()
{
	reset();
}

} // namespace openmsx
