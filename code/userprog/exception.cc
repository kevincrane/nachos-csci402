// exception.cc 
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
#include "../threads/synch.h"
#include <stdio.h>
#include <iostream>

using namespace std;

#define MAX_LOCKS 1000
#define MAX_CVS 1000

// To hold all of the data about a created lock
struct lockData{
  Lock* lock;                
  AddrSpace *space;        // AddrSpace of the owning process
  bool isDeleted;          // If the lock has been deleted
  bool toBeDeleted;        // If the lock is supposed to be deleted once all active threads finish
  int numActiveThreads;    // Number of threads waiting on or using the lock
};

lockData locks[MAX_LOCKS];                // Array of all locks used by user programs
Lock* lockArray = new Lock("lockArray");  // To lock the array of locks
int nextLockPos = 0;                      // Next position available in the array of locks

// To hold all of the data about a created condition
struct cvData{
  Condition* condition;
  AddrSpace *space;         // AddrSpace of the owning process
  bool isDeleted;           // If the condition has been deleted
  bool toBeDeleted;         // If the condition is supposed to be deleted once all active threads finish
  int numActiveThreads;     // Number of threads waiting on a condition
};

cvData conditions[MAX_CVS];               // Array of conditions used by user programs
Lock* cvArray = new Lock("CVArray");      // To lock the array of conditions
int nextCVPos = 0;                        // Next position available in the array

int copyin(unsigned int vaddr, int len, char *buf) {
    // Copy len bytes from the current thread's virtual address vaddr.
    // Return the number of bytes so read, or -1 if an error occors.
    // Errors can generally mean a bad virtual address was passed in.
    bool result;
    int n=0;			// The number of bytes copied in
    int *paddr = new int;

    while ( n >= 0 && n < len) {
      result = machine->ReadMem( vaddr, 1, paddr );
      while(!result) // FALL 09 CHANGES
	  {
   			result = machine->ReadMem( vaddr, 1, paddr ); // FALL 09 CHANGES: TO HANDLE PAGE FAULT IN THE ReadMem SYS CALL
	  }	
      
      buf[n++] = *paddr;
     
      if ( !result ) {
	//translation failed
	return -1;
      }

      vaddr++;
    }

    delete paddr;
    return len;
}

int copyout(unsigned int vaddr, int len, char *buf) {
    // Copy len bytes to the current thread's virtual address vaddr.
    // Return the number of bytes so written, or -1 if an error
    // occors.  Errors can generally mean a bad virtual address was
    // passed in.
    bool result;
    int n=0;			// The number of bytes copied in

    while ( n >= 0 && n < len) {
      // Note that we check every byte's address
      result = machine->WriteMem( vaddr, 1, (int)(buf[n++]) );

      if ( !result ) {
	//translation failed
	return -1;
      }

      vaddr++;
    }

    return n;
}

void Create_Syscall(unsigned int vaddr, int len) {
    // Create the file with the name in the user buffer pointed to by
    // vaddr.  The file name is at most MAXFILENAME chars long.  No
    // way to return errors, though...
    char *buf = new char[len+1];	// Kernel buffer to put the name in

    if (!buf) return;

    if( copyin(vaddr,len,buf) == -1 ) {
	printf("%s","Bad pointer passed to Create\n");
	delete buf;
	return;
    }

    buf[len]='\0';

    fileSystem->Create(buf,0);
    delete[] buf;
    return;
}

int Open_Syscall(unsigned int vaddr, int len) {
    // Open the file with the name in the user buffer pointed to by
    // vaddr.  The file name is at most MAXFILENAME chars long.  If
    // the file is opened successfully, it is put in the address
    // space's file table and an id returned that can find the file
    // later.  If there are any errors, -1 is returned.
    char *buf = new char[len+1];	// Kernel buffer to put the name in
    OpenFile *f;			// The new open file
    int id;				// The openfile id

    if (!buf) {
	printf("%s","Can't allocate kernel buffer in Open\n");
	return -1;
    }

    if( copyin(vaddr,len,buf) == -1 ) {
	printf("%s","Bad pointer passed to Open\n");
	delete[] buf;
	return -1;
    }

    buf[len]='\0';

    f = fileSystem->Open(buf);
    delete[] buf;

    if ( f ) {
	if ((id = currentThread->space->fileTable.Put(f)) == -1 )
	    delete f;
	return id;
    }
    else
	return -1;
}

