// $Id$

#include <iostream>
#include <assert.h>

#include "MSXZ80.hh"
#include "Scheduler.hh"
#include "MSXMotherBoard.hh"
#include "Z80IO.h"


CPU_Regs R;

Emutime MSXZ80::currentCPUTime(3579545, 0);	// temporary hack for c/c++ mix


MSXZ80::MSXZ80()
{
	PRT_DEBUG("instantiating an MSXZ80 object");
}
MSXZ80::~MSXZ80()
{
	PRT_DEBUG("destructing an MSXZ80 object");
}

void MSXZ80::init()
{
	currentCPUTime(0);
	Z80_Running=1;
	InitTables();
}

void MSXZ80::reset()
{
	//TODO
}

void MSXZ80::IRQ(bool irq)
{
	Interrupt(Z80_NORM_INT);
}


void MSXZ80::setTargetTStates(const Emutime &time)
{
	assert (time >= currentCPUTime);
	targetCPUTime = time;
}

void MSXZ80::executeUntilEmuTime(const Emutime &time)
{
	setTargetTStates(time);
	while (currentCPUTime < targetCPUTime) {
		Z80_SingleInstruction();
		currentCPUTime+=R.ICount;
	}
};

/****************************************************************************/
/* Input a byte from given I/O port                                         */
/****************************************************************************/
byte Z80_In (byte port)
{
    return MSXMotherBoard::instance()->readIO(port, MSXZ80::currentCPUTime);
};


/****************************************************************************/
/* Output a byte to given I/O port                                          */
/****************************************************************************/
void Z80_Out (byte port,byte value)
{
	 MSXMotherBoard::instance()->writeIO(port, value, MSXZ80::currentCPUTime);
};
   
/****************************************************************************/
/* Read a byte from given memory location                                   */
/****************************************************************************/
byte Z80_RDMEM(word A)
{
#ifdef DEBUG
	debugmemory[A]=MSXMotherBoard::instance()->readMem(A, MSXZ80::currentCPUTime);
	return debugmemory[A];
#else
	return  MSXMotherBoard::instance()->readMem(A, MSXZ80::currentCPUTime);
#endif
};

/****************************************************************************/
/* Write a byte to given memory location                                    */
/****************************************************************************/
void Z80_WRMEM(word A,byte V)
{
	 MSXMotherBoard::instance()->writeMem(A,V, MSXZ80::currentCPUTime);
	 // No debugmemory[A] here otherwise self-modifying code could 
	 // alter the executing code before the disassembled opcode
	 // is printed;
};


