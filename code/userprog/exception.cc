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
#include "../threads/system.h"
#include "syscall.h"
#include "interrupt.h"
#include "utility.h"
#include "synch.h"
#include "../threads/synch.h"
#include "network.h"
#include "../network/post.h"
#include <stdio.h>
#include <iostream>

using namespace std;

#define MAX_LOCKS 1000
#define MAX_CVS 1000
#define MAX_PROCESSES 1000

#define MAX_SIZE 20

#define CREATELOCK 0
#define ACQUIRE    1
#define RELEASE     2
#define DESTROYLOCK 3
#define CREATECV    4
#define WAIT        5
#define SIGNAL      6
#define BROADCAST   7
#define DESTROYCV   8
#define CREATEMV    9
#define SET         10
#define GET         11
#define DESTROYMV   12
#define BADINDEX1    1
#define BADINDEX2    3
#define WRONGPROCESS1 1
#define WRONGPROCESS2 4
#define DELETED1     1
#define DELETED2     5
#define TOBEDELETED1 1
#define TOBEDELETED2 6
#define SUCCESS1    1
#define SUCCESS2    7
#define WRONGLOCK   18
#define EMPTYLIST   19
#define OWNER1      2
#define OWNER2      0
#define WAITING1    2
#define WAITING2    1

#define PageInMem 0
#define PageInSwap 1
#define PageInExec 2

// To hold all of the data about a created lock
struct lockData{
  Lock* lock;                
  AddrSpace *space;        // AddrSpace of the owning process
  bool isDeleted;          // If the lock has been deleted
  bool toBeDeleted;        // If the lock is supposed to be deleted once all active threads finish
  int numActiveThreads;    // Number of threads waiting on or using the lock
};

// Debug values for keeping track of threads Forked and Exited
int fork_count = 0;
int exit_count = 0;

// Keep track of which TLB index to use
int currentTLB = 0;


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

// Data for creating new process
Lock* procLock = new Lock("NewProcess Lock");
// Lock for printing formatted strings from user processes
Lock *printLock= new Lock("Print Lock");


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

// newKernelThread(int vAddress)
// - Creates a new kernel thread in this address space
// - Takes virtual address of thread as an argument; returns void
// - Should only be run off of Fork call, meaning it is currentThread
// - NOTE: in order to pass as VoidFunctionPtr to t->Fork, needed to remove association with AddrSpace
//   - as a result, need to call currentThread manually for function/values
void newKernelThread(int vAddress)
{
  //TODO: Checks for valid thread, debug statements
  DEBUG('u', "newKernelThread=%s,%d\n", currentThread->getName(), currentThread->getThreadNum());
  
  currentThread->space->kernThreadLock->Acquire();
  
  DEBUG('u', "newKernelThread: Starting a new kernel thread in process %s at address %d\n", 
      currentThread->space->getProcessName(), vAddress);
  
  // Set PCReg (and NextPCReg) to kernel thread's virtual address
  machine->WriteRegister(PCReg, vAddress);
  machine->WriteRegister(NextPCReg, vAddress+4);
  
  // Set the StackReg to the new starting position of the stack for this thread (jump up in stack in increments of UserStackSize)
  machine->WriteRegister(StackReg, currentThread->space->getEndStackReg() - (currentThread->getThreadNum()*UserStackSize));
  DEBUG('u', "newKernelThread: Writing to StackReg 0x%d\n", 
      currentThread->space->getEndStackReg()-(currentThread->getThreadNum()*UserStackSize));
      
  // Prevent info loss during context switch
  currentThread->space->RestoreState();
  
  // Increment number of threads running and release lock
  currentThread->space->kernThreadLock->Release();
  
  // Run the new kernel thread
  machine->Run();
}

// exec_thread
// - Called from Exec syscall to start a new process
void exec_thread(int vAddr) {
  currentThread->space->InitRegisters();
  currentThread->space->RestoreState();
  machine->Run();
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
  } else
    return -1;
}

int Random_Syscall(int m){
	int retVal;
	retVal = (int)(rand() % m);
	return retVal;
}