void Write_Syscall(unsigned int vaddr, int len, int id) {
    // Write the buffer to the given disk file.  If ConsoleOutput is
    // the fileID, data goes to the synchronized console instead.  If
    // a Write arrives for the synchronized Console, and no such
    // console exists, create one. For disk files, the file is looked
    // up in the current address space's open file table and used as
    // the target of the write.
    
    char *buf;		// Kernel buffer for output
    OpenFile *f;	// Open file for output

    if ( id == ConsoleInput) return;
    
    if ( !(buf = new char[len]) ) {
	printf("%s","Error allocating kernel buffer for write!\n");
	return;
    } else {
        if ( copyin(vaddr,len,buf) == -1 ) {
	    printf("%s","Bad pointer passed to to write: data not written\n");
	    delete[] buf;
	    return;
	}
    }

    if ( id == ConsoleOutput) {
      for (int ii=0; ii<len; ii++) {
	printf("%c",buf[ii]);
      }

    } else {
	if ( (f = (OpenFile *) currentThread->space->fileTable.Get(id)) ) {
	    f->Write(buf, len);
	} else {
	    printf("%s","Bad OpenFileId passed to Write\n");
	    len = -1;
	}
    }

    delete[] buf;
}

int Read_Syscall(unsigned int vaddr, int len, int id) {
    // Write the buffer to the given disk file.  If ConsoleOutput is
    // the fileID, data goes to the synchronized console instead.  If
    // a Write arrives for the synchronized Console, and no such
    // console exists, create one.    We reuse len as the number of bytes
    // read, which is an unnessecary savings of space.
    char *buf;		// Kernel buffer for input
    OpenFile *f;	// Open file for output

    if ( id == ConsoleOutput) return -1;
    
    if ( !(buf = new char[len]) ) {
	printf("%s","Error allocating kernel buffer in Read\n");
	return -1;
    }

    if ( id == ConsoleInput) {
      //Reading from the keyboard
      scanf("%s", buf);

      if ( copyout(vaddr, len, buf) == -1 ) {
	printf("%s","Bad pointer passed to Read: data not copied\n");
      }
    } else {
	if ( (f = (OpenFile *) currentThread->space->fileTable.Get(id)) ) {
	    len = f->Read(buf, len);
	    if ( len > 0 ) {
	        //Read something from the file. Put into user's address space
  	        if ( copyout(vaddr, len, buf) == -1 ) {
		    printf("%s","Bad pointer passed to Read: data not copied\n");
		}
	    }
	} else {
	    printf("%s","Bad OpenFileId passed to Read\n");
	    len = -1;
	}
    }

    delete[] buf;
    return len;
}

void Close_Syscall(int fd) {
    // Close the file associated with id fd.  No error reporting.
    OpenFile *f = (OpenFile *) currentThread->space->fileTable.Remove(fd);

    if ( f ) {
      delete f;
    } else {
      printf("%s","Tried to close an unopen file\n");
    }
}

<<<<<<< HEAD
=======
<<<<<<< HEAD
<<<<<<< HEAD
=======
>>>>>>> fcb1815aca4dea4f8b5ecae7d946aafd95e2c4cc
>>>>>>> 775a692fad1ca094b90219b1c5c731a6ac88feeb
// Fork Syscall. Fork new thread from pointer to void function
void Fork_Syscall(int vAddress)
{
  /*
  // TODO: check for max threads, whether vAddress is outside size of page table
  char* name = currentThread->space->getProcessName();
  Thread *t=new Thread(name);
  
<<<<<<< HEAD
  int num = currentThread->space->threadTable->Put(t);             // add new thread to thread table
=======
  int num = currentThread->space->threadTable.Put(t);             // add new thread to thread table
>>>>>>> 775a692fad1ca094b90219b1c5c731a6ac88feeb
  int processID = currentThread->space->getProcessID();
  sprintf(name, "%s%d", name, num);
  
<<<<<<< HEAD
=======
  Thread *t=new Thread(name);
>>>>>>> 775a692fad1ca094b90219b1c5c731a6ac88feeb
  t->setThreadNum(num);
  t->setProcessID(processID);
  t->space = currentThread->space;      // Set new thread to currentThread's address space
  
  // Finally Fork a new kernel thread 
  t->Fork((VoidFunctionPtr)newKernelThread, vAddress);
  */
  DEBUG('p', "Called Fork syscall");
  currentThread->space->addThread(vAddress);
}

