// synch.cc 
//	Routines for synchronizing threads.  Three kinds of
//	synchronization routines are defined here: semaphores, locks 
//   	and condition variables (the implementation of the last two
//	are left to the reader).
//
// Any implementation of a synchronization routine needs some
// primitive atomic operation.  We assume Nachos is running on
// a uniprocessor, and thus atomicity can be provided by
// turning off interrupts.  While interrupts are disabled, no
// context switch can occur, and thus the current thread is guaranteed
// to hold the CPU throughout, until interrupts are reenabled.
//
// Because some of these routines might be called with interrupts
// already disabled (Semaphore::V for one), instead of turning
// on interrupts at the end of the atomic operation, we always simply
// re-set the interrupt state back to its original value (whether
// that be disabled or enabled).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synch.h"
#include "system.h"

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	Initialize a semaphore, so that it can be used for synchronization.
//
//	"debugName" is an arbitrary name, useful for debugging.
//	"initialValue" is the initial value of the semaphore.
//----------------------------------------------------------------------

Semaphore::Semaphore(char* debugName, int initialValue)
{
    name = debugName;
    if(initialValue < 0)		// ensure starting value is valid, prevent possible infinite loop
    	initialValue = 0;
    value = initialValue;
    queue = new List;
}

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	De-allocate semaphore, when no longer needed.  Assume no one
//	is still waiting on the semaphore!
//----------------------------------------------------------------------

Semaphore::~Semaphore()
{
    delete queue;
}

//----------------------------------------------------------------------
// Semaphore::P
// 	Wait until semaphore value > 0, then decrement.  Checking the
//	value and decrementing must be done atomically, so we
//	need to disable interrupts before checking the value.
//
//	Note that Thread::Sleep assumes that interrupts are disabled
//	when it is called.
//----------------------------------------------------------------------

void
Semaphore::P()
{
    IntStatus oldLevel = interrupt->SetLevel(IntOff);	// disable interrupts
    
    while (value == 0) { 			// semaphore not available
		queue->Append((void *)currentThread);	// so go to sleep
		currentThread->Sleep();
    } 
    value--; 					// semaphore available, 
						// consume its value
    
    (void) interrupt->SetLevel(oldLevel);	// re-enable interrupts
}

//----------------------------------------------------------------------
// Semaphore::V
// 	Increment semaphore value, waking up a waiter if necessary.
//	As with P(), this operation must be atomic, so we need to disable
//	interrupts.  Scheduler::ReadyToRun() assumes that threads
//	are disabled when it is called.
//----------------------------------------------------------------------

void
Semaphore::V()
{
    Thread *thread;
    IntStatus oldLevel = interrupt->SetLevel(IntOff);

    thread = (Thread *)queue->Remove();
    if (thread != NULL)	   // make thread ready, consuming the V immediately
		scheduler->ReadyToRun(thread);
    value++;
    (void) interrupt->SetLevel(oldLevel);
}

// Dummy functions -- so we can compile our later assignments 
// Note -- without a correct implementation of Condition::Wait(), 
// the test case in the network assignment won't work!
Lock::Lock(char* debugName) {
  name = debugName;
  waitQueue = new List;
  isFree = true;
  lockOwner = NULL;
}

Lock::~Lock() {
  delete waitQueue;
}

void Lock::Acquire() {
  IntStatus oldLevel = interrupt->SetLevel(IntOff); // disable interrupts

  if(currentThread == lockOwner) {
    (void) interrupt->SetLevel(oldLevel); // enable interrupts
    return;
  }

  if(isFree) {
    isFree = false;                           	// lock is no longer free
    lockOwner = currentThread;               	// now owned by currentThread
  } else {
    waitQueue->Append((void *)currentThread);	// add to lock wait queue
    currentThread->Sleep();                 	// put to sleep
  }
  (void) interrupt->SetLevel(oldLevel); 		// enable interrupts
}

void Lock::Release() {
  Thread *thread;
  IntStatus oldLevel = interrupt->SetLevel(IntOff); // disable interrupts
	if(currentThread != lockOwner) {
		if(lockOwner!= NULL){
    	printf ("Error: Thread (%s) is not the lock owner (%s)!\n", currentThread->getName(), lockOwner->getName()); // print error message
		}
		else{
			printf("Error: There is no lock owner; thread (%s) cannot release the lock!\n", currentThread->getName());
		}
    (void) interrupt->SetLevel(oldLevel);            // restore interrupts
    return;
  }
	if(!(waitQueue->IsEmpty())) {
		thread = (Thread *)waitQueue->Remove(); // remove a thread from lock's wait queue
    if(thread != NULL) {
      scheduler->ReadyToRun(thread);    // put in ready queue in ready state
      lockOwner = thread;               // make lock owner
    }
  } else {
    isFree = true;                      // make lock free
    lockOwner = NULL;                   // clear lock ownership
				
		
		
  }
  (void) interrupt->SetLevel(oldLevel); // restore interrupts
}

Condition::Condition(char* debugName) { 
  name = debugName;
  waitQueue = new List;
  waitingLock = NULL;
}

Condition::~Condition() { 
  delete waitQueue;
}

void Condition::Wait(Lock* conditionLock) { 
  IntStatus oldLevel = interrupt->SetLevel(IntOff); // disable interrupts

  if(conditionLock == NULL) {
    printf ("Error: The lock you want to wait is NULL\n");
    (void) interrupt->SetLevel(oldLevel);
    return;
  }
  if(waitingLock == NULL) {
     waitingLock = conditionLock;
  }
  if(waitingLock != conditionLock) {
     printf ("Error: this isn't the correct waiting lock!\n");
     (void) interrupt->SetLevel(oldLevel);
     return;
  }
  conditionLock->Release();
  waitQueue->Append((void *)currentThread);	// add to lock wait queue
  currentThread->Sleep();
  conditionLock->Acquire();
  (void) interrupt->SetLevel(oldLevel);
}

void Condition::Signal(Lock* conditionLock) { 
  Thread *thread;   //TODO: more descriptive var name
  IntStatus oldLevel = interrupt->SetLevel(IntOff); //disable interrupts

  // If no threads waiting, restore interrupts and return
  if(waitQueue->IsEmpty()) {
    printf ("Error: waitQueue in %s is empty!\n", name);
    (void) interrupt->SetLevel(oldLevel);
    return;
  }
  if(waitingLock != conditionLock) {
    printf ("Error: Signal, waitingLock does not equal conditionLock\n");
    (void) interrupt->SetLevel(oldLevel);
    return;
  }
  thread = (Thread *)waitQueue->Remove();
  if(thread != NULL) {
    scheduler->ReadyToRun(thread);      // Put in ready queue in ready state
  }
  if (waitQueue->IsEmpty()) {
    waitingLock = NULL;
  }
  (void) interrupt->SetLevel(oldLevel);
}

void Condition::Broadcast(Lock* conditionLock) { 
  while (!waitQueue->IsEmpty()) {
    Signal(conditionLock);
  }
}
