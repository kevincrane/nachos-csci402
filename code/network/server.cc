// Server Application

#include "system.h"
#include "network.h"
#include "post.h"
#include "interrupt.h"
#include "utility.h"
#include "synch.h"

#define MAX_LOCKS 1000
#define MAX_CVS   1000
#define MAX_MVS   1000
#define MAX_PROCESSES 1000
#define MAX_SIZE  20

#define CREATELOCK  48
#define ACQUIRE     49
#define RELEASE     50
#define DESTROYLOCK 51
#define CREATECV    52
#define WAIT        53
#define SIGNAL      54
#define BROADCAST   55
#define DESTROYCV   56
#define CREATEMV    57
#define SET1        1
#define SET2        0
#define GET1        1
#define GET2        1
#define DESTROYMV1  1
#define DESTROYMV2  2
#define BADINDEX    13
#define WRONGPROCESS 14
#define DELETED     15
#define TOBEDELETED 16
#define SUCCESS     17
#define WRONGLOCK   18
#define EMPTYLIST   19
#define OWNER       20
#define WAITING     21

PacketHeader  inPacketHeader;
PacketHeader  outPacketHeader;
MailHeader    inMailHeader;
MailHeader    outMailHeader; 

char* message;

int nextLock = 0;    // Next lock position in the array
int nextCV = 0;      // Next CV position in the array
int nextMVPos = 0;   // Next MV position in the array

// Server version of the lock class
class serverLock {

public:
  char* name;          // Name of the lock
  bool available;      // If it is currently being used
  int machineID;
  int mailboxNum;
  List* waitList;      // Those waiting for the lock

  // Default constructor
  serverLock::serverLock() {
    name = new char[MAX_SIZE];
    available = true;
    machineID = -1;
    mailboxNum = -1;
    waitList = new List();
  }

  void serverLock::destruct() {
    delete waitList;
  }
};

// Server version of the CV class
class serverCV {

public:
  char* name;           // Name of the CV
  int lockIndex;        // Index of the lock belonging to the CV
  int machineID;
  int mailboxNum;
  List* waitList;       // Waiting threads

  // Default constructor
  serverCV::serverCV() {
    name = new char[MAX_SIZE];
    lockIndex = -1;
    machineID = -1;
    mailboxNum = -1;
    waitList = new List();
  }

  void serverCV::destruct() {
    delete waitList;
  }
};

// Threads to add to wait lists
class waitingThread {
public:
  int machineID;
  int mailboxNum;

  waitingThread::waitingThread() {
    machineID = -1;
    mailboxNum = -1;
  }
};

// Keeps track of all lock data
struct lockData {
  serverLock lock;
  bool isDeleted;
  bool toBeDeleted;
  int numActiveThreads;
};

// Keeps track of all cv data
struct cvData {
  serverCV condition;
  bool isDeleted;
  bool toBeDeleted;
  int numActiveThreads;
};

// Contain all information on a MV
struct mvData {
  int values[1000]; // Assume max array size is 1000
  int size;
  bool isDeleted;
  char* name;
};

lockData lcks[MAX_LOCKS];  // Array containing all locks
cvData   cvs[MAX_CVS];     // Array containing all CVs
mvData   mvs[MAX_MVS];     // Array containing all MVs

// Create a lock with the inputted name
void CreateLock(char* msg) {
  
  lockData myLock;
  char* response = new char[MAX_SIZE];
  char* name = new char[MAX_SIZE - 3];

  //printf("MSG %s.\n", msg);

  // Pull the name out of the message
  for(int i = 3; i < MAX_SIZE + 1; i++) {
    name[i-3] = msg[i];
  }

  //printf("Name %s.\n", name);

  //printf("NextLock %i.\n", nextLock);

  // Check if the lock name already exists, if so return the existing index
  for(int i = 0; i < nextLock; i++) {
    printf("Lock %i %s.\n", i, lcks[i].lock.name);
    if(*lcks[i].lock.name == *name) {
      sprintf(response, "s%d", i);
      outMailHeader.length = strlen(response) + 1;
      postOffice->Send(outPacketHeader,outMailHeader, response);
      return;
    }
  } 

  // Initialize the data
  myLock.isDeleted = false;
  myLock.toBeDeleted = false;
  myLock.numActiveThreads = 0;
  myLock.lock.name = new char[MAX_SIZE];
  
  // Copy the name from the message into the lock's name variable
  sprintf(myLock.lock.name, "%s", name);

  // Check that we haven't created too many locks
  if(nextLock >= MAX_LOCKS) {
    sprintf(myLock.lock.name, "e%d", MAX_LOCKS);
  }
  else {
    lcks[nextLock] = myLock;
    sprintf(response, "s%d", nextLock);
  }
 
  outMailHeader.length = strlen(response) + 1;

  printf("outPacketHeader to %i.\n", outPacketHeader.to);
  printf("outMailHeader to %i.\n", outMailHeader.to);

  postOffice->Send(outPacketHeader, outMailHeader, response);

  nextLock++;
}

