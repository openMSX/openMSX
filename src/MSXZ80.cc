// $Id$

#include <iostream>

#include "MSXZ80.hh"
#include "Scheduler.hh"
#include "MSXMotherBoard.hh"
#include <assert.h>

//#include "Z80.h"
#include "Z80IO.h"

CPU_Regs R;

MSXZ80::MSXZ80(void) : currentCPUTime(3579545, 0), targetCPUTime(3579545, 0)
{
	PRT_DEBUG("instantiating an MSXZ80 object");
}
void MSXZ80::init(void)
{
	MSXDevice::init();
	currentCPUTime(0);
	Z80_Running=1;
	InitTables();
	MSXMotherBoard::instance()->setActiveCPU(this);
}

MSXZ80::~MSXZ80(void)
{
	PRT_DEBUG("destructing an MSXZ80 object");
};

void MSXZ80::setTargetTStates(const Emutime &time)
{
	assert (time >= currentCPUTime);
        targetCPUTime = time;
};

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
byte Z80_In (byte Port)
{
    return MSXMotherBoard::instance()->readIO(Port,Scheduler::nowRunning->currentCPUTime);
};


/****************************************************************************/
/* Output a byte to given I/O port                                          */
/****************************************************************************/
void Z80_Out (byte Port,byte Value)
{
	 MSXMotherBoard::instance()->writeIO(Port,Value,Scheduler::nowRunning->currentCPUTime);
};
   
/****************************************************************************/
/* Read a byte from given memory location                                   */
/****************************************************************************/
//unsigned Z80_RDMEM(dword A);
byte Z80_RDMEM(word A)
{
#ifdef DEBUG
	debugmemory[A]=MSXMotherBoard::instance()->readMem(A,Scheduler::nowRunning->currentCPUTime);
	return debugmemory[A];
#else
	return  MSXMotherBoard::instance()->readMem(A,Scheduler::nowRunning->currentCPUTime);
#endif
};

/****************************************************************************/
/* Write a byte to given memory location                                    */
/****************************************************************************/
//void Z80_WRMEM(dword A,byte V);
void Z80_WRMEM(word A,byte V)
{
	 MSXMotherBoard::instance()->writeMem(A,V,Scheduler::nowRunning->currentCPUTime);
	 // No debugmemory[A] here otherwise self-modifying code could 
	 // alter the executing code before the disassembled opcode
	 // is printed;
};