void Print_Syscall(unsigned int stPtr, int p1, int p2, int p3) {
  char *string = new char[100];
  printLock->Acquire();
  
  // Error Checking
  if(!string || string == 0) {
    printf("%s","Print: ERROR: invalid string pointer\n");
    return;
	}
  
  if(copyin(stPtr, 100, string) == -1 ) {
    printf("%s","Print: ERROR: failed to copy string\n");
    delete string;
    return;
  }
  
  if((p1==-1) && (p2==-1) && (p3==-1)) {
    printf(string);
  }
  else if((p2==-1) && (p3==-1)) {
    printf(string, p1);
  }
  else if(p3==-1) {
    printf(string, p1, p2);
  }
  else
  {
    printf(string,p1,p2, p3);
  }

	printLock->Release();

  return;  
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


void Acquire_Syscall(int lockIndex) {
   
  PacketHeader inPacketHeader;
  PacketHeader outPacketHeader;
  MailHeader   inMailHeader;
  MailHeader   outMailHeader;

  char* msg = new char[MAX_SIZE];
  char* response = new char[MAX_SIZE];

  sprintf(msg, "s%di%d", ACQUIRE, lockIndex);

  outPacketHeader.to = 0;
  outMailHeader.to = 0;
  outMailHeader.from = currentThread->getProcessID();
  outPacketHeader.from = currentThread->getProcessID();
  outMailHeader.length = strlen(msg) + 1;

  bool success = postOffice->Send(outPacketHeader, outMailHeader, msg);

  if(!success) {
    printf("The postOffice send failed. You must not have the other Nachos running.Terminating Nachos.\n");
    interrupt->Halt();
  }

  postOffice->Receive(0, &inPacketHeader, &inMailHeader, response);

  if(response[0] == 'e') {
    if((response[1]-48) == BADINDEX1 && (response[2]-48) == BADINDEX2) {
      printf("ERROR: Bad Index.\n");
    }
    else if((response[1]-48)==DELETED1 && (response[2]-48)==DELETED2) {
      printf("ERROR: Already Deleted.\n");
    }
    else if((response[1]-48)==TOBEDELETED1 && (response[2]-48)==TOBEDELETED2) {
      printf("NOTE: The lock will be deleted soon.\n");
    }
  }
  else if((response[0]-48)==OWNER1 && (response[1]-48)==OWNER2) {
    printf("ERROR: Already the lock owner.\n");
  }
  else if((response[0]-48)==SUCCESS1 && (response[1]-48)==SUCCESS2) {
    printf("SUCCESS: Acquired the lock.\n");
  }
  else if((response[0]-48)==WAITING1 && (response[1]-48)==WAITING2) {
    printf("SUCCESS: Waiting to acquire the lock.\n");
  }
  else {
    printf("ERROR: unknown response %s.\n", response);
  }
  fflush(stdout);
  return;
}

void Release_Syscall(int lockIndex) {

  PacketHeader inPacketHeader;
  PacketHeader outPacketHeader;
  MailHeader inMailHeader;
  MailHeader outMailHeader;

  char* msg = new char[MAX_SIZE];
  char* response = new char[MAX_SIZE];

  sprintf(msg, "s%di%d", RELEASE, lockIndex);

  outPacketHeader.to = 0;
  outMailHeader.to = 0;
  outMailHeader.from = currentThread->getProcessID();
  outPacketHeader.from = currentThread->getProcessID();
  outMailHeader.length = strlen(msg) + 1;

  bool success = postOffice->Send(outPacketHeader, outMailHeader, msg);

  if(!success) {
    printf("The postOffice send failed. Terminating Nachos.\n");
    interrupt->Halt();
  }

  postOffice->Receive(0, &inPacketHeader, &inMailHeader, response);
  
  if(response[0] == 'e') {
    if((response[1]-48)==BADINDEX1 && (response[2]-48)==BADINDEX2) {
      printf("ERROR: Bad Index.\n");
    }
    else if((response[1]-48)==DELETED1 && (response[2]-48)==DELETED2) {
      printf("ERROR: Already Deleted.\n");
    }
    else if((response[1]-48)==WRONGPROCESS1 && (response[2]-48)==WRONGPROCESS2) {
      printf("ERROR: Not the lock owner!\n");
    }
  }
  else if((response[0]-48)==SUCCESS1 && (response[1]-48) == SUCCESS2) {
    printf("SUCCESS: Released the lock.\n");
  }
  return;  
}


// Fork Syscall. Fork new thread from pointer to void function
void Fork_Syscall(int vAddress)
{
  if(vAddress > (currentThread->space->getNumPages()*PageSize)) {
    DEBUG('u', "Fork: ERROR: Invalid address, would extend beyond usable range.\n");
    return;
  }
  
  processTableLock->Acquire();
  
  DEBUG('u', "Shit: FORK CALLED %d times!\n", ++fork_count);
  // TODO: check for max threads, whether vAddress is outside size of page table
  char* name = "tempName";
  Thread *t = new Thread(name);
  
  // Initialize new thread data
  int num = currentThread->space->threadTable->Put(t);             // add new thread to thread table
  int processID = currentThread->space->getProcessID();
  char* tName = currentThread->space->getProcessName();
  name = new char[30];
  strcpy(name, tName);
  sprintf(name, "%s-%d", tName, num);
  
  DEBUG('u', "pID=%d\n", processID);
  DEBUG('u', "pName=%s\n", currentThread->space->getProcessName());
  DEBUG('u', "pNum=%d\n", num);
//  sprintf(name, "%s%d", currentThread->space->getProcessName(), num);
  
  // Set properties of individual thread
  t->setName(name);
  t->setThreadNum(num);
  t->setProcessID(processID);
  t->space = currentThread->space;      // Set new thread to currentThread's address space
  DEBUG('u', "New thread named '%s'; thread# %d with processID %d (%s)\n", name, num, processID, t->space->getProcessName());
  
  processTableLock->Release();
 
  // Finally Fork a new kernel thread 
  t->Fork((VoidFunctionPtr)newKernelThread, vAddress);
//  currentThread->Yield(); // CHANGED
}


// Exec Syscall. Create a new process with own address space, etc.
int Exec_Syscall(int vAddress, int len) {
  
  // Ripped from Open_Syscall; if help needed, refer back there
  char *buf = new char[len+1];  // Kernel buffer to put the name in
  OpenFile *f;                  // The new open file
  int fileID;                       // The openfile id

  DEBUG('u', "Shit: FORK (Exec) CALLED %d times!\n", ++fork_count);
  
  // Error checking
  if (!buf) {
    printf("%s","Can't allocate kernel buffer in Exec\n");
    return -1;
  }
  if( copyin(vAddress,len,buf) == -1 ) {
    printf("%s","Bad pointer passed to Exec\n");
    delete[] buf;
    return -1;
  }

  buf[len]='\0';

  f = fileSystem->Open(buf);
  if(!f) {
    printf("Error opening file at %s\n", buf);
    delete f;
    return -1;
  }
  
  
  // Create a new address space and add the first thread to the process
  AddrSpace* processSpace = new AddrSpace(f);
  
  processTableLock->Acquire();
//  int pID = processTable->Put(processSpace);
  int pID = processSpace->getProcessID();
  char* pName = processSpace->getProcessName();
  char* name = new char[20];
  strcpy(name, pName);
  sprintf(name, "%s%d", pName, pID);
  processSpace->setProcessName(name);
  DEBUG('u', "EXEC: Created new process '%s'. Now %d processes in processTable.\n", processSpace->getProcessName(), processTable->Size());
//  printf("MYNAMEBITCH2=%s\n", pName);
  processTableLock->Release();
  
//  delete f;                           // Close the file
//  processSpace->InitRegisters();      // set the initial register values
//  processSpace->RestoreState();       // load page table register
  
  // Create first thread to be added to address space
  Thread* newThread = new Thread(buf);
  newThread->space = processSpace;
  int threadNum = processSpace->threadTable->Put(newThread);       // add first new thread to thread table
  newThread->setThreadNum(threadNum);
  newThread->setProcessID(pID);

  // Fork this new thread
  newThread->Fork((VoidFunctionPtr)exec_thread, vAddress);
//  currentThread->Yield();
  
  // Returns process ID (spaceID) to be written to register 2
  return pID;
}

void Exit_Syscall() {
  DEBUG('u', "Exit: Exit has been entered!\n");
  processTableLock->Acquire();

  // If all processes have already exitted
  if(processTable->Size() == 0) {
    printf("Exit: ERROR: All processes have exited already, Nachos should have halted.\n");
    processTableLock->Release();
    interrupt->Halt();
  }

  // If current thread had an error when making the process
  if(currentThread->getProcessID() == -1) {
    printf("Exit: ERROR: Can't exit this thread, it is not a valid process.\n");
    processTableLock->Release();
    interrupt->Halt();
  }

  DEBUG('u', "Shit: EXIT CALLED %d times!\n", ++exit_count);

  DEBUG('u', "Exit: Number threads left in process = %i \n", currentThread->space->threadTable->Size());
  DEBUG('u', "Exit: Number of processes left = %i \n", processTable->Size());

  // Last executing thread in the last process
  if(processTable->Size() == 1) {
    if(currentThread->space->threadTable->Size() == 1) {
      processTableLock->Release();
      printf("\nExit: Nachos successfully shutting down.\n");
      interrupt->Halt();
    }
  }

  // Last executing thread in a process - not the last process
  if(currentThread->space->threadTable->Size() == 1) {
    DEBUG('u', "Exit: Removing process '%s' (id=%i) from Nachos (no threads left).\n", currentThread->space->getProcessName(), currentThread->getProcessID());
    currentThread->space->threadTable->Remove(currentThread->getThreadNum());
    processTable->Remove(currentThread->getProcessID());

    for(int i = 0; i < currentThread->space->getNumPages(); i++) {
      if(currentThread->space->pageTable[i].valid == true && currentThread->space->pageTable[i].physicalPage != -1) {
        iptLock->Acquire();
        ipt[currentThread->space->pageTable[i].physicalPage].valid = false;
        iptLock->Release();
        currentThread->space->removePage(i);
      }
    }
    // delete currentThread->space;
  } else {
   currentThread->space->threadTable->Remove(currentThread->getThreadNum());
  }

  processTableLock->Release();

  DEBUG('u', "Thread %s has been exited (%i).\n", currentThread->getName(), currentThread->space->isMain());
  // if(!(currentThread->space->isMain()))
  currentThread->Finish();
}

void Yield_Syscall() {
  currentThread->Yield();
}

void Wait_Syscall(int cvIndex, int lockIndex) {
  PacketHeader inPacketHeader;
  PacketHeader outPacketHeader;
  MailHeader   inMailHeader;
  MailHeader   outMailHeader;

  char* msg = new char[MAX_SIZE];
  char* response = new char[MAX_SIZE]; 	

  sprintf(msg, "s%dc%dl%d", WAIT, cvIndex, lockIndex);
  DEBUG('r', "msg: %s\n", msg);

  outPacketHeader.to = 0;
  outMailHeader.to = 0;
	outMailHeader.length = strlen(msg) + 1;
  outMailHeader.from = currentThread->getProcessID();   // IS THIS RIGHT?
  outPacketHeader.from = currentThread->getProcessID(); // IS THIS RIGHT?
	DEBUG('r', "outMailHeader.length %i\n", outMailHeader.length);
  bool success = postOffice->Send(outPacketHeader, outMailHeader, msg);
  if(!success) {
    printf("The postOffice send failed in Signal. You must not have the other Nachos running. Terminating Nachos.\n");
    interrupt->Halt();
  }

  postOffice->Receive(0, &inPacketHeader, &inMailHeader, response);
  fflush(stdout);

  /*cvArray->Acquire();
  lockArray->Acquire();

  // Check that the index is valid
  if(cvIndex < 0 || cvIndex >= nextCVPos) {
    printf("ERROR: The given cv index is not valid.\n");
    cvArray->Release();
    lockArray->Release();
    return;
  } 

  // Check that the condition belongs to the calling processes
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
	cvArray->Release();
  lockArray->Release();
  conditions[cvIndex].condition->Wait(locks[lockIndex].lock);*/
}

void Signal_Syscall(int cvIndex, int lockIndex) {
  PacketHeader inPacketHeader;
  PacketHeader outPacketHeader;
  MailHeader   inMailHeader;
  MailHeader   outMailHeader;

  char* msg = new char[MAX_SIZE];
  char* response = new char[MAX_SIZE]; 	

  sprintf(msg, "s%dc%dl%d", SIGNAL, cvIndex, lockIndex);
  DEBUG('r', "msg: %s\n", msg);

  outPacketHeader.to = 0;
  outMailHeader.to = 0;
	outMailHeader.length = strlen(msg) + 1;
  outMailHeader.from = currentThread->getProcessID();   // IS THIS RIGHT?
  outPacketHeader.from = currentThread->getProcessID(); // IS THIS RIGHT?

  bool success = postOffice->Send(outPacketHeader, outMailHeader, msg);
	
  if(!success) {
    printf("The postOffice send failed in Signal. You must not have the other Nachos running. Terminating Nachos.\n");
    interrupt->Halt();
  }

  postOffice->Receive(0, &inPacketHeader, &inMailHeader, response);

  //TODO: Error handling
  fflush(stdout);

  /*cvArray->Acquire();
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
  // SWITCH THESE?
  conditions[cvIndex].numActiveThreads--; // One less thread using the condition
  conditions[cvIndex].condition->Signal(locks[lockIndex].lock);

  // Check if the condition is to be deleted, if so, delete it
  if(conditions[cvIndex].toBeDeleted && (conditions[cvIndex].numActiveThreads == 0)) {
    conditions[cvIndex].isDeleted = true;
    printf("Deleted condition %i.\n", cvIndex);
  }
  cvArray->Release();
  lockArray->Release();*/
}

void Broadcast_Syscall(int cvIndex, int lockIndex) {
 	PacketHeader inPacketHeader;
  PacketHeader outPacketHeader;
  MailHeader   inMailHeader;
  MailHeader   outMailHeader;

	char* msg = new char[MAX_SIZE];
  char* response = new char[MAX_SIZE]; 	

	sprintf(msg, "s%dc%dl%d", BROADCAST, cvIndex, lockIndex);
	DEBUG('r', "msg: %s\n", msg);

	outPacketHeader.to = 0;
  outMailHeader.to = 0;
	outMailHeader.length = strlen(msg) + 1;
  outMailHeader.from = currentThread->getProcessID();   // IS THIS RIGHT?
  outPacketHeader.from = currentThread->getProcessID(); // IS THIS RIGHT?

	bool success = postOffice->Send(outPacketHeader, outMailHeader, msg);
	
	if(!success) {
    printf("The postOffice send failed in Broadcast. You must not have the other Nachos running. Terminating Nachos.\n");
    interrupt->Halt();
  }

	postOffice->Receive(0, &inPacketHeader, &inMailHeader, response);

	//TODO: Error handling
	fflush(stdout);
	
	
  /*cvArray->Acquire();
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
	DEBUG('w', "Condition %i has %i numActiveThreads\n", cvIndex, conditions[cvIndex].numActiveThreads);
  conditions[cvIndex].condition->Broadcast(locks[lockIndex].lock);
  conditions[cvIndex].numActiveThreads = 0;

  // Check if the condition is to be deleted, if so, delete it
  if(conditions[cvIndex].toBeDeleted && (conditions[cvIndex].numActiveThreads == 0)) {
    conditions[cvIndex].isDeleted = true;
    printf("Deleted condition %i.\n", cvIndex);
  }
  cvArray->Release();
  lockArray->Release();*/
}

int CreateLock_Syscall(unsigned int strPtr, int length) {

  int lockIndex = -1;

  PacketHeader inPacketHeader;
  PacketHeader outPacketHeader;
  MailHeader   inMailHeader;
  MailHeader   outMailHeader;

  char* msg = new char[MAX_SIZE];
  char* name = new char[MAX_SIZE];
  char* response = new char[MAX_SIZE];
  
  copyin(strPtr, MAX_SIZE, msg);
  
  if(length >= MAX_SIZE - 4) {
    printf("ERROR: This lock name is too long.\n");
    return -1;
  }

  sprintf(name, "s%dn%s", CREATELOCK, msg);

  // VALIDATE NAME?

  printf("Process ID %i.\n", currentThread->getProcessID());

  outPacketHeader.to = 0;
  outMailHeader.to = 0;
  outMailHeader.from = currentThread->getProcessID();   // IS THIS RIGHT?
  outPacketHeader.from = currentThread->getProcessID(); // IS THIS RIGHT?
  outMailHeader.length = strlen(name) + 1;

  bool success = postOffice->Send(outPacketHeader, outMailHeader, name);
  
  if(!success) {
    printf("The postOffice send failed. You must not have the other Nachos running. Terminating Nachos.\n");
    interrupt->Halt();
  }

  postOffice->Receive(0, &inPacketHeader, &inMailHeader, response);

  if(response[0] == 'e') {
    printf("ERROR: Exceeded max number of locks.\n");
  }
  else if(response[0] == 's') {
    int i = 1;
    int j = strlen(response);
    int total = 0;
    
    while(response[j] != 's') {
      if(response[j] != '\0') {
	total += (response[j]-48)*i;
	i = i*10;
      }
      j--;
    }
    lockIndex = total;
  }
  fflush(stdout);

  printf("Lock Index: %i.\n", lockIndex);
  return lockIndex;
}
  
void DestroyLock_Syscall(int lockIndex) {
  
  PacketHeader inPacketHeader;
  PacketHeader outPacketHeader;
  MailHeader   inMailHeader;
  MailHeader   outMailHeader;

  char* msg = new char[MAX_SIZE];
  char* response = new char[MAX_SIZE];

  sprintf(msg, "s%di%d", DESTROYLOCK, lockIndex);
  
  outPacketHeader.to = 0;
  outMailHeader.to = 0;
  outMailHeader.from = currentThread->getProcessID();   // IS THIS RIGHT?
  outPacketHeader.from = currentThread->getProcessID(); // IS THIS RIGHT?
  outMailHeader.length = strlen(msg) + 1;

  bool success = postOffice->Send(outPacketHeader, outMailHeader, msg);
  
  if(!success) {
    printf("The postOffice send failed. You must not have the other Nachos running. Terminating Nachos.\n");
    interrupt->Halt();
  }

  postOffice->Receive(0, &inPacketHeader, &inMailHeader, response);

  if(response[0] == 'e') {
    if((response[1]-48) == BADINDEX1 && (response[2]-48) == BADINDEX2) {
      printf("ERROR: Bad Index.\n");
    }
    else if((response[1]-48)==DELETED1 && (response[2]-48)==DELETED2) {
      printf("ERROR: Already Deleted.\n");
    }
    else if((response[1]-48)==TOBEDELETED1 && (response[2]-48)==TOBEDELETED2) {
      printf("NOTE: The lock will be deleted soon.\n");
    }
  }
  else if(response[0] == 's') {
    printf("SUCCESS: The lock was deleted.\n");
  }
  fflush(stdout);

  return;
}

int CreateCondition_Syscall(unsigned int strPtr, int length) {
  //  printf("Entered CreateCondition_Syscall.\n");
	int cvIndex = -1;
	
	PacketHeader inPacketHeader;
  PacketHeader outPacketHeader;
  MailHeader   inMailHeader;
  MailHeader   outMailHeader;

  char* msg = new char[MAX_SIZE];
  char* name = new char[MAX_SIZE];
  char* response = new char[MAX_SIZE];
	
	copyin(strPtr, MAX_SIZE, msg);
	
	if(length >= MAX_SIZE - 4){
		printf("ERROR: This cv name is too long.\n");
		return -1;
	}
	
	sprintf(name, "s%dn%s", CREATECV, msg);
	outPacketHeader.to = 0;
  outMailHeader.to = 0;
  outMailHeader.from = currentThread->getProcessID();   // IS THIS RIGHT?
  outPacketHeader.from = currentThread->getProcessID(); // IS THIS RIGHT?
  outMailHeader.length = strlen(name) + 1;
	
	bool success = postOffice->Send(outPacketHeader, outMailHeader, name);
	
	if(!success) {
    printf("The postOffice send failed. You must not have the other Nachos running. Terminating Nachos.\n");
    interrupt->Halt();
  }
	
	postOffice->Receive(0, &inPacketHeader, &inMailHeader, response);
	
  if(response[0] == 'e') {
    printf("ERROR: Exceeded max number of cvs.\n");
  }
  else if(response[0] == 's') {
    int i = 1;
    int j = strlen(response);
    int total = 0;
    
    while(response[j] != 's') {
      if(response[j] != '\0') {
				total += (response[j]-48)*i;
				i = i*10;
      }
      j--;
    }
    cvIndex = total;
  }
  fflush(stdout);

  return cvIndex;
}

void DestroyCondition_Syscall(int cvIndex) {
	int index = cvIndex;

	PacketHeader inPacketHeader;
  PacketHeader outPacketHeader;
  MailHeader   inMailHeader;
  MailHeader   outMailHeader;

  char* msg = new char[MAX_SIZE];
  char* response = new char[MAX_SIZE]; 	

	sprintf(msg, "s%dn%d", DESTROYCV, index);
	outPacketHeader.to = 0;
  outMailHeader.to = 0;
  outMailHeader.from = currentThread->getProcessID();   // IS THIS RIGHT?
  outPacketHeader.from = currentThread->getProcessID(); // IS THIS RIGHT?
  outMailHeader.length = strlen(msg) + 1;

	bool success = postOffice->Send(outPacketHeader, outMailHeader, msg);
	
	if(!success) {
    printf("The postOffice send failed in DestroyCV. You must not have the other Nachos running. Terminating Nachos.\n");
    interrupt->Halt();
  }

	postOffice->Receive(0, &inPacketHeader, &inMailHeader, response);

	fflush(stdout);

  return;
	

	/* //printf("Entered DestroyCondition_Syscall.\n");
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
  return;*/
}

int CreateMV_Syscall(unsigned int strPtr, int length, int arraySize) {

  int index = -1;

  PacketHeader outPacketHeader;
  PacketHeader inPacketHeader;
  MailHeader   outMailHeader;
  MailHeader   inMailHeader;

  char* msg = new char[MAX_SIZE];
  char* response = new char[MAX_SIZE];
  char* name = new char[MAX_SIZE];

  copyin(strPtr, MAX_SIZE, msg);

  if(length >= MAX_SIZE-4) {
    printf("ERROR: This mv name is too long.\n");
    return -1;
  }

  if(arraySize < 1 || arraySize > 999) {
    printf("ERROR: Array must have at least 1 entry.\n");
    return -1;
  }

  if(arraySize < 10) {
    sprintf(name, "s%dt%dn%s", CREATEMV, arraySize,  msg);
  }
  else if(arraySize < 100) {
    sprintf(name, "s%dh%dn%s", CREATEMV, arraySize, msg);
  }
  else if(arraySize < 1000) {
    sprintf(name, "s%dk%dn%s", CREATEMV, arraySize, msg);
  }
  
  // VALIDATE NAME?

  outPacketHeader.to = 0;
  outMailHeader.to = 0;
  outMailHeader.from = currentThread->getProcessID();
  outPacketHeader.from = currentThread->getProcessID();
  outMailHeader.length = strlen(name) + 1;

  bool success = postOffice->Send(outPacketHeader, outMailHeader, name);

  if(!success) {
    printf("The postOffice send failed. Terminating Nachos.\n");
    interrupt->Halt();
  }

  postOffice->Receive(0, &inPacketHeader, &inMailHeader, response);

  if(response[0] == 'e') {
    printf("ERROR: Exceeded max number of mvs.\n");
  }
  else if(response[0] == 's') {
    int i = 1;
    int j = strlen(response);
    int total = 0;

    while(response[j] != 's') {
      if(response[j] != '\0') {
	total += (response[j]-48)*i;
	i = i*10;
      }
      j--;
    }
    index = total;
    printf("SUCCESS: Created the MV.\n");
  }

  fflush(stdout);
  return index;
}

void DestroyMV_Syscall(int index) {

  PacketHeader outPacketHeader;
  PacketHeader inPacketHeader;
  MailHeader   outMailHeader;
  MailHeader   inMailHeader;

  char* message = new char[MAX_SIZE];
  char* response = new char[MAX_SIZE];

  sprintf(message, "s%di%d", DESTROYMV, index);

  outPacketHeader.to = 0;
  outMailHeader.to = 0;
  outMailHeader.from = currentThread->getProcessID();
  outPacketHeader.from = currentThread->getProcessID();
  outMailHeader.length = strlen(message) + 1;

  bool success = postOffice->Send(outPacketHeader, outMailHeader, message);

  if(!success) {
    printf("The postOffice send failed. You must not have the other Nachos running. Terminating Nachos.\n");
    interrupt->Halt();
  }

  postOffice->Receive(0, &inPacketHeader, &inMailHeader, response);

  if(response[0] == 'e') {
    if((response[1]-48)==BADINDEX1 && (response[2]-48)==BADINDEX2) {
      printf("ERROR: Bad Index.\n");
    }
    else if((response[1]-48)==DELETED1 && (response[2]-48)==DELETED2) {
      printf("ERROR: Already Deleted.\n");
    }
    else if((response[1]-48)==TOBEDELETED1 && (response[2]-48)==TOBEDELETED2) {
      printf("NOTE: The lock will be deleted soon.\n");
    }
  }
  else if(response[0] == 's') {
    printf("SUCCESS: The lock was deleted.\n");
  }
  fflush(stdout);
  return;
}

void SetMV_Syscall(int index, int value, int arrayIndex) {

  PacketHeader outPacketHeader;
  PacketHeader inPacketHeader;
  MailHeader   outMailHeader;
  MailHeader   inMailHeader;

  char* message = new char[MAX_SIZE];
  char* response = new char[MAX_SIZE];

  sprintf(message, "s%di%di%da%d", SET, index, value, arrayIndex);

  outPacketHeader.to = 0;
  outMailHeader.to = 0;
  outMailHeader.from = currentThread->getProcessID();
  outPacketHeader.from = currentThread->getProcessID();
  outMailHeader.length = strlen(message) + 1;

  bool success = postOffice->Send(outPacketHeader, outMailHeader, message);
  
  if(!success) {
    printf("The postOffice send failed. Terminating Nachos.\n");
    interrupt->Halt();
  }

  postOffice->Receive(0, &inPacketHeader, &inMailHeader, response);

  if(response[0] == 'e') {
    if((response[1]-48)==BADINDEX1 && (response[2]-48)==BADINDEX2) {
      printf("ERROR: Bad Index.\n");
    }
    else if((response[1]-48)==DELETED1 && (response[2]-48)==DELETED2) {
      printf("ERROR: Already Deleted.\n");
    }
  }
  else if(response[0] == 's') {
    printf("SUCCESS: The MV value is %i.\n", value);
  }
  fflush(stdout);
  return;
}

int GetMV_Syscall(int index, int arrayIndex) {

  int value = -1;
 
  PacketHeader outPacketHeader;
  PacketHeader inPacketHeader;
  MailHeader   outMailHeader;
  MailHeader   inMailHeader;

  char* message = new char[MAX_SIZE];
  char* response = new char[MAX_SIZE];

  sprintf(message, "s%di%da%d", GET, index, arrayIndex);

  outPacketHeader.to = 0;
  outMailHeader.to = 0;
  outMailHeader.from = currentThread->getProcessID();
  outPacketHeader.from = currentThread->getProcessID();
  outMailHeader.length = strlen(message) + 1;

  bool success = postOffice->Send(outPacketHeader, outMailHeader, message);
  
  if(!success) {
    printf("The postOffice send failed. Terminating Nachos.\n");
    interrupt->Halt();
  }

  postOffice->Receive(0, &inPacketHeader, &inMailHeader, response);

  if(response[0] == 'e') {
    if((response[1]-48)==BADINDEX1 && (response[2]-48)==BADINDEX2) {
      printf("ERROR: Bad Index.\n");
    }
    else if((response[1]-48)==DELETED1 && (response[2]-48)==DELETED2) {
      printf("ERROR: Already Deleted.\n");
    }
  }
  else if(response[0] == 's') {
    int i = 1;
    int j = strlen(response);
    int total = 0;

    while(response[j] != 's') {
      if(response[j] != '\0') {
	total += (response[j]-48)*i;
	i=i*10;
      }
      j--;
    }
    value = total;
    printf("SUCCESS: Getting MV value %i.\n", value);
  }
  fflush(stdout);
  return value;
}


// Evict a page from the pageMap and return the PPN of the page that was cleared up
int evictAPage() {
  int evictPPN = 0;
  
  // First in-first out eviction
  if(evictType == 0) {
    iptLock->Acquire();
    // Find page in IPT with oldest (largest) time-stamp
    for(int i=1; i<NumPhysPages; i++) {
      if(ipt[evictPPN].timeStamp < ipt[i].timeStamp) {
        evictPPN = i;
      }
    }
    ipt[evictPPN].timeStamp = 0;
    iptLock->Release();
    DEBUG('v', "evictAPage: evicting PPN %i through FIFO method (timeStamp=%i).\n", evictPPN, ipt[evictPPN].timeStamp);
    
  } else {
    // Random eviction
    do {
      evictPPN = rand()%NumPhysPages;
      iptLock->Acquire();
      if(!ipt[evictPPN].inUse) {
        ipt[evictPPN].inUse = true;
        iptLock->Release();
        break;
      }
      iptLock->Release();
    } while(true);
    DEBUG('v', "evictAPage: evicting PPN %i through Random method.\n", evictPPN);
  }
  
  // Propagate dirty bit from TLB to IPT and disable current page in TLB
  IntStatus oldLevel = interrupt->SetLevel(IntOff);
  for(int i = 0; i < TLBSize; i++) {
    // Check to see whether the selected page is in the TLB
    if(evictPPN == machine->tlb[i].physicalPage) {
      // If so, set it invalid and propagate the dirty bit back to the IPT
      ipt[evictPPN].dirty = machine->tlb[i].dirty;
      machine->tlb[i].valid = false;
    }
  }
  (void) interrupt->SetLevel(oldLevel);
  
  // Copy page from memory to swap page
  if(ipt[evictPPN].dirty == true) {
    swapFileLock->Acquire();
    int swapLoc = swapFileMap->Find();      // Find the next page location in the swapfile
    if(swapLoc >= 0) {
      // Found space in SwapFile, writing data from memory to swap file
      char* valToWrite = (char*)(machine->mainMemory + (evictPPN*PageSize));
      swapFile->WriteAt(valToWrite, PageSize, swapLoc*PageSize);
      DEBUG('v', "evictAPage: Wrote from memLoc=%i to swapFile loc=%i\n", 
          valToWrite, swapLoc*PageSize);
    } else {
      DEBUG('v', "evictAPage: ERROR: Could not find any free space in the SwapFile\n");
    }
    
    // Updating PageTable to reflect page moving from mem to swap
    currentThread->space->pageTable[ipt[evictPPN].virtualPage].pageLoc = PageInSwap;
    currentThread->space->pageTable[ipt[evictPPN].virtualPage].swapByteOffset = swapLoc;
    currentThread->space->pageTable[ipt[evictPPN].virtualPage].physicalPage = -1;
//    currentThread->space->pageTable[ipt[evictPPN].virtualPage].valid = false;
    swapFileLock->Release();   
  }
  
  iptLock->Acquire();
  ipt[evictPPN].use = false;
  ipt[evictPPN].inUse = false;
  iptLock->Release();
  
  return evictPPN;
}

// Assign new physical page number after finding an IPT access miss
int handleIPTMiss(unsigned int vpn) {
  int ppn = pageMap->Find();
  
  if(ppn == -1) {
    // Could not find a free space in the pageMap, must evict one to make space
    ppn = evictAPage();
    DEBUG('v', "\nhandleIPTMiss: could not find a free space in pageMap, looking to evict bitch ppn %i.\n", ppn);
  }
  
  DEBUG('v', "handleIPTMiss: in thread '%s': VPN=%i; found PPN= %i.\n", currentThread->getName(), vpn, ppn);
  
  // Have a valid physical page number to use
  pageLock->Acquire();
  if((currentThread->space->pageTable[vpn].pageLoc) == PageInSwap) {
    // Find page in Swap File
    DEBUG('v', "handleIPTMiss: trying to access page in SwapFile.\n");
    
    swapFileLock->Acquire();
    int swapLoc = currentThread->space->pageTable[vpn].swapByteOffset;
    swapFile->ReadAt(machine->mainMemory + (ppn*PageSize), PageSize, swapLoc*PageSize);
    swapFileMap->Clear(swapLoc);
    swapFileLock->Release();
    
    currentThread->space->pageTable[vpn].swapByteOffset = -1;
    currentThread->space->pageTable[vpn].pageLoc = PageInMem;
    currentThread->space->pageTable[vpn].dirty = true;
    
  } else if(currentThread->space->pageTable[vpn].pageLoc == PageInExec) {
    // Copying page from Executable
    DEBUG('v', "Acquired a slut, boning MEM\n");
    currentThread->space->pExec->ReadAt(machine->mainMemory + (ppn*PageSize), PageSize, currentThread->space->pageTable[vpn].location);
    currentThread->space->pageTable[vpn].dirty = false;
    
    DEBUG('v', "handleIPTMiss: Copied page from Executable page %i to pageTable loc=%i.\n",
        machine->mainMemory + (ppn*PageSize), currentThread->space->pageTable[vpn].location);
  }
  
  // Set pageTable for this virtual page to Valid
  currentThread->space->pageTable[vpn].physicalPage = ppn;
  currentThread->space->pageTable[vpn].valid = TRUE;
  
  pageLock->Release();
  DEBUG('v', "Acquired a slut, leaving with ppn%i\n",ppn);
  return ppn;
  
}


// Handle any PageFaultExceptions found
void handlePageFault(unsigned int vAddress) {

//  DEBUG('v', "PF Exception: PageFaultException found in %s at vAddress=%i\n", currentThread->getName(), vAddress);
  
  // Disable interrupts
  IntStatus oldLevel = interrupt->SetLevel(IntOff);
  
  // Define the Virtual Page Number
  int vpn = vAddress/PageSize; //divRoundUp(vAddress, PageSize);
  int ppn = -1;                        // Defines physical page number to look for
  
  // Find the physical page number that matches the VPN passed in
  iptLock->Acquire();
  
  // Increase timestamp on all IPT values. Higher value = older.
  for(int i=0; i<TLBSize; i++) {
    if(machine->tlb[i].valid == true)
      ipt[machine->tlb[i].physicalPage].timeStamp++;
  }
  
  // Find the correct IPT page matching the page that threw the PageFaultException
  for(int i=0; i<NumPhysPages; i++) {
    if(ipt[i].valid && (vpn == ipt[i].virtualPage) && (currentThread->getProcessID() == ipt[i].processID)) {
      ipt[i].use = true;
      ppn=i;
//      printf("FUCKER: IPT valid=%i;  vpn=%i, ipt.vpn=%i;  pID=%i, ipt.pID=%i;\n", ipt[i].valid, vpn, ipt[i].virtualPage, currentThread->getProcessID(), ipt[i].processID);
//      DEBUG('v', "Found physical page number '%i' in thread '%s'\n", ppn, currentThread->getName());
      break;
    }
  }
  iptLock->Release();
  
  if(ppn == -1) {
    DEBUG('v', "IPT miss in thread '%s'\n", currentThread->getName());
    
    // Search for correct physical page number to add to IPT
    ppn = handleIPTMiss(vpn);


    iptLock->Acquire();
    pageLock->Acquire();
    ipt[ppn].virtualPage = vpn;
    ipt[ppn].physicalPage = ppn;
    ipt[ppn].valid = true;
    ipt[ppn].readOnly = currentThread->space->pageTable[vpn].readOnly;
    ipt[ppn].use = currentThread->space->pageTable[vpn].use;
    ipt[ppn].dirty = currentThread->space->pageTable[vpn].dirty;
    ipt[ppn].processID = currentThread->space->pageTable[vpn].processID;
    ipt[ppn].location = currentThread->space->pageTable[vpn].location;
    pageLock->Release();
    iptLock->Release();
  }
  
  // Check to see if current page in TLB is dirty; update IPT, pageTable if it is
  if(machine->tlb[currentTLB].dirty && machine->tlb[currentTLB].valid) {
    ipt[machine->tlb[currentTLB].physicalPage].dirty = true;
    currentThread->space->pageTable[machine->tlb[currentTLB].virtualPage].dirty = true;
  }
  
  // Copy fields from IPT to TLB  
  if(ppn >= 0 && ppn < NumPhysPages) {
    machine->tlb[currentTLB].virtualPage = ipt[ppn].virtualPage;
    machine->tlb[currentTLB].physicalPage = ipt[ppn].physicalPage;
    machine->tlb[currentTLB].valid = ipt[ppn].valid;
    machine->tlb[currentTLB].use = ipt[ppn].use;
    machine->tlb[currentTLB].dirty = ipt[ppn].dirty;
    machine->tlb[currentTLB].readOnly = ipt[ppn].readOnly;
  } else {
    printf("ERROR (handlePageFault): invalid physical page number in IPT (ppn=%i. Max is %i).\n", ppn, NumPhysPages);
  }
  
  currentTLB = (currentTLB+1)%TLBSize;
  
  // Restore interrupts
  (void) interrupt->SetLevel(oldLevel);
}


// #############################################################################


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
      case SC_Random:
	      DEBUG('a', "Random syscall.\n");
	      rv = Random_Syscall(machine -> ReadRegister(4));
      break;
      case SC_Print:
        DEBUG('a', "Print syscall.\n");
        Print_Syscall(machine->ReadRegister(4), 
            machine->ReadRegister(5), 
            machine->ReadRegister(6), 
            machine->ReadRegister(7));
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
        Fork_Syscall(machine->ReadRegister(4));
      break;
      case SC_Exec:
        DEBUG('a', "Exec syscall.\n");
        rv = Exec_Syscall(machine->ReadRegister(4),
            machine->ReadRegister(5));
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
        rv = CreateLock_Syscall(machine->ReadRegister(4),
				machine->ReadRegister(5));
      break;
      case SC_DestroyLock:
        DEBUG('a', "DestroyLock syscall.\n");
        DestroyLock_Syscall(machine->ReadRegister(4));
      break;
      case SC_CreateCondition:
        DEBUG('a', "CreateCondition syscall.\n");
        rv = CreateCondition_Syscall(machine->ReadRegister(4),
				     machine->ReadRegister(5));
      break;
      case SC_DestroyCondition:
        DEBUG('a', "DestroyCondition syscall.\n");
        DestroyCondition_Syscall(machine->ReadRegister(4));
      break;
			case SC_CreateMV:
	DEBUG('a', "CreateMV syscall.\n");
	CreateMV_Syscall(machine->ReadRegister(4),
			 machine->ReadRegister(5),
			 machine->ReadRegister(6));
      break;
      case SC_SetMV:
	DEBUG('a', "Set syscall.\n");
	SetMV_Syscall(machine->ReadRegister(4),
		      machine->ReadRegister(5),
		      machine->ReadRegister(6));
      break;
      case SC_GetMV:
	DEBUG('a', "Get syscall.\n");
	GetMV_Syscall(machine->ReadRegister(4),
		      machine->ReadRegister(5));
      break;
      case SC_DestroyMV:
	DEBUG('a', "DestroyMV syscall.\n");
	DestroyMV_Syscall(machine->ReadRegister(4));
      break;	
      }

      // Put in the return value and increment the PC
      machine->WriteRegister(2,rv);
      machine->WriteRegister(PrevPCReg,machine->ReadRegister(PCReg));
      machine->WriteRegister(PCReg,machine->ReadRegister(NextPCReg));
      machine->WriteRegister(NextPCReg,machine->ReadRegister(PCReg)+4);
      return;
      
    } else if(which==PageFaultException) {
      // Handle TLB miss
      int faultVAddress = machine->ReadRegister(39);
      handlePageFault(faultVAddress);
      
    } else {
      cout<<"Unexpected user mode exception - which:"<<which<<"  type:"<< type<<endl;
      interrupt->Halt();
    }
}