<<<<<<< HEAD
=======
<<<<<<< HEAD
=======
=======
>>>>>>> fcb1815aca4dea4f8b5ecae7d946aafd95e2c4cc
>>>>>>> 775a692fad1ca094b90219b1c5c731a6ac88feeb
void Acquire_Syscall(int lockIndex) {
  printf("Entered Acquire_Syscall.\n");
  
  lockArray->Acquire();

  // Check that the index is valid
  if(lockIndex < 0 || lockIndex >= nextLockPos) {
    printf("ERROR: The given lock index is not valid.\n");
    lockArray->Release();
    return;
  } 

  // Check that the lock belongs to the calling process
  else if (locks[lockIndex].space != currentThread->space) {
    printf("ERROR: This process does not own the lock!\n");
    lockArray->Release();
    return;
  } 

  // Check that the lock has not been deleted
  else if (locks[lockIndex].isDeleted) {
    printf("ERROR: This lock has been deleted.\n");
    lockArray->Release();
    return;
  } 

  // Check that the lock is not to be deleted
  else if (locks[lockIndex].toBeDeleted) {
    printf("ERROR: This lock is to be deleted.\n");
    lockArray->Release();
    return;
  }
  locks[lockIndex].numActiveThreads++; // A new thread is using this lock
  locks[lockIndex].lock->Acquire();
  lockArray->Release();
}

void Release_Syscall(int lockIndex) {
  printf("Entered Release_Syscall.\n");

  lockArray->Acquire();

  // Check that the index is valid
  if(lockIndex < 0 || lockIndex >= nextLockPos) {
    printf("ERROR: The given lock index is not valid.\n");
    lockArray->Release();
    return;
  } 

  // Check that the lock belongs to the calling process
  else if (locks[lockIndex].space != currentThread->space) {
    printf("ERROR: This process does not own the lock!\n");
    lockArray->Release();
    return;
  } 

  // Check that the lock has not been deleted
  else if (locks[lockIndex].isDeleted) {
    printf("ERROR: This lock has been deleted.\n");
    lockArray->Release();
    return;
  }
  locks[lockIndex].lock->Release();
  locks[lockIndex].numActiveThreads--; // A thread is no longer using this lock

  // Check if the lock was waiting to be deleted, if so, delete it
  if(locks[lockIndex].toBeDeleted && (locks[lockIndex].numActiveThreads == 0)) {
    locks[lockIndex].isDeleted = true;
    printf("Deleted lock %i.\n", lockIndex);
  }

  lockArray->Release();
}

<<<<<<< HEAD
=======
<<<<<<< HEAD
void Fork_Syscall() {
  printf("Entered Fork_Syscall.\n");
}
=======
>>>>>>> fcb1815aca4dea4f8b5ecae7d946aafd95e2c4cc

>>>>>>> 775a692fad1ca094b90219b1c5c731a6ac88feeb
void Exec_Syscall() {
  printf("Entered Exec_Syscall.\n");
}

void Exit_Syscall() {
  printf("Entered Exit_Syscall.\n");
<<<<<<< HEAD
=======
<<<<<<< HEAD
=======
>>>>>>> 775a692fad1ca094b90219b1c5c731a6ac88feeb

  // Last executing thread in the last process
  interrupt->Halt();

  // Thread in a process, not the last executing thread in the process, nor is
  // it the last process

  // Release stack memory pages for this thread, only the pages where the valid
  // bit is true

  // Last executing thread in a process - not the last process

  // Clear out all valid entries in the page table

<<<<<<< HEAD
  // Clear out all locks/cvs for that process  
=======
  // Clear out all locks/cvs for that process
  
>>>>>>> fcb1815aca4dea4f8b5ecae7d946aafd95e2c4cc
>>>>>>> 775a692fad1ca094b90219b1c5c731a6ac88feeb
}

void Yield_Syscall() {
  printf("Entered Yield_Syscall.\n");
  currentThread->Yield();
}

