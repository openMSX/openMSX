
#include <assert.h>
#include "MSXCPU.hh"


MSXCPU::MSXCPU()
{
	PRT_DEBUG("Creating an MSXCPU object");
	z80 = new MSXZ80();
	//r800 = new MSXR800();
}

MSXCPU::~MSXCPU()
{
	PRT_DEBUG("Destroying an MSXCPU object");
	delete z80;
	//delete r800;
}

MSXCPU* MSXCPU::instance()
{
	if (oneInstance == NULL ) {
		oneInstance = new MSXCPU();
	}
	return oneInstance;
}
MSXCPU* MSXCPU::oneInstance = NULL;


void MSXCPU::init()
{
	MSXDevice::init();
	setActiveCPU(CPU_Z80);
	z80->init();
	//r800->init();
}

void MSXCPU::reset()
{
	MSXDevice::reset();
	z80->reset();
	//r800->reset();
}


void MSXCPU::setActiveCPU(int cpu)
{
	switch (cpu) {
	case CPU_Z80:
		activeCPU = z80;
		break;
//	case CPU_R800:
//		activeCPU = r800;
//		break;
	default:
		assert(false);
	}
}

void MSXCPU::IRQ(bool irq)
{
	activeCPU->IRQ(irq);
}

void MSXCPU::executeUntilEmuTime(const Emutime &time)
{
	activeCPU->executeUntilEmuTime(time);
}
