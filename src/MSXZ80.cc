// $Id$

#include "MSXZ80.hh"
#include "Scheduler.hh"
#include "iostream.h"

//#include "Z80.h"
#include "Z80IO.h"

CPU_Regs R;

MSXDevice *MSXZ80::Motherb; // place for static is reserved here!!

MSXZ80::MSXZ80(void)
{
//TODO: something usefull here ??
	cout << "instantiating an MSXZ80 object";
	CurrentCPUTime=0;
}
void MSXZ80::init(void)
{
	CurrentCPUTime=0;
	Z80_Running=1;
	InitTables();
	timeScaler=6;
	//TODO: timeScaler= getTimeMultiplier(3500000);
}

MSXZ80::~MSXZ80(void)
{
//TODO: something usefull here ??
	cout << "destructing an MSXZ80 object";
};

void MSXZ80::setTargetTStates(UINT64 TStates)
{
        if (TStates < CurrentCPUTime){
                cout << "ERROR The target CPU T-states is set to a lower value then the current CPU time";
                };
        TargetCPUTime=TStates;
};

void MSXZ80::executeUntilEmuTime(UINT64 TStates)
{
  setTargetTStates(TStates);
  while (CurrentCPUTime < TargetCPUTime){
	Z80_SingleInstruction();
	CurrentCPUTime+=timeScaler*R.ICount;
  }
};

/****************************************************************************/
/* Input a byte from given I/O port                                         */
/****************************************************************************/
byte Z80_In (byte Port)
{
    return MSXZ80::Motherb->readIO(Port,Scheduler::nowRunning->CurrentCPUTime);
};


/****************************************************************************/
/* Output a byte to given I/O port                                          */
/****************************************************************************/
void Z80_Out (byte Port,byte Value)
{
	 MSXZ80::Motherb->writeIO(Port,Value,Scheduler::nowRunning->CurrentCPUTime);
};
   
/****************************************************************************/
/* Read a byte from given memory location                                   */
/****************************************************************************/
//unsigned Z80_RDMEM(dword A);
byte Z80_RDMEM(word A)
{
#ifdef DEBUG
		debugmemory[A]=MSXZ80::Motherb->readMem(A,Scheduler::nowRunning->CurrentCPUTime);
		return debugmemory[A];
#endif
#ifndef DEBUG
	return  MSXZ80::Motherb->readMem(A,Scheduler::nowRunning->CurrentCPUTime);
#endif
};

/****************************************************************************/
/* Write a byte to given memory location                                    */
/****************************************************************************/
//void Z80_WRMEM(dword A,byte V);
void Z80_WRMEM(word A,byte V)
{
	 MSXZ80::Motherb->writeMem(A,V,Scheduler::nowRunning->CurrentCPUTime);
	 // No debugmemory[A] here otherwise self-modifying code could 
	 // alter the executing code before the disassembled opcode
	 // is printed;
};