void Wait_Syscall(int cvIndex, int lockIndex) {
  printf("Entered Wait_Syscall.\n");
  cvArray->Acquire();
  lockArray->Acquire();

  // Check that the index is valid
  if(cvIndex < 0 || cvIndex >= nextCVPos) {
    printf("ERROR: The given cv index is not valid.\n");
    cvArray->Release();
    lockArray->Release();
    return;
  } 

  // Check that the condition belongs to the calling thread
  else if (conditions[cvIndex].space != currentThread->space) {
    printf("ERROR: This process does not own the condition!\n");
    cvArray->Release();
    lockArray->Release();
    return;
  } 

  // Check that the lock index is valid
  else if (lockIndex < 0 || lockIndex >= nextLockPos) {
    printf("ERROR: The given lock index is not valid.\n");
    cvArray->Release();
    lockArray->Release();
    return;
  } 

  // Check that the lock belongs to the calling thread
  else if (locks[lockIndex].space != currentThread->space) {
    printf("ERROR: This process does not own the lock!\n");
    cvArray->Release();
    lockArray->Release();
    return;
  } 

  // Check that the condition has not been deleted
  else if (conditions[cvIndex].isDeleted) {
    printf("ERROR: This condition has been deleted.\n");
    cvArray->Release();
    lockArray->Release();
    return;
  } 

  // Check that the condition is not to be deleted
  else if (conditions[cvIndex].toBeDeleted) {
    printf("ERROR: This condition is to be deleted.\n");
    cvArray->Release();
    lockArray->Release();
    return;
  } 

  // Check that the lock is not deleted
  else if (locks[lockIndex].isDeleted) {
    printf("ERROR: This lock has been deleted.\n");
    cvArray->Release();
    lockArray->Release();
    return;
  } 

  // Check that the lock is not to be deleted
  else if (locks[lockIndex].toBeDeleted) {
    printf("ERROR: This lock is to be deleted.\n");
    cvArray->Release();
    lockArray->Release();
    return;
  }
  conditions[cvIndex].numActiveThreads++;   // Add a thread using the condition
  conditions[cvIndex].condition->Wait(locks[lockIndex].lock);
  cvArray->Release();
  lockArray->Release();
}

void Signal_Syscall(int cvIndex, int lockIndex) {
  printf("Entered Signal_Syscall.\n");
  
  cvArray->Acquire();
  lockArray->Acquire();

  // Check that the index is valid
  if(cvIndex < 0 || cvIndex >= nextCVPos) {
    printf("ERROR: The given cv index is not valid.\n");
    cvArray->Release();
    lockArray->Release();
    return;
  } 

  // Check that the condition belongs to the calling process
  else if (conditions[cvIndex].space != currentThread->space) {
    printf("ERROR: This process does not own the condition!\n");
    cvArray->Release();
    lockArray->Release();
    return;
  } 

  // Check that the lock index is valid
  else if (lockIndex < 0 || lockIndex >= nextLockPos) {
    printf("ERROR: The given lock index is not valid.\n");
    cvArray->Release();
    lockArray->Release();
    return;
  } 

  // Check that the lock belongs to the calling process
  else if (locks[lockIndex].space != currentThread->space) {
    printf("ERROR: This process does not own the lock!\n");
    cvArray->Release();
    lockArray->Release();
    return;
  } 

  // Check that the condition has not been deleted
  else if (conditions[cvIndex].isDeleted) {
    printf("ERROR: This condition has been deleted.\n");
    cvArray->Release();
    lockArray->Release();
    return;
  } 

  // Check that the lock has not been deleted
  else if (locks[lockIndex].isDeleted) {
    printf("ERROR: This lock has been deleted.\n");
    cvArray->Release();
    lockArray->Release();
    return;
  } 
<<<<<<< HEAD

  // TODO: SWITCH THESE?
=======
<<<<<<< HEAD
=======
  // SWITCH THESE?
>>>>>>> fcb1815aca4dea4f8b5ecae7d946aafd95e2c4cc
>>>>>>> 775a692fad1ca094b90219b1c5c731a6ac88feeb
  conditions[cvIndex].numActiveThreads--; // One less thread using the condition
  conditions[cvIndex].condition->Signal(locks[lockIndex].lock);

  // Check if the condition is to be deleted, if so, delete it
  if(conditions[cvIndex].toBeDeleted && (conditions[cvIndex].numActiveThreads == 0)) {
    conditions[cvIndex].isDeleted = true;
    printf("Deleted condition %i.\n", cvIndex);
  }
  cvArray->Release();
  lockArray->Release();
}

