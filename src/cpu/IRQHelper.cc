// $Id$

#include "IRQHelper.hh"


IRQHelper::IRQHelper()
{
	cpu = MSXCPU::instance();
	request = false;
}

IRQHelper::~IRQHelper()
{
	reset();
}
