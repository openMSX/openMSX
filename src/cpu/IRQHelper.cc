// $Id$

#include "IRQHelper.hh"
#include "IRQHelper.ii"

IRQHelper::IRQHelper()
{
	cpu = MSXCPU::instance();
	request = false;
}

IRQHelper::~IRQHelper()
{
	reset();
}