// Acquire the lock with the given index
void Acquire(int index) {

  char* response = new char[MAX_SIZE];

  // Check that the lock index is valid
  if(index < 0 || index >= nextLock) {
    printf("ERROR: Bad index on acquire\n");
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  // Check that the lock has not already been deleted
  else if(lcks[index].isDeleted) {
    printf("ERROR: Lock is already deleted\n");
    sprintf(response, "e%d", DELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  // Check that the lock isn't to be deleted
  else if(lcks[index].toBeDeleted) {
    printf("ERROR: Lock is already to be deleted\n");
    sprintf(response, "e%d", TOBEDELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  // Check if the thread is already the lock owner
  if(lcks[index].lock.machineID == inPacketHeader.from && lcks[index].lock.mailboxNum == inMailHeader.from) {
    printf("This thread already has the lock.\n");
    sprintf(response, "%d", OWNER);
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  // Increment the number of threads
  lcks[index].numActiveThreads++;

  // Check if the lock is available
  if(lcks[index].lock.available) {
    lcks[index].lock.available = false; // Make it unavailable
    lcks[index].lock.machineID = inPacketHeader.from; // Change the owner
    lcks[index].lock.mailboxNum = inMailHeader.from;
    sprintf(response, "%d", SUCCESS);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }
  // Add the thread to the wait queue
  else {
    DEBUG('s', "Lock %i is unavailable, time to wait\n", index);
    waitingThread* wt = new waitingThread;
    wt->machineID = inPacketHeader.from;
    wt->mailboxNum = inMailHeader.from;
    lcks[index].lock.waitList->Append((void*)wt);
  } 
}

// Release the lock with the given index
void Release(int index) {

  char* response = new char[MAX_SIZE];
  waitingThread* thread;

  // Check that the lock index is valid
  if(index < 0 || index >= nextLock) {
    printf("ERROR: Bad index on release\n");
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  // Check that the lock has not already been deleted
  else if (lcks[index].isDeleted) {
    printf("ERROR: This lock is already deleted.\n");
    sprintf(response, "e%d", DELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  lcks[index].numActiveThreads--; // A thread is no longer using this lock

  // Check if the lock was waiting to be deleted, if so, delete it
  if(lcks[index].toBeDeleted && (lcks[index].numActiveThreads == 0)) {
    lcks[index].isDeleted = true;
    printf("Deleted lock %i.\n", index);
  }

  // Check that this is the lock owner
  if(lcks[index].lock.machineID != inPacketHeader.from || lcks[index].lock.mailboxNum != inMailHeader.from) {
    printf("ERROR: Not the lock owner!\n");
    sprintf(response, "e%d", WRONGPROCESS);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }
  
  // Check if the wait list is empty, if not pull a thread off the wait list and
  // give it the lock
  if(!(lcks[index].lock.waitList->IsEmpty())) {
    thread = (waitingThread*)lcks[index].lock.waitList->Remove();

    lcks[index].lock.machineID = thread->machineID;   // Change the lock owner
    lcks[index].lock.mailboxNum = thread->mailboxNum;
    sprintf(response, "%d", SUCCESS);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response); // Tell the thread it has successfully release the lock
    char* response2 = new char[MAX_SIZE];
    sprintf(response2, "%d", SUCCESS);
    outMailHeader.to = thread->mailboxNum;
    outPacketHeader.to = thread->machineID;
    outMailHeader.length = strlen(response2) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response2); // Tell the thread that now has the lock that it has successfully acquired
  }
  else {
    // Make the lock available
    lcks[index].lock.available = true;
    lcks[index].lock.machineID = -1;
    lcks[index].lock.mailboxNum = -1;
    sprintf(response, "%d", SUCCESS);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response); // Tell the thread it has successfully released the lock
  }
  return;
}

// Destroy the lock with the given index
void DestroyLock(int index) {

  char* response = new char[MAX_SIZE];
 
  // Check that the lock index is valid
  if(index < 0 || index >= nextLock) {
    printf("ERROR: Bad index on destroy lock\n");
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  // Check that the lock has not been deleted
  else if(lcks[index].isDeleted) {
    printf("ERROR: This lock is already deleted\n");
    sprintf(response, "e%d", DELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  // Check that the lock is not already to be deleted
  else if(lcks[index].toBeDeleted) {
		printf("ERROR: This lock is already to be deleted\n");
    sprintf(response, "e%d", TOBEDELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  // Check if there are still active threads running
  else if(lcks[index].numActiveThreads > 0) {
    lcks[index].toBeDeleted = true;
    sprintf(response, "e%d", TOBEDELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  // Delete the lock
  else {
    lcks[index].isDeleted = true;
    sprintf(response, "s");
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
  }
  return;
}

// Create a CV with the given name
void CreateCV(char* msg) {

  cvData myCV;
  char* response = new char[MAX_SIZE];
  char* name = new char[MAX_SIZE - 3];
	
  // Copy the name from the msg
  for(int i = 3; i < MAX_SIZE + 1; i++)
    name[i-3] = msg[i];

  // Check if CV name already exists
  for(int i = 0; i < nextCV; i++) {
    DEBUG('r', "i: %i.name = %s\n", i, cvs[i].condition.name);
    DEBUG('r', "name: %s\n", name);
    if(strcmp(cvs[i].condition.name,name)==0 && !cvs[i].isDeleted) {
      printf("Names match, name: %s, returning index value of %i\n\n", name, i);
      sprintf(response, "s%d", i);
      outMailHeader.length = strlen(response) + 1;
      postOffice->Send(outPacketHeader,outMailHeader, response);
      return;
    }
  }
	
  // Initialize the data
  myCV.condition.name = name;
  myCV.isDeleted = false;
  myCV.toBeDeleted = false;
  myCV.numActiveThreads = 0;

  // Check that we haven't exceeded the max lock number
  if(nextCV >= MAX_CVS)
    sprintf(myCV.condition.name, "e%d", MAX_CVS);
  else{
    cvs[nextCV] = myCV;
    sprintf(response, "s%d", nextCV);
  }

  outMailHeader.length = strlen(response) + 1;
	
  printf("CV Created at position: %i\n\n", nextCV);
	
  postOffice->Send(outPacketHeader, outMailHeader, response);

  nextCV++;
}

// Call wait on the cv in the msg
void Wait(char* msg) {
  DEBUG('b', "Entered wait call on server side\n");
  char* index = new char[MAX_SIZE];
  char* response = new char[MAX_SIZE];
  waitingThread *thread = new waitingThread;
  thread->machineID = inPacketHeader.from;
  thread->mailboxNum = inMailHeader.from;
  
  // Pull data out of the msg
  for(int i = 3; i < MAX_SIZE + 1; i++)
    index[i-3] = msg[i];
  int i = 1;
  int j = strlen(index) - 1;
  int cvIndex = 0;
  int lockIndex = 0;
  DEBUG('b', "WAIT: MSG: %s, INDEX: %s\n", msg, index);
  while(index[j] != 'l'){
    DEBUG('b', "WAIT: IN LOCK LOOP; index[j]: %i\n", index[j]);
    lockIndex += (index[j]-48)*i;
    i = i*10;
    j--;
  }
  i = 1;
  j--;
  while(j>-1){
    DEBUG('b', "WAIT: IN CV LOOP; index[j]: %i\n", index[j]);
    cvIndex += (index[j]-48)*i;
    i = i*10;
    j--;
  }  
  DEBUG('b', "WAIT: CVINDEX: %i, LOCKINDEX: %i\n", cvIndex, lockIndex);
  
  // Check that the index is valid     
  if(cvIndex < 0 || cvIndex >= nextCV) {
    printf("ERROR: The given cv index is not valid.\n");
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  } 

  // Check that the lock index is valid
  else if (lockIndex < 0 || lockIndex >= nextLock) {
    printf("ERROR: The given lock index is not valid.\n");
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  } 

  // Check that the CV has not been deleted
  else if (cvs[cvIndex].isDeleted) {
    printf("ERROR: This condition has been deleted.\n");
    sprintf(response, "e%d", DELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  } 

  // Check that the CV is not to be deleted
  else if (cvs[cvIndex].toBeDeleted) {
    printf("ERROR: This condition is to be deleted.\n");
    sprintf(response, "e%d", TOBEDELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  } 

  // Check that the lock has not been deleted
  else if (lcks[lockIndex].isDeleted) {
    printf("ERROR: This lock has been deleted.\n");
    sprintf(response, "e%d", DELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  } 

  // Check that the lock is not to be deleted
  else if (lcks[lockIndex].toBeDeleted) {
    printf("ERROR: This lock is to be deleted.\n");
    sprintf(response, "e%d", TOBEDELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }
  // Check that we have the correct waiting lock
  else if(cvs[cvIndex].condition.lockIndex != lockIndex && cvs[cvIndex].condition.lockIndex != -1) {
    DEBUG('b', "Correct: %i, Called: %i\n", cvs[cvIndex].condition.lockIndex, lockIndex);
    printf("ERROR: this is not the correct waiting lock.\n");
    sprintf(response, "e%d", WRONGLOCK);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }
  
  DEBUG('s', "\nNo errors yet, yay! \n \n");
  cvs[cvIndex].numActiveThreads++;   // Add a thread using the condition

  // 1st thread to wait on lock
  if(cvs[cvIndex].condition.lockIndex == -1) {
    DEBUG('s', "Entered 1st thread to wait on lock\n");
    cvs[cvIndex].condition.lockIndex = lockIndex;
    thread->machineID = inPacketHeader.from;
    thread->mailboxNum = inMailHeader.from;
    cvs[cvIndex].condition.waitList->Append((void *)thread);
    
    // Waitlist is not empty
    if(!(lcks[lockIndex].lock.waitList->IsEmpty())) {
      DEBUG('s', "Lock wait list is not empty\n");
      thread = (waitingThread*)lcks[lockIndex].lock.waitList->Remove();
      lcks[lockIndex].lock.machineID = thread->machineID;
      lcks[lockIndex].lock.mailboxNum = thread->mailboxNum;
      char* response2 = new char[MAX_SIZE];
      sprintf(response2, "%d", SUCCESS); //Changed %d to s%d
      DEBUG('s', "Response going to %i: %s\n", thread->mailboxNum, response2);
      outMailHeader.to = thread->mailboxNum;
      outPacketHeader.to = thread->machineID;
      outMailHeader.length = strlen(response2) + 1;
      postOffice->Send(outPacketHeader, outMailHeader, response2);
      lcks[lockIndex].numActiveThreads--;
    }
    // Waitlist is empty
    else {
      //WORKS
      DEBUG('s', "Lock wait list is empty\n");
      lcks[lockIndex].lock.available = true;
      lcks[lockIndex].lock.machineID = -1;
      lcks[lockIndex].lock.mailboxNum = -1;
      lcks[lockIndex].numActiveThreads = 0;
    }
  }
  //All tests passed, time to wait
  else{
    DEBUG('s', "All tests passed, time to wait!\n");
    thread->machineID = inPacketHeader.from;
    thread->mailboxNum = inMailHeader.from;
    cvs[cvIndex].condition.waitList->Append((void *)thread);

    if(!(lcks[lockIndex].lock.waitList->IsEmpty())) {
      DEBUG('s', "WaitList is not empty, getting next thread\n");
      thread = (waitingThread*)lcks[lockIndex].lock.waitList->Remove();
      lcks[lockIndex].lock.machineID = thread->machineID;
      lcks[lockIndex].lock.mailboxNum = thread->mailboxNum;
      char* response2 = new char[MAX_SIZE];
      sprintf(response2, "%d", SUCCESS);
      outMailHeader.to = thread->mailboxNum;
      outPacketHeader.to = thread->machineID;
      outMailHeader.length = strlen(response) + 1;
      postOffice->Send(outPacketHeader, outMailHeader, response2);
      lcks[lockIndex].numActiveThreads--;
    }
    //Empty queue, how in the wait cycle!
    else {
      lcks[lockIndex].lock.available = true;
      lcks[lockIndex].lock.machineID = -1;
      lcks[lockIndex].lock.mailboxNum = -1;
      lcks[lockIndex].numActiveThreads--;
    }
  }
}

// Signal the cv with the given msg
void Signal(char* msg) {
  char* index = new char[MAX_SIZE];
  char* response = new char[MAX_SIZE];
  waitingThread *thread = new waitingThread;
  thread->machineID = inPacketHeader.from;
  thread->mailboxNum = inMailHeader.from;

  // Calculate the data from the msg
  for(int i = 3; i < MAX_SIZE + 1; i++)
    index[i-3] = msg[i];
  int i = 1;
  int j = strlen(index) - 1;
  int cvIndex = 0;
  int lockIndex = 0;
  DEBUG('s', "SIGNAL: FROM: %i MSG: %s, INDEX: %s\n", inMailHeader.from, msg, index);
  while(index[j] != 'l'){
    DEBUG('b', "SIGNAL: IN LOCK LOOP; index[j]: %i\n", index[j]);
    lockIndex += (index[j]-48)*i;
    i = i*10;
    j--;
  }
  i = 1;
  j--;
  while(j>-1){
    DEBUG('b', "SIGNAL: IN CV LOOP; index[j]: %i\n", index[j]);
    cvIndex += (index[j]-48)*i;
    i = i*10;
    j--;
  }  
  DEBUG('s', "SIGNAL: CVINDEX: %i, LOCKINDEX: %i\n", cvIndex, lockIndex);
  
  // Check that the cv index is valid
  if(cvIndex < 0 || cvIndex >= nextCV) {
    printf("ERROR: The given cv index is not valid.\n");
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  } 

  // Check that the lock index is valid
  else if (lockIndex < 0 || lockIndex >= nextLock) {
    printf("ERROR: The given lock index is not valid.\n");
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  } 

  // Check that the cv has not been deleted
  else if (cvs[cvIndex].isDeleted) {
    printf("ERROR: This condition has been deleted.\n");
    sprintf(response, "e%d", DELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  } 

  // Check that the lock has not been deleted
  else if (lcks[lockIndex].isDeleted) {
    printf("ERROR: This lock has been deleted.\n");
    sprintf(response, "e%d", DELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  } 
  
  // Check that the cv has waiting threads
  else if(cvs[cvIndex].numActiveThreads < 1){
    printf("ERROR: This condition variable has no threads waiting\n");
    sprintf(response, "e%d", EMPTYLIST);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);	
    return;
  }

  // Check that the waitList is not empty
  else if(cvs[cvIndex].condition.waitList->IsEmpty()) {
    printf("ERROR: waitList is empty!\n");
    sprintf(response, "e%d", EMPTYLIST);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  // Check that we have the right lock
  else if(cvs[cvIndex].condition.lockIndex != lockIndex) {
    printf("ERROR: Wrong lock.\n");
    sprintf(response, "e%d", WRONGLOCK);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }
  
  cvs[cvIndex].numActiveThreads--; 
  thread = (waitingThread *)cvs[cvIndex].condition.waitList->Remove();
  lcks[lockIndex].lock.waitList->Append((void *)thread);  

  // If the waitlist is now empty, make the lock index invalid
  if(cvs[cvIndex].condition.waitList->IsEmpty()) {
    cvs[cvIndex].condition.lockIndex = -1;
  }  

  // Check if it should now be deleted
  if(cvs[cvIndex].toBeDeleted && (cvs[cvIndex].numActiveThreads == 0)) {
    cvs[cvIndex].isDeleted = true;
    printf("Deleted condition %i.\n", cvIndex);
  }
	
  //Release will wake waiting client up, so message goes to caller of signal
  sprintf(response, "s%d", SUCCESS);
  outMailHeader.length = strlen(response) + 1;
  DEBUG('s', "Signalling mailbox %i with %s\n", outMailHeader.to, response);
  postOffice->Send(outPacketHeader, outMailHeader, response);
}

// Broadcast on the cv in the given msg
void Broadcast(char* msg) {
  waitingThread *thread = new waitingThread;
  char* index = new char[MAX_SIZE];
  char* response = new char[MAX_SIZE];

  // Pull the data out of the message
  for(int i = 3; i < MAX_SIZE + 1; i++)
    index[i-3] = msg[i];
  int i = 1;
  int j = strlen(index) - 1;
  int cvIndex = 0;
  int lockIndex = 0;
  DEBUG('s', "BROADCAST: MSG: %s, INDEX: %s\n", msg, index);
  while(index[j] != 'l'){
    DEBUG('s', "BROADCAST: IN LOCK LOOP; index[j]: %i\n", index[j]);
    lockIndex += (index[j]-48)*i;
    i = i*10;
    j--;
  }
  i = 1;
  j--;
  while(j>-1){
    DEBUG('s', "BROADCAST: IN CV LOOP; index[j]: %i\n", index[j]);
    cvIndex += (index[j]-48)*i;
    i = i*10;
    j--;
  }  
  DEBUG('s', "BROADCAST: CVINDEX: %i, LOCKINDEX: %i\n", cvIndex, lockIndex);

  // Validate the CV index
  if(cvIndex < 0 || cvIndex >= nextCV) {
    printf("ERROR: The given cv index is not valid.\n");
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  } 

  // Check that the lock index is valid
  else if (lockIndex < 0 || lockIndex >= nextLock) {
    printf("ERROR: The given lock index is not valid.\n");
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  } 

  // Check that the CV is not deleted
  else if (cvs[cvIndex].isDeleted) {
    printf("ERROR: This condition has been deleted.\n");
    sprintf(response, "e%d", DELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  } 

  // Check that the lock is not deleted
  else if (lcks[lockIndex].isDeleted) {
    printf("ERROR: This lock has been deleted.\n");
    sprintf(response, "e%d", DELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }  

  // Check that the waitlist is not empty
  else if(cvs[cvIndex].condition.waitList->IsEmpty()) {
    printf("ERROR: waitList is empty!\n");
    sprintf(response, "e%d", EMPTYLIST);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  // Check that the lock is the right lock
  else if(cvs[cvIndex].condition.lockIndex != lockIndex) {
    printf("ERROR: Wrong lock.\n");
    sprintf(response, "e%d", WRONGLOCK);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  // Broadcasting to the waiting threads
  while(!cvs[cvIndex].condition.waitList->IsEmpty()) {
    DEBUG('s', "Broadcast: Waiting for waitList to empty\n");
    thread = (waitingThread *)cvs[cvIndex].condition.waitList->Remove();
    lcks[lockIndex].lock.waitList->Append((void *)thread);  
  }
  
  cvs[cvIndex].numActiveThreads = 0;
  cvs[cvIndex].condition.lockIndex = -1;
  
  // Check if we should now delete the cv
  if(cvs[cvIndex].toBeDeleted && (cvs[cvIndex].numActiveThreads == 0)) {
    cvs[cvIndex].isDeleted = true;
    printf("Deleted condition %i.\n", cvIndex);
  }
  sprintf(response, "%d", SUCCESS);
  outMailHeader.length = strlen(response) + 1;
  postOffice->Send(outPacketHeader, outMailHeader, response);
}

// Destroy the CV in the msg
void DestroyCV(char* msg) {
  DEBUG('r', "Entered DestroyCV\n");
  char* index = new char[MAX_SIZE-3];
  char* response = new char[MAX_SIZE];
  DEBUG('r', "Msg: %s\n", msg);
	
  // Pull the data out of the message
  for(int i = 3; i < MAX_SIZE + 1; i++)
    index[i-3] = msg[i];
  DEBUG('r', "index: %s\n", index);
  int i = 1;
  int j = strlen(index);
  int cvIndex = 0;
	
  for(j; j >=0; j--){
    if(index[j] != '\0') {
      cvIndex += (index[j]-48)*i;
      i = i*10;
    }  
  }
	
  DEBUG('r', "cvIndex: %i\n", cvIndex);

  // Check that the CV index is valid
  if(cvIndex < 0 || cvIndex >= nextCV) {
    printf("CV has a bad index!\n");
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  // Check that the CV has not been deleted
  else if(cvs[cvIndex].isDeleted) {
    printf("CV is already deleted!\n");
    sprintf(response, "e%d", DELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  // Check that the CV is not already to be deleted
  else if(cvs[cvIndex].toBeDeleted) {
    printf("CV is to be deleted!\n");
    sprintf(response, "e%d", TOBEDELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  // Check that the CV does not have active threads running
  else if(cvs[cvIndex].numActiveThreads > 0) {
    printf("CV has threads still waiting on it, set to be deleted!\n");
    cvs[cvIndex].toBeDeleted = true;
    sprintf(response, "e%d", TOBEDELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  // Delete the CV
  else {
    printf("CV at %i is deleted!\n", cvIndex);
    cvs[cvIndex].isDeleted = true;
    sprintf(response, "s%d", SUCCESS);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
  }
  return;

}

// Create the MV with the given name
void CreateMV(char* msg) {

  mvData myMV;

  char* response = new char[MAX_SIZE];
  char* name = new char[MAX_SIZE - 3];

  // Check if the array size is in the 10s digit and pull out the name
  if(msg[2] == 't') {
    myMV.size = (msg[3]-48);
    for(int i = 5; i < MAX_SIZE + 1; i++) {
      name[i-5] = msg[i];
    }
  }
  // Check if the array size is in the 100s digit and pull out the name
  else if(msg[2] == 'h') {
    myMV.size = ((msg[3]*10-48) + (msg[4]-48));
    for(int i = 6; i < MAX_SIZE + 1; i++) {
      name[i-6] = msg[i];
    }
  }
  // Check if the array size is in the 1000s digit and pull out the name
  else if(msg[2] == 'k') {
    myMV.size = ((msg[3]*100-48) + (msg[4]*10-48) + (msg[5]-48));
    for(int i = 7; i < MAX_SIZE + 1; i++) {
      name[i-7] = msg[i];
    }
  }  

  // Check the the MV already exists
  for(int i = 0; i < nextMVPos; i++) {
    if(strcmp(mvs[i].name,name) == 0) {
      DEBUG('s', "CreateMV name found %s at index %i\n", name, i);
      sprintf(response, "s%d", i);
      outMailHeader.length = strlen(response) + 1;
      postOffice->Send(outPacketHeader,outMailHeader, response);
      return;
    }
  } 

  // Initialize all the values
  for(int i = 0; i < 1000; i++) {
    myMV.values[i] = 0;
  }
  myMV.isDeleted = false;
  myMV.name = new char[MAX_SIZE];
  
  sprintf(myMV.name, "%s", name);

  if(nextMVPos >= MAX_MVS) {
    sprintf(myMV.name, "e%d", MAX_MVS);
  }
  else {
    mvs[nextMVPos] = myMV;
    sprintf(response, "s%d", nextMVPos);
  }
 	DEBUG('s', "CreateMV position: %i\n", nextMVPos);
  outMailHeader.length = strlen(response) + 1;

  bool success = postOffice->Send(outPacketHeader, outMailHeader, response);
  if(!success) {
     printf("The postOffice send failed. Terminating Nachos.\n");
     interrupt->Halt();
  }

  nextMVPos++;
}

// Set MV[index]'s value for the given array index
void Set(int index, int value, int arrayIndex) {

  char* response = new char[MAX_SIZE];
  DEBUG('s', "Set index: %i, value: %i, arrayIndex: %i\n", index, value, arrayIndex);

  // Check that the MV index is valid
  if(index < 0 || index >= nextMVPos) {
    DEBUG('s', "Set: Bad index\n");
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  // Check that the index into the array is valid
  if(arrayIndex < 0 || arrayIndex >= mvs[index].size) {
    DEBUG('s', "Set: Bad arrayIndex\n");
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  // Check that the MV is not deleted
  if(mvs[index].isDeleted) {
    DEBUG('s', "Set: Is Deleted\n");
    sprintf(response, "e%d", DELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }
  // Change the value
  mvs[index].values[arrayIndex] = value;
  DEBUG('s', "Set value: %i, index: %i\n", mvs[index].values[arrayIndex], index);
  sprintf(response, "s");
  outMailHeader.length = strlen(response) + 1;
  postOffice->Send(outPacketHeader, outMailHeader, response);
  return;
}

// Get the value from the MV[index] with the given array index
void Get(int index, int arrayIndex) {

  char* response = new char[MAX_SIZE];

  // Check that the MV index is valid
  if(index < 0 || index >= nextMVPos) {
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  // Check that the array index is valid
  if(arrayIndex < 0 || arrayIndex >= mvs[index].size) {
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  // Check that the MV is not already deleted
  if(mvs[index].isDeleted) {
    sprintf(response, "e%d", DELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }
  DEBUG('s', "Get value: %i, index: %i, array index: %i\n", mvs[index].values[arrayIndex], index, arrayIndex);
  sprintf(response, "s%d", mvs[index].values[arrayIndex]);
  DEBUG('s', "Get: Sending back response: %s\n", response);
  outMailHeader.length = strlen(response) + 1;
  postOffice->Send(outPacketHeader, outMailHeader, response);
  return;
}

// Destroy the MV with the given index
void DestroyMV(int index) {

  char* response = new char[MAX_SIZE];

  // Check that the given MV index is valid
  if(index < 0 || index >= nextMVPos) {
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  // Check that the MV is not deleted
  else if(mvs[index].isDeleted) {
    sprintf(response, "e%d", DELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  // Delete the MV
  else {
    mvs[index].isDeleted = true;
    sprintf(response, "s");
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
  }
  return;
}

void Identify(){
	int id;
	char* response = new char[MAX_SIZE];

	outPacketHeader.to = inPacketHeader.from;
	outMailHeader.to = inMailHeader.from;
	DEBUG('s', "inMailHeader.from: %i, outMailHeader.to: %i\n", inMailHeader.from,outMailHeader.to);
	sprintf(response, "i%d", outMailHeader.to);
	outMailHeader.length = strlen(response)+1;

	postOffice->Send(outPacketHeader, outMailHeader, response);
}

void CheckTerminated() {
	bool success;
	char* msg = new char[MAX_SIZE];
  waitingThread* thread;
	
	DEBUG('s', "Checking terminated processes\n");
	for(int i = 0; i<nextLock; i++){
		if(!lcks[i].lock.available){
			outPacketHeader.to = lcks[i].lock.machineID;
			outMailHeader.to = lcks[i].lock.mailboxNum;
			msg = "t";
			DEBUG('s', "outPacketHeader.to: %i, outMailHeader.to: %i\n", outPacketHeader.to,outMailHeader.to);
			success = postOffice->Send(outPacketHeader, outMailHeader, msg);
			DEBUG('s', "Success %d\n", success);
			if(!success){			
				DEBUG('s', "Found a dead thread on lock %i\n", i);
				//Release lock from dead thread and give to next waiting thread
				if(!(lcks[i].lock.waitList->IsEmpty())) {
					thread = (waitingThread*)lcks[i].lock.waitList->Remove();
					char* response2 = new char[MAX_SIZE];
					sprintf(response2, "%d", SUCCESS);
					outMailHeader.to = thread->mailboxNum;
					outPacketHeader.to = thread->machineID;
					outMailHeader.length = strlen(response2) + 1;
					postOffice->Send(outPacketHeader, outMailHeader, response2); // Tell the thread that now has the lock that it has successfully acquired
				}
				else {
					// Make the lock available
					lcks[i].lock.available = true;
					lcks[i].lock.machineID = -1;
					lcks[i].lock.mailboxNum = -1;
				}
			}
			else{
				DEBUG('s',"Process still exists on lock %i\n", i);
			}
		}
	}
	return;
}

void Server() {

  while(true) {
    
    printf("***\nEntered Server loop.\n");
    message = new char[MAX_SIZE];
//    CheckTerminated();
    postOffice->Receive(0, &inPacketHeader, &inMailHeader, message);

    printf("Received a message.\n");
		DEBUG('s', "Message from: %i\n", inMailHeader.from);
    outPacketHeader.to = inPacketHeader.from;
    outMailHeader.to = inMailHeader.from;

    if(message[1] == CREATELOCK && message[2] == 'n') {   
      DEBUG('s', "Got a CreateLock message.\n");
      CreateLock(message);
    } 
    else if (message[1] == ACQUIRE && message[2] == 'i') {
			DEBUG('s', "Got a AcquireLock message.\n");
      int i = 1;
      int j = strlen(message);
      int total = 0;
      
      while(message[j] != 'i') {
				if(message[j] != '\0') {
	  			total += (message[j]-48)*i;
				  i=i*10;
				}
			j--;
      }
      Acquire(total);
    } 
    else if (message[1] == RELEASE && message[2] == 'i') {
			DEBUG('s', "Got a ReleaseLock message.\n");
      int i = 1;
      int j = strlen(message);
      int total = 0;
      
      while(message[j] != 'i') {
				if(message[j] != '\0') {
				  total += (message[j]-48)*i;
	  			i=i*10;
				}
				j--;
      }
      Release(total);
    } 
    else if (message[1] == DESTROYLOCK && message[2] == 'i') {
			DEBUG('s', "Got a DestroyLock message.\n");
      int i = 1;
      int j = strlen(message);
      int total = 0;
      
      while(message[j] != 'i') {
				if(message[j] != '\0') {
					total += (message[j]-48)*i;
					i=i*10;
				}
				j--;
			}
      DestroyLock(total); 
    } 
    else if (message[1] == CREATECV && message[2] == 'n') {
			DEBUG('s', "Got a CreateCV message.\n");
      CreateCV(message);
    }
    else if (message[1] == WAIT && message[2] == 'c') {
			DEBUG('s', "Got a Wait message.\n");
      Wait(message);
    }
    else if (message[1] == SIGNAL && message[2] == 'c') {
			DEBUG('s', "Got a Signal message.\n");
      Signal(message);
    }
    else if (message[1] == BROADCAST && message[2] == 'c') {
			DEBUG('s', "Got a Broadcast message.\n");
      Broadcast(message);
    }
    else if (message[1] == DESTROYCV && message[2] == 'n') {
			DEBUG('s', "Got a DestroyCV message.\n");
      DestroyCV(message);
    }
    else if (message[1] == CREATEMV && (message[2] == 't' || message[2] == 'h' || message[2] == 'k')) {
			DEBUG('s', "Got a CreateMV message.\n");
      CreateMV(message);
    }
    else if ((message[1]-48) == GET1 && (message[2]-48)==GET2 && message[3]=='i') {
			DEBUG('s', "Got a Get message.\n");
      int i = 1;
      int j = strlen(message);
      int index = 0;
      int arrayIndex = 0;

      while(message[j] != 'a') {
				if(message[j] != '\0') {
					arrayIndex += (message[j]-48)*i;
					i=i*10;
				}
				j--;
		  }
      j--;
      i=1;
      while(message[j] != 'i') {
				index += (message[j]-48)*i;
				i=i*10;
				j--;
      }

      Get(index, arrayIndex);
    }
    else if ((message[1]-48)==SET1 && (message[2]-48)==SET2 && message[3]=='i') {
			DEBUG('s', "Got a Set message: %s\n", message);
      int i = 1;
      int j = strlen(message);
      int arrayIndex = 0;
      int value = 0;
      int index = 0;

      while(message[j] != 'a') {
				if(message[j] != '\0') {
					arrayIndex += (message[j]-48)*i;
					i=i*10;
				}
				j--;
						}
						j--;
						i = 1;
						while(message[j] != 'i') {
						  value += (message[j]-48)*i;
				i = i*10;
				j--;
						}
						j--;
						i = 1;
						while(message[j] != 'i') {
				index += (message[j]-48)*i;
				i=i*10;
				j--;
      }

      Set(index, value, arrayIndex);
    }
    else if ((message[1]-48)==DESTROYMV1 && (message[2]-48)==DESTROYMV2 && message[3]=='i') {
			DEBUG('s', "Got a DestroyMV message.\n");
      int i = 1;
      int j = strlen(message);
      int total = 0;
      
      while(message[j] != 'i') {
				if(message[j] != '\0') {
					total += (message[j]-48)*i;
					i=i*10;
				}
				j--;
      }
      DestroyMV(total);
    }
		else if (message[1]=='3' && message[2]=='4'){
			DEBUG('s', "Got an Identify message.\n");
			Identify();
		}
    else {
      printf("ERROR: Received an invalid message.\n");
    }
  }
}


