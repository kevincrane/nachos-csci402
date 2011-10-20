// addrspace.h 
//	Data structures to keep track of executing user programs 
//	(address spaces).
//
//	For now, we don't keep any information about address spaces.
//	The user level CPU state is saved and restored in the thread
//	executing the user program (see thread.h).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#ifndef ADDRSPACE_H
#define ADDRSPACE_H

#include "copyright.h"
#include "filesys.h"
#include "table.h"
#include "iptEntry.h"

#define UserStackSize   1024   // increase this as necessary!
#define MaxNumProgs     10     // max number of programs/stacks
#define MaxNumThreads     60   // max number of threads/addrspace

#define MaxOpenFiles 256
#define MaxChildSpaces 256

class AddrSpace {
  public:
    AddrSpace(OpenFile *executable);	// Create an address space,
					// initializing it with the program
					// stored in the file "executable"
    ~AddrSpace();			// De-allocate an address space

    void InitRegisters();		// Initialize user-level CPU registers,
					// before jumping to user code

    void SaveState();			// Save/restore address space-specific
    void RestoreState();		// info on a context switch
    Table fileTable;			// Table of openfiles
    
    Table *threadTable;     // Table of threads in current process
    Lock *threadLock;       // Lock for actions on current thread
    
    Lock *kernThreadLock;   // Lock for creating a new kernel thread
    
//    void newKernelThread(int);  // Create a new kernel thread  
    
    char* getProcessName() { return processName; }
    void setProcessName(char* name) { processName = name; }
    int getProcessID() { return processID; }
    int getEndStackReg() { return endStackReg; }
    
    int getNumThreadsRunning() { return numThreadsRunning; }
    void incNumThreadsRunning() { numThreadsRunning++; }
    void decNumThreadsRunning() { numThreadsRunning--; }

    void removePage(int i);
    int getNumPages() {return numPages;}
    
    bool isMain() { return isMai; }
    void setMain() { isMai = true; }

    IPTEntry *pageTable;	// Assume linear page table translation
					// for now!
  private:
    
    unsigned int numPages;		// Number of pages in the virtual 
					// address space

		char* processName;      // Name of process in current address space
		int processID;          // ID of process in current address space
		int endStackReg;        // Defines value for end of stack based on size of pageTable
		int numThreadsRunning;  // Number of threads running in this address space
		int numPagesReserved;   // Number of pages used so far
		bool isMai;            // Is this the main thread?
};

#endif // ADDRSPACE_H