void Broadcast_Syscall(int cvIndex, int lockIndex) {
  printf("Entered Broadcast_Syscall.\n");
 
  cvArray->Acquire();
  lockArray->Acquire();

  // Check that the index is valid
  if(cvIndex < 0 || cvIndex >= nextCVPos) {
    printf("ERROR: The given cv index is not valid.\n");
    cvArray->Release();
    lockArray->Release();
    return;
  } 

  // Check that the condition belongs to the calling process
  else if (conditions[cvIndex].space != currentThread->space) {
    printf("ERROR: This process does not own the condition!\n");
    cvArray->Release();
    lockArray->Release();
    return;
  } 

  // Check that the lock index is valid
  else if (lockIndex < 0 || lockIndex >= nextLockPos) {
    printf("ERROR: The given lock index is not valid.\n");
    cvArray->Release();
    lockArray->Release();
    return;
  } 

  // Check that the lock belongs to the calling process
  else if (locks[lockIndex].space != currentThread->space) {
    printf("ERROR: This process does not own the lock!\n");
    cvArray->Release();
    lockArray->Release();
    return;
  } 

  // Check that the condition has not been deleted
  else if (conditions[cvIndex].isDeleted) {
    printf("ERROR: This condition has been deleted.\n");
    cvArray->Release();
    lockArray->Release();
    return;
  } 

  // Check that the lock has not been deleted
  else if (locks[lockIndex].isDeleted) {
    printf("ERROR: This lock has been deleted.\n");
    cvArray->Release();
    lockArray->Release();
    return;
  }  
  conditions[cvIndex].condition->Broadcast(locks[lockIndex].lock);
  conditions[cvIndex].numActiveThreads = 0;

  // Check if the condition is to be deleted, if so, delete it
  if(conditions[cvIndex].toBeDeleted && (conditions[cvIndex].numActiveThreads == 0)) {
    conditions[cvIndex].isDeleted = true;
    printf("Deleted condition %i.\n", cvIndex);
  }
  cvArray->Release();
  lockArray->Release();
}

int CreateLock_Syscall() {
  printf("Entered CreateLock_Syscall.\n");
  lockData myLock;
  myLock.lock = new Lock("L");
  myLock.space = currentThread->space;
  myLock.isDeleted = false;
  myLock.toBeDeleted = false;
  myLock.numActiveThreads = 0;

  lockArray->Acquire();

  locks[nextLockPos] = myLock;
  nextLockPos++;
  
  lockArray->Release();

  return (nextLockPos-1);
}

void DestroyLock_Syscall(int lockIndex) {
  printf("Entered DestroyLock_Syscall.\n");
  
  lockArray->Acquire();

  // Check that the index is valid
  if(lockIndex < 0 || lockIndex >= nextLockPos) {
    printf("ERROR: The given lock index is not valid.\n");
    lockArray->Release();
    return;
  } 

  // Check that the lock belongs to the calling process
  else if (locks[lockIndex].space != currentThread->space) {
    printf("ERROR: This process does not own the lock!\n");
    lockArray->Release();
    return;
  } 

  // Check that the lock hasn't been deleted
  else if (locks[lockIndex].isDeleted) {
    printf("ERROR: This lock has already been deleted.\n");
    lockArray->Release();
    return;
  } 

  // Check that the lock isn't already to be deleted
  else if (locks[lockIndex].toBeDeleted) {
    lockArray->Release();
    return;
  } 

  // Check if there are still threads using the lock
  else if(locks[lockIndex].numActiveThreads > 0) {
    locks[lockIndex].toBeDeleted = true;
  } else {
    locks[lockIndex].isDeleted = true;
    printf("Deleted lock %i.\n", lockIndex);
  }
  lockArray->Release();
  return;
}

int CreateCondition_Syscall() {
  printf("Entered CreateCondition_Syscall.\n");
  cvData myCV;
  myCV.condition = new Condition("C");
  myCV.space = currentThread->space;
  myCV.isDeleted = false;
  myCV.toBeDeleted = false;
  myCV.numActiveThreads = 0;

  cvArray->Acquire();

  conditions[nextCVPos] = myCV;
  nextCVPos++;
  
  cvArray->Release();

  return (nextCVPos-1);
  return 0;
}

void DestroyCondition_Syscall(int cvIndex) {
  printf("Entered DestroyCondition_Syscall.\n");
  cvArray->Acquire();

  // Check that the index is valid
  if(cvIndex < 0 || cvIndex >= nextCVPos) {
    printf("ERROR: The given condition index is not valid.\n");
    cvArray->Release();
    return;
  } 

  // Check that the condition belongs to the calling thread
  else if (conditions[cvIndex].space != currentThread->space) {
    printf("ERROR: This process does not own the cv!\n");
    cvArray->Release();
    return;
  } 

  // Check that the condition hasn't already been deleted
  else if (conditions[cvIndex].isDeleted) {
    printf("ERROR: This lock has already been deleted.\n");
    cvArray->Release();
    return;
  } 

  // Check that the condition isn't already set to tobeDeleted
  else if (conditions[cvIndex].toBeDeleted) {
    cvArray->Release();
    return;
  } 

  // Check that there are no more active threads
  else if(conditions[cvIndex].numActiveThreads > 0) {
    conditions[cvIndex].toBeDeleted = true;
  } else {
    conditions[cvIndex].isDeleted = true;
    printf("Deleted condition %i.\n", cvIndex);
  }
  cvArray->Release();
  return;
}
<<<<<<< HEAD
=======
<<<<<<< HEAD
>>>>>>> b303e463ba81ff0dade2980204631e1b363e9d4b
=======
>>>>>>> fcb1815aca4dea4f8b5ecae7d946aafd95e2c4cc
>>>>>>> 775a692fad1ca094b90219b1c5c731a6ac88feeb


