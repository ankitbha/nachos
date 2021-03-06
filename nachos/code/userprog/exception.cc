
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "console.h"
#include "synch.h"

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------
static Semaphore *readAvail;
static Semaphore *writeDone;
static void ReadAvail(int arg) { readAvail->V(); }
static void WriteDone(int arg) { writeDone->V(); }

static void ConvertIntToHex (unsigned v, Console *console)
{
   unsigned x;
   if (v == 0) return;
   ConvertIntToHex (v/16, console);
   x = v % 16;
   if (x < 10) {
      writeDone->P() ;
      console->PutChar('0'+x);
   }
   else {
      writeDone->P() ;
      console->PutChar('a'+x-10);
   }
}

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);
    int memval, vaddr, printval, tempval, exp, regnumber, regcontent, returnVal,err=0;
    unsigned printvalus;
	unsigned vpn; //For SysCall_GetPA
	unsigned sleeptime; //for SysCall_Sleep
	unsigned waitPid; // for SysCall_Join
    if (!initializedConsoleSemaphores) {
       readAvail = new Semaphore("read avail", 0);
       writeDone = new Semaphore("write done", 1);
       initializedConsoleSemaphores = true;
    }
    Console *console = new Console(NULL, NULL, ReadAvail, WriteDone, 0);;

    if ((which == SyscallException) && (type == SysCall_Halt)) {
	DEBUG('a', "Shutdown, initiated by user program.\n");
   	interrupt->Halt();
    }
    else if ((which == SyscallException) && (type == SysCall_PrintInt)) {
       printval = machine->ReadRegister(4);
       if (printval == 0) {
	  	  writeDone->P() ;
          console->PutChar('0');
       }
       else {
          if (printval < 0) {
	    	 writeDone->P() ;
             console->PutChar('-');
             printval = -printval;
          }
          tempval = printval;
          exp=1;
          while (tempval != 0) {
             tempval = tempval/10;
             exp = exp*10;
          }
          exp = exp/10;
          while (exp > 0) {
	     	 writeDone->P() ;
             console->PutChar('0'+(printval/exp));
             printval = printval % exp;
             exp = exp/10;
          }
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SysCall_PrintChar)) {
		writeDone->P() ;
        console->PutChar(machine->ReadRegister(4));   // echo it!
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SysCall_PrintString)) {
       vaddr = machine->ReadRegister(4);
       machine->ReadMem(vaddr, 1, &memval);
       while ((*(char*)&memval) != '\0') {
	  	  writeDone->P() ;
          console->PutChar(*(char*)&memval);
          vaddr++;
          machine->ReadMem(vaddr, 1, &memval);
       }
		//printf("Testing printing in printstr");
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SysCall_PrintIntHex)) {
       printvalus = (unsigned)machine->ReadRegister(4);
       writeDone->P() ;
       console->PutChar('0');
       writeDone->P() ;
       console->PutChar('x');
       if (printvalus == 0) {
          writeDone->P() ;
          console->PutChar('0');
       }
       else {
          ConvertIntToHex (printvalus, console);
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
	else if ((which == SyscallException) && ( type == SysCall_GetReg )) {
		regnumber = machine->ReadRegister(4);
		regcontent = machine->ReadRegister(regnumber);
		machine->WriteRegister(2,regcontent);
		// Advance program counters.
		machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
		machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
		machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
	} 
	else if ((which == SyscallException) && (type == SysCall_GetPA)) {
       vaddr = machine->ReadRegister(4);
       vpn = (vaddr*1.0/PageSize);
       if (vpn >= machine->pageTableSize) {
            err=1;
        } else if (!machine->KernelPageTable[vpn].valid) {
            err=1;
        }
       else if(machine->KernelPageTable[vpn].physicalPage >= NumPhysPages){
            err=1;
        }
       if(err==1)
               machine->WriteRegister(2,-1);
       else
               machine->WriteRegister(2,machine->KernelPageTable[vpn].physicalPage);
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
	else if ((which == SyscallException) && (type == SysCall_GetPID)) {
	   machine->WriteRegister(2,currentThread->GetPID());
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
	}
	else if ((which == SyscallException) && (type == SysCall_GetPPID)) {
	   machine->WriteRegister(2,currentThread->GetPPID());
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
	}
	else if ((which == SyscallException) && (type == SysCall_Time)) {
	   machine->WriteRegister(2,stats->totalTicks);
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
	}
	else if((which == SyscallException) && (type == SysCall_Yield)){
       currentThread->YieldCPU();
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
	else if((which == SyscallException) && (type == SysCall_Sleep)){
	   sleeptime = machine->ReadRegister(4);
	   //printf("%u",sleeptime);
	   if(sleeptime==0){
			//printf("Yielding");
			currentThread->YieldCPU();
 	   }
	   else{
			threadSleeping->SortedInsert((void *)currentThread,stats->totalTicks+sleeptime);
			//fprintf(stderr,"Put current thread to sleep \n");
			//printf("Inserted in sorted manner");
			IntStatus oldLevel = interrupt->SetLevel(IntOff);
			fprintf(stderr,"Putting current thread to sleep \n");
			currentThread->PutThreadToSleep();
			fprintf(stderr,"woke up current thread from sleep \n");
			//printf("Putting thread to sleep done");
			(void) interrupt->SetLevel(oldLevel);
	   }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
	}
	else if((which == SyscallException) && (type == SysCall_Join)){
		waitPid=machine->ReadRegister(4);
		if ((currentThread->searchChild(waitPid))==-1)
			returnVal=-1;
		else if ((currentThread->getChildStatus(waitPid))==0)
			returnVal=0;
		else {
			IntStatus oldLevel = interrupt->SetLevel(IntOff);
			currentThread->PutThreadToSleep();
			(void) interrupt->SetLevel(oldLevel);
			returnVal=currentThread->getChildStatus(waitPid);
		}
		machine->WriteRegister(2,returnVal);
		
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
	}
	else if((which == SyscallException) && (type == SysCall_Exit)){
	   
		//Halting if the current thread is the only process
		if(scheduler->isReadyListEmpty()){
			interrupt->Halt();
		}
		else{
			currentThread->parentThread->setChildStatus(currentThread->GetPID(),0);
			currentThread->FinishThread();
		}
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
	}/*
	else if((which == SyscallException) && (type == SysCall_NumInstr)){
	
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
       }*/



//----------------------------------------------------------------------------- MY WORK STARTS HERE ---------------------------------------------------------------------------------------------------------



        else if((which == SyscallException) && (type == Syscall_Fork)){

       // Advance program counters.
	machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
	
	// create a new thread
	NachOSThread* childthread = new NachOSThread("Forked child");

	// we will have to modify the thread to actually implement fork. For more details see the thread.cc in ../threads
	// The thread has adress space and pid and other things.

	childthread->parentThread = currerntThread;
	
	// we will have to define a new constructor so that we can copy parent's virtual space to child's virtual space.
	childthread->space = new AddressSpace(currentThread->sspace->numberofpages(), currentThread->space->physicaladdress());
	// we have new functions in addressspace.cc from where we get number of pages and physical address of the first page of the parent. 	
        
	currentThread->childCount++;
        currentThread->childState[currentThread->childCount] = 1;	// see if you want to add anything else to the list. Also node the change to void*. The list does not accept any data type in it. 	
	currentThread->childIds[currentThread->childCount] = childthread->getPID();

	
	machine->WriteRegister(2,0);  // writing 0 in reg $2 will make sure that child gets 0 as return value of the fork call.
	childthread->SaveUserState(); // we save this state for the child


	machine->WriteRegsister(2, childthread->getPID()); // then we change the state of the parent after that. It was clever ;)
	
	// Now we need to allocate stack to the child thread and also attach child thread to the ready queue. We will use threadfork syntax here. 
	

	childthread->CreateThreadStack(&FunctionFork, 0);


	IntStatus oldLevel = interrupt->SetLevel(IntOff);
	scheduler->MoveThreadToReadyQueue(childthread);
	(void) interrupt->SetLevel(oldLevel);

	}






//----------------------------------------------------------------------------it ends here--------------------------------------------------------------------------------------------------------------------


	else
	{
	printf("Unexpected user mode exception %d %d\n", which, type);
	ASSERT(FALSE);
    }
}