// Fatty exception handler
void ExceptionHandler(ExceptionType which) {
    int type = machine->ReadRegister(2); // Which syscall?
    int rv=0; 	// the return value from a syscall

    if ( which == SyscallException ) {
      switch (type) {
      default:
        DEBUG('a', "Unknown syscall - shutting down.\n");
      case SC_Halt:
        DEBUG('a', "Shutdown, initiated by user program.\n");
        interrupt->Halt();
      break;
      case SC_Create:
        DEBUG('a', "Create syscall.\n");
        Create_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
      break;

      case SC_Open:
        DEBUG('a', "Open syscall.\n");
        rv = Open_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
      break;
      case SC_Write:
        DEBUG('a', "Write syscall.\n");
        Write_Syscall(machine->ReadRegister(4),
            machine->ReadRegister(5),
            machine->ReadRegister(6));
      break;
      case SC_Read:
        DEBUG('a', "Read syscall.\n");
        rv = Read_Syscall(machine->ReadRegister(4),
            machine->ReadRegister(5),
            machine->ReadRegister(6));
      break;
      case SC_Close:
        DEBUG('a', "Close syscall.\n");
        Close_Syscall(machine->ReadRegister(4));
      break;
      case SC_Acquire:
        DEBUG('a', "Acquire syscall.\n");
        Acquire_Syscall(machine->ReadRegister(4));
      break;
      case SC_Release:
        DEBUG('a', "Release syscall.\n");
        Release_Syscall(machine->ReadRegister(4));
      break;

      case SC_Fork:
        DEBUG('a', "Fork syscall.\n");
        Fork_Syscall(machine->ReadRegister(4));		//TODO: should I be receiving a return value?
      break;
      case SC_Exec:
        DEBUG('a', "Exec syscall.\n");
        Exec_Syscall();
      break;
      case SC_Exit:
        DEBUG('a', "Exit syscall.\n");
        Exit_Syscall();
      break;
      case SC_Yield:
        DEBUG('a', "Yield syscall.\n");
        Yield_Syscall();
      break;

      case SC_Wait:
        DEBUG('a', "Wait syscall.\n");
        Wait_Syscall(machine->ReadRegister(4), 
        machine->ReadRegister(5));
      break;
      case SC_Signal:
        DEBUG('a', "Signal syscall.\n");
        Signal_Syscall(machine->ReadRegister(4),
            machine->ReadRegister(5));
      break;
      case SC_Broadcast:
        DEBUG('a', "Broadcast syscall.\n");
        Broadcast_Syscall(machine->ReadRegister(4),
            machine->ReadRegister(5));
      break;
      case SC_CreateLock:
        DEBUG('a', "CreateLock syscall.\n");
        rv = CreateLock_Syscall();
      break;
      case SC_DestroyLock:
        DEBUG('a', "DestroyLock syscall.\n");
        DestroyLock_Syscall(machine->ReadRegister(4));
      break;
      case SC_CreateCondition:
        DEBUG('a', "CreateCondition syscall.\n");
        rv = CreateCondition_Syscall();
      break;
      case SC_DestroyCondition:
        DEBUG('a', "DestroyCondition syscall.\n");
        DestroyCondition_Syscall(machine->ReadRegister(4));
      break;
		
	}

		// Put in the return value and increment the PC
		machine->WriteRegister(2,rv);
		machine->WriteRegister(PrevPCReg,machine->ReadRegister(PCReg));
		machine->WriteRegister(PCReg,machine->ReadRegister(NextPCReg));
		machine->WriteRegister(NextPCReg,machine->ReadRegister(PCReg)+4);
		return;
    } else {
      cout<<"Unexpected user mode exception - which:"<<which<<"  type:"<< type<<endl;
      interrupt->Halt();
    }
}
