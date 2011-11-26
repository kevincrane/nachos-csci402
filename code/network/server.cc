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
//    CheckTerminated();  // *** PART 3
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

/* PART 2
// Server Application

#include "system.h"
#include "network.h"
#include "post.h"
#include "interrupt.h"
#include "utility.h"
#include "synch.h"

#define MAX_LOCKS 1000
#define MAX_CVS 1000
#define MAX_MVS 1000
#define MAX_PROCESSES 1000
#define MAX_REQUESTS 1000
#define MAX_SIZE 20
#define MAX_SERVERS 5

#define CREATELOCK 48
#define ACQUIRE 49
#define RELEASE 50
#define DESTROYLOCK 51
#define CREATECV 52
#define WAIT 53
#define SIGNAL 54
#define BROADCAST 55
#define DESTROYCV 56
#define CREATEMV 57
#define SET1 49
#define SET2 48
#define GET1 49
#define GET2 49
#define SET 10
#define GET 11
#define DESTROYMV 12
#define DESTROYMV1 49
#define DESTROYMV2 50
#define BADINDEX 13
#define WRONGPROCESS 14
#define DELETED 15
#define TOBEDELETED 16
#define SUCCESS 17
#define WRONGLOCK 18
#define EMPTYLIST 19
#define OWNER 20
#define WAITING 21

int nextLock = 0;  // Next lock position in the array
int nextCV = 0;    // Next CV position in the array
int nextMVPos = 0; // Next MV position in the array
int timestamp = 0; // Keep track of when messages were sent

PacketHeader inPacketHeader;
PacketHeader outPacketHeader;
MailHeader inMailHeader;
MailHeader outMailHeader;

// Server version of the lock class
class serverLock {
	
public:
  char* name; // Name of the lock
  bool available; // If it is currently being used
  int machineID;
  int mailboxNum;
  List* waitList; // those waiting for the lock
  
  // Default constructor
  serverLock::serverLock() {
    name = new char[MAX_SIZE];
    available = true;
    machineID = -1;
    mailboxNum = -1;
    waitList = new List();
  }
};

// Server version of the CV class
class serverCV {

public:
  char* name; // Name of the CV
  int lockIndex; // Index of the lock belonging to the CV
  int machineID;
  int mailboxNum;
  List* waitList; // Waiting threads
	
  // Default constructor
  serverCV::serverCV() {
    name = new char[MAX_SIZE];
    lockIndex = -1;
    machineID = -1;
    mailboxNum = -1;
    waitList = new List();
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

// Contains all information on a MV
struct mvData {
  int values[1000]; // Assume max array size is 1000
  int size;
  bool isDeleted;
  char* name;
};

struct PendingRequest {
  char* name;
  int type;
  int numberOfNoReplies;
  char* originalClientMsg;
  int clientMachineID;
  int clientMailboxID;
  int timestamp;
  List* pendingDYH;
  bool active;
};

struct waitingMsg {
  char* msg;
  int fromMachineID;
  int fromMailboxID;
  int timestamp;
};

waitingMsg waitingMsgs[MAX_REQUESTS];
lockData lcks[MAX_LOCKS]; // Array containing all locks for this server
cvData cvs[MAX_CVS]; // Array containing all CVs for this server
mvData mvs[MAX_MVS]; // Array containing all MVs for this server
PendingRequest pendingRequests[MAX_REQUESTS];

int nextPR = 0;
int nextMsg = 0 ;

void CreateLock(char* name, int machineID, int mailboxNum) {
	
  printf("Entered CreateLock.\n");

  outPacketHeader.to = machineID;
  outMailHeader.to = mailboxNum;

  lockData myLock;
  char* response = new char[MAX_SIZE];
	
  // Check if the lock name already exists, if so return the existing index
  for(int i = 0; i < nextLock; i++) {
    if(*lcks[i].lock.name == *name) {
      sprintf(response, "s%d", i);
      outMailHeader.length = strlen(response) + 1;
      postOffice->Send(outPacketHeader, outMailHeader, response);
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
 	
  // Check taht we haven't created too many locks
  if(nextLock >= MAX_LOCKS) {
    sprintf(myLock.lock.name, "e%d", MAX_LOCKS);
  }
  else {
    lcks[nextLock] = myLock;
    sprintf(response, "s%d", nextLock);
  }
  
  outMailHeader.length = strlen(response) + 1;
	
  printf("Sending response %s to %i %i.\n", response, outPacketHeader.to, outMailHeader.to);
  postOffice->Send(outPacketHeader, outMailHeader, response);
  
  nextLock++;
}

// Acquire the lock with the given index
void Acquire(int index, int machineID, int mailboxNum) {
  char* response = new char[MAX_SIZE];
  outMailHeader.to = mailboxNum;
  outPacketHeader.to = machineID;
	
	// Check that the lock index is valid
	if(index < 0 || index >= nextLock) {
		printf("ERROR: Bad index on acquire.\n");
		sprintf(response, "e%d", BADINDEX);
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
		return;
	}
	
	// Check that the lock has not already been deleted
	else if(lcks[index].isDeleted) {
		printf("ERROR: Lock is already deleted.\n");
		sprintf(response, "e%d", DELETED);
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
		return;
	}
	
	// Check that the lock isn't to be deleted
	else if(lcks[index].toBeDeleted) {
		printf("ERROR: Lock is already to be deleted.\n");
		sprintf(response, "e%d", TOBEDELETED);
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
		return;
	}
	
	// Check if the thread is already the lock owner
	if(lcks[index].lock.machineID == machineID && lcks[index].lock.mailboxNum == mailboxNum) {
		printf("This thread already has the lock.\n");
		sprintf(response, "%d", OWNER);  // IS THIS RIGHT??????****************
		postOffice->Send(outPacketHeader, outMailHeader, response);
		return;
	}

	// Increment the number of threads
	lcks[index].numActiveThreads++;
	
	// Check if the lock is available
	if(lcks[index].lock.available) {
		lcks[index].lock.available = false;  // Make it unavailable
		lcks[index].lock.machineID = machineID;  // Change the owner
		lcks[index].lock.mailboxNum = mailboxNum;
		sprintf(response, "%d", SUCCESS);
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
		return;
	}
	// Add the thread to the wait queue
	else {
		waitingThread* wt = new waitingThread;
		wt->machineID = machineID;
		wt->mailboxNum = mailboxNum;
		lcks[index].lock.waitList->Append((void*)wt);
	}
}

void Release(int index, int machineID, int mailboxNum) {

	char* response = new char[MAX_SIZE];
	waitingThread* thread;
	outMailHeader.to = mailboxNum;
	outPacketHeader.to = machineID;
	
	// Check that the lock index is valid
	if(index < 0 || index >= nextLock) {
		printf("ERROR: Bad index on release.\n");
		sprintf(response, "e%d", BADINDEX);
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
		return;
	}
	
	// Check that the lock has not already been deleted
	else if(lcks[index].isDeleted) {
		printf("ERROR: This lock is already deleted.\n");
		sprintf(response, "e%d", DELETED);
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
		return;
	}
	
	lcks[index].numActiveThreads--; // A thread is no longer using this lock
	
	// Check if the lock was waiting to be deleted, if so, delete it
	if(lcks[index].toBeDeleted && (lcks[index].numActiveThreads = 0)) {
		lcks[index].isDeleted = true;
		printf("Deleted lock %i.\n", index);
	}
	
	// Check that this is the lock owner
	if(lcks[index].lock.machineID != machineID || lcks[index].lock.mailboxNum != mailboxNum) {
		printf("ERROR: Not the lock owner!\n");
		sprintf(response, "e%d", WRONGPROCESS);
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
		return;
	}
	
	// Check if the the wait list is empty, if not pull a thread off the wait list 
	// and give it the lock
	if(!(lcks[index].lock.waitList->IsEmpty())) {
		thread = (waitingThread*)lcks[index].lock.waitList->Remove();
		lcks[index].lock.machineID = thread->machineID;
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
		postOffice->Send(outPacketHeader, outMailHeader, response); // tell the thread it has successfully release the lock
	}
	return;
}

// Destroy the lock with the given index
void DestroyLock(int index, int machineID, int mailboxNum) {
	char* response = new char[MAX_SIZE];
	
	outMailHeader.to = mailboxNum;
	outPacketHeader.to = machineID;
	
	// Check that the lock index is valid
	if(index < 0 || index >= nextLock) {
		printf("ERROR: Bad index on destroy lock.\n");
		sprintf(response, "e%d", BADINDEX);
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
		return;
	}
	
	// Check that the lock has not been deleted
	else if(lcks[index].isDeleted) {
		printf("ERROR: This lock is already deleted.\n");
		sprintf(response, "e%d", DELETED);
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
		return;
	}
	
	// Check that the lock is not already to be deleted
	else if(lcks[index].toBeDeleted) {
		printf("ERROR: This lock is already to be deleted.\n");
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

void CreateCV(char* name, int machineID, int mailboxID) {

	cvData myCV;
	char* response = new char[MAX_SIZE];
	
	outMailHeader.to = mailboxID;
	outPacketHeader.to = machineID;
	
	// Check if CV name already exists
	for(int i = 0; i < nextCV; i++) {
		if(strcmp(cvs[i].condition.name, name)==0 && !cvs[i].isDeleted) {
			printf("Names match, name: %s, returning index value of %i.\n\n", name, i);
			sprintf(response, "s%d", i);
			outMailHeader.length = strlen(response) + 1;
			postOffice->Send(outPacketHeader, outMailHeader, response);
			return;
		}
	}
	
	// Initialize the data
	myCV.condition.name = name;
	myCV.isDeleted = false;
	myCV.toBeDeleted = false;
	myCV.numActiveThreads = 0;
	
	// Check that we haven't exceeded the max CV number
	if(nextCV >= MAX_CVS)
		sprintf(myCV.condition.name, "e%d", MAX_CVS);
	else {
		cvs[nextCV] = myCV;
		sprintf(response, "s%d", nextCV);
	}
	
	outMailHeader.length = strlen(response) + 1;
	
	printf("CV created at position: %i.\n\n", nextCV);
	postOffice->Send(outPacketHeader, outMailHeader, response);
	nextCV++;
}

void Wait(int cvIndex, int lockIndex, int machineID, int mailboxID) {

	char* response = new char[MAX_SIZE];
	waitingThread *thread = new waitingThread;
	thread->machineID = machineID;
	thread->mailboxNum = mailboxID;
	outMailHeader.to = mailboxID;
	outPacketHeader.to = machineID;
	
	// Check that the index is valid
	if(cvIndex < 0 || cvIndex >= nextCV) {
		printf("ERROR: The given CV index is not valid.\n");
		sprintf(response, "e%d", BADINDEX);
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
		return;
	}
	// Check that the lock index is valid
	else if(lockIndex < 0 || lockIndex >= nextLock) {
		printf("ERROR: The given lock index is not valid.\n");
		sprintf(response, "e%d", BADINDEX);
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
		return;
	}
	// Check that the CV has not been deleted
	else if(cvs[cvIndex].isDeleted) {
		printf("ERROR: This condition has been deleted.\n");
		sprintf(response, "e%d", DELETED);
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
		return;
	}
	// Check that the CV is not to be deleted
	else if(cvs[cvIndex].toBeDeleted) {
		printf("ERROR: This condition is to be deleted.\n");
		sprintf(response, "e%d", TOBEDELETED);
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
		return;
	}
	// Check that the lock has not been deleted
	else if(lcks[lockIndex].isDeleted) {
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

void Signal(int cvIndex, int lockIndex, int machineID, int mailboxID) {
	
	char* response = new char[MAX_SIZE];
	waitingThread *thread = new waitingThread;
	thread->machineID = machineID;
	thread->mailboxNum = mailboxID;
	outPacketHeader.to = machineID;
	outMailHeader.to = mailboxID;
	
	// Check that the cv index is valid 
	if(cvIndex < 0 || cvIndex >= nextCV) {
		printf("ERROR: The given CV index is not valid.\n");
		sprintf(response, "e%d", BADINDEX);
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
		return;
	}
	// Check that the lock index is valid
	else if(lockIndex < 0 || lockIndex >= nextLock) {
		printf("ERROR: The given lock index is not valid.\n");
		sprintf(response, "e%d", BADINDEX);
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
		return;
	}
	// Check that the cv has not been deleted
	else if(cvs[cvIndex].isDeleted) {
		printf("ERROR: This condition has been deleted.\n");
		sprintf(response, "e%d", DELETED);
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
		return;
	}
	// Check that the lock has not been deleted
	else if(lcks[lockIndex].isDeleted) {
		printf("ERROR: This lock has been deleted.\n");
		sprintf(response, "e%d", DELETED);
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
		return;
	}
	// Check that the cv has waiting threads
	else if(cvs[cvIndex].numActiveThreads < 1) {
		printf("ERROR: This condition variable has no threads waiting.\n");
		sprintf(response, "e%d", EMPTYLIST);
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
		return;
	}
	// Check that the waitlist is not empty
	else if(cvs[cvIndex].condition.waitList->IsEmpty()) {
		printf("ERROR: waitlist is empty!\n");
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
	
	// Release will wake waiting client up, so message goes to caller of signal
	sprintf(response, "s%d", SUCCESS);
	outMailHeader.length = strlen(response) + 1;
	postOffice->Send(outPacketHeader, outMailHeader, response);
}

void Broadcast(int cvIndex, int lockIndex, int machineID, int mailboxID) {
	waitingThread *thread = new waitingThread;
	char* response = new char[MAX_SIZE];
	
	outPacketHeader.to = machineID;
	outMailHeader.to = mailboxID;
	
	// Validate the CV index
	if(cvIndex < 0 || cvIndex >= nextCV) {
		printf("ERROR: The given CV index is not valid.\n");
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

void DestroyCV(int cvIndex, int machineID, int mailboxID) {
  char* response = new char[MAX_SIZE];
	
  outPacketHeader.to = machineID;
  outMailHeader.to = mailboxID;

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

void CreateMV(char* name, int machineID, int mailboxID) {
	mvData myMV;
	outPacketHeader.to = machineID;
	outMailHeader.to = mailboxID;
	
	char* response = new char[MAX_SIZE];
	
	// Check if the MV already exists
	for(int i = 0; i < nextMVPos; i++) {
		if(*mvs[i].name == *name) {
			sprintf(response, "s%d", i);
			outMailHeader.length = strlen(response) + 1;
			postOffice->Send(outPacketHeader, outMailHeader, response);
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
	
	outMailHeader.length = strlen(response) + 1;
	
	bool success = postOffice->Send(outPacketHeader, outMailHeader, response);
	if(!success) {
		printf("The postOffice send failed. Terminating Nachs.\n");
		interrupt->Halt();
	}
	nextMVPos++;
}

void Set(int index, int value, int arrayIndex, int machineID, int mailboxID) {
	char* response = new char[MAX_SIZE];
	outPacketHeader.to = machineID;
	outMailHeader.to = mailboxID;
	
	// Check that the MV index is valid
	if(index < 0 || index >= nextMVPos) {
		sprintf(response, "e%d", BADINDEX);
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
		return;
	}
	// Check that the index into the array is valid
	if(arrayIndex < 0 || arrayIndex >= mvs[index].size - 1) {
		sprintf(response, "e%d", BADINDEX);
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
		return;
	}
	
	// Check that the MV is not deleted
	if(mvs[index].isDeleted) {
		sprintf(response, "e%d", DELETED);
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
		return;
	}
	// Change the value
	mvs[index].values[arrayIndex] = value;
	sprintf(response, "s");
	outMailHeader.length = strlen(response) + 1;
	postOffice->Send(outPacketHeader, outMailHeader, response);
	return;
}

void Get(int index, int arrayIndex, int machineID, int mailboxID) {

	char* response = new char[MAX_SIZE];
	outPacketHeader.to = machineID;
	outMailHeader.to = mailboxID;
	
	// Check that the MV index is valid
	if(index < 0 || index >= nextMVPos) {
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  // Check that the array index is valid
  if(arrayIndex < 0 || arrayIndex >= mvs[index].size-1) {
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

void DestroyMV(int index, int machineID, int mailboxID) {
	char* response = new char[MAX_SIZE];
	outPacketHeader.to = machineID;
	outMailHeader.to = mailboxID;
	
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

void Identify() {
int id;
	char* response = new char[MAX_SIZE];

	outPacketHeader.to = inPacketHeader.from;
	outMailHeader.to = inMailHeader.from;
	DEBUG('s', "inMailHeader.from: %i, outMailHeader.to: %i\n", inMailHeader.from,outMailHeader.to);
	sprintf(response, "i%d", outMailHeader.to);
	outMailHeader.length = strlen(response)+1;

	postOffice->Send(outPacketHeader, outMailHeader, response);
}

void Server(int machineID, int numServers) {

  waitingMsg msgReceived;                // Msg that just came in
  waitingMsg currentMsg;                 // Msg I'm currently processing
  outPacketHeader.from = machineID;
  outMailHeader.from = 0;

  while(1) {

    printf("Entered server loop.\n");

    msgReceived.msg = new char[MAX_SIZE];

    postOffice->Receive(0, &inPacketHeader, &inMailHeader, msgReceived.msg); // Receive a message

    
    msgReceived.fromMailboxID = inMailHeader.from;
    msgReceived.fromMachineID = inPacketHeader.from;
    msgReceived.timestamp = 0;

    waitingMsgs[nextMsg] = msgReceived; // Add to the queue
    nextMsg++;
  
    // Pull off a waiting msg
    currentMsg = waitingMsgs[0];

    // Move the messages up in the queue
    for(int i = 0; i < nextMsg; i++) {
      waitingMsgs[i] = waitingMsgs[i+1];
    }
    nextMsg--;
 
    printf("Current message is %s.\n", currentMsg.msg);

    // This is a client message
    if(currentMsg.msg[0] == 's') {
      printf("Received a client message.\n");
			
      // This is a CreateLock requests
      if(currentMsg.msg[1] == CREATELOCK && currentMsg.msg[2] == 'n') {
	char* name = new char[MAX_SIZE];
				
	// Pull the name out of the message
	for(int i = 3; i < MAX_SIZE - 3; i++) {
	  name[i-3] = currentMsg.msg[i];
	}

	printf("Name is %s.\n", name);
		
	int index = -1;

	// Check if I have this lock
	for(int i = 0; i < nextLock; i++) {
	  if(*lcks[i].lock.name ==  *name) {
	    index = i;
	  }
	}
	
	if(index == -1) {
	  // I don't have this lock...ask the other servers

	  // Initialize the new pending request
	  pendingRequests[nextPR].type = CREATELOCK;
	  pendingRequests[nextPR].numberOfNoReplies = 0;
	  pendingRequests[nextPR].originalClientMsg = new char[MAX_SIZE];
					
	  sprintf(pendingRequests[nextPR].originalClientMsg, "%s", currentMsg.msg);
					
	  pendingRequests[nextPR].clientMachineID = currentMsg.fromMachineID;
	  pendingRequests[nextPR].clientMailboxID = currentMsg.fromMailboxID;
	  pendingRequests[nextPR].timestamp = 0;
	  pendingRequests[nextPR].active = true;
	  pendingRequests[nextPR].pendingDYH = new List();
	
	  char* response = new char[MAX_SIZE];
				
	  // Iterate through servers send them
	  for(int i = 0; i < numServers; i++) {
	    if(i != outPacketHeader.from) {
	      response = new char[MAX_SIZE];
	      sprintf(response, "dyht%di%dl%s", CREATELOCK-48, nextPR, name);
	      outMailHeader.length = strlen(response) + 1;
	      outPacketHeader.to = i;
	      outMailHeader.to = 0;
	      postOffice->Send(outPacketHeader, outMailHeader, response);
	      timestamp++;
	    }
	  }	
	  nextPR++;
	}
	else {
	  CreateLock(name, currentMsg.fromMachineID, currentMsg.fromMailboxID);
	}
      }
      // This is an Acquire request
      else if(currentMsg.msg[1] == ACQUIRE && currentMsg.msg[2] == 'n') {
	printf("Received an acquire request.\n");
	char* name = new char[MAX_SIZE];
	
	// Pull the name out of the message
	for(int i = 3; i < MAX_SIZE - 3; i++) {
	  name[i-3] = currentMsg.msg[i];
	}
	
	printf("Name is %s.\n", name);
	
	int index = -1;
	
	// Check if I have this lock
	for(int i = 0; i < nextLock; i++) {
	  if(*lcks[i].lock.name ==  *name) {
	    index = i;
	  }
	}
	
	if(index == -1) {
	  // I don't have this lock...ask the other servers

	  // Initialize the new pending request
	  pendingRequests[nextPR].type = ACQUIRE;
	  pendingRequests[nextPR].numberOfNoReplies = 0;
	  pendingRequests[nextPR].originalClientMsg = new char[MAX_SIZE];
	  
	  sprintf(pendingRequests[nextPR].originalClientMsg, "%s", currentMsg.msg);
					
	  pendingRequests[nextPR].clientMachineID = currentMsg.fromMachineID;
	  pendingRequests[nextPR].clientMailboxID = currentMsg.fromMailboxID;
	  pendingRequests[nextPR].timestamp = 0;
	  pendingRequests[nextPR].active = true;
	  pendingRequests[nextPR].pendingDYH = new List();
	  
	  char* response = new char[MAX_SIZE];
				
	  // Iterate through servers send them
	  for(int i = 0; i < numServers; i++) {
	    if(i != outPacketHeader.from) {
	      response = new char[MAX_SIZE];
	      sprintf(response, "dyht%di%dl%s", ACQUIRE - 48, nextPR, name);
	      outMailHeader.length = strlen(response) + 1;
	      outPacketHeader.to = i;
	      outMailHeader.to = 0;
	      postOffice->Send(outPacketHeader, outMailHeader, response);
	      timestamp++;
	    }
	  }	
	  nextPR++;
	}
	else {
	  Acquire(index, currentMsg.fromMachineID, currentMsg.fromMailboxID);
	}
      }
      // This is a Release request
      else if(currentMsg.msg[1] == RELEASE && currentMsg.msg[2] == 'i') {
	char* name = new char[MAX_SIZE];
	
	// Pull the name out of the message
	for(int i = 3; i < MAX_SIZE - 3; i++) {
	  name[i-3] = currentMsg.msg[i];
	}
	
	printf("Name is %s.\n", name);
	
	int index = -1;

	// Check if I have this lock
	for(int i = 0; i < nextLock; i++) {
	  if(*lcks[i].lock.name ==  *name) {
	    index = i;
	  }
	}
	
	if(index == -1) {
	  // I don't have this lock...ask the other servers

	  // Initialize the new pending request
	  pendingRequests[nextPR].type = RELEASE;
	  pendingRequests[nextPR].numberOfNoReplies = 0;
	  pendingRequests[nextPR].originalClientMsg = new char[MAX_SIZE];
	  
	  sprintf(pendingRequests[nextPR].originalClientMsg, "%s", currentMsg.msg);
	  
	  pendingRequests[nextPR].clientMachineID = currentMsg.fromMachineID;
	  pendingRequests[nextPR].clientMailboxID = currentMsg.fromMailboxID;
	  pendingRequests[nextPR].timestamp = 0;
	  pendingRequests[nextPR].active = true;
	  pendingRequests[nextPR].pendingDYH = new List();
	
	  char* response = new char[MAX_SIZE];
				
	  // Iterate through servers send them
	  for(int i = 0; i < numServers; i++) {
	    if(i != outPacketHeader.from) {
	      response = new char[MAX_SIZE];
	      sprintf(response, "dyht%di%dl%s", RELEASE-48, nextPR, name);
	      outMailHeader.length = strlen(response) + 1;
	      outPacketHeader.to = i;
	      outMailHeader.to = 0;
	      postOffice->Send(outPacketHeader, outMailHeader, response);
	      timestamp++;
	    }
	  }	
	  nextPR++;
	}
	else {
	  Release(index, currentMsg.fromMachineID, currentMsg.fromMailboxID);
	}
      }
      // This is a DestroyLock request
      else if(currentMsg.msg[1] == DESTROYLOCK && currentMsg.msg[2] == 'i') {
	char* name = new char[MAX_SIZE];
	
	// Pull the name out of the message
	for(int i = 3; i < MAX_SIZE - 3; i++) {
	  name[i-3] = currentMsg.msg[i];
	}
	
	printf("Name is %s.\n", name);
			
	int index = -1;
	
	// Check if I have this lock
	for(int i = 0; i < nextLock; i++) {
	  if(*lcks[i].lock.name ==  *name) {
	    index = i;
	  }
	}
	
	if(index == -1) {
	  // I don't have this lock...ask the other servers

	  // Initialize the new pending request
	  pendingRequests[nextPR].type = DESTROYLOCK;
	  pendingRequests[nextPR].numberOfNoReplies = 0;
	  pendingRequests[nextPR].originalClientMsg = new char[MAX_SIZE];
	  
	  sprintf(pendingRequests[nextPR].originalClientMsg, "%s", currentMsg.msg);
	  
	  pendingRequests[nextPR].clientMachineID = currentMsg.fromMachineID;
	  pendingRequests[nextPR].clientMailboxID = currentMsg.fromMailboxID;
	  pendingRequests[nextPR].timestamp = 0;
	  pendingRequests[nextPR].active = true;
	  pendingRequests[nextPR].pendingDYH = new List();
	
	  char* response = new char[MAX_SIZE];
	  
	  // Iterate through servers send them
	  for(int i = 0; i < numServers; i++) {
	    if(i != outPacketHeader.from) {
	      response = new char[MAX_SIZE];
	      sprintf(response, "dyht%di%dl%s", DESTROYLOCK-48, nextPR, name);
	      outMailHeader.length = strlen(response) + 1;
	      outPacketHeader.to = i;
	      outMailHeader.to = 0;
	      postOffice->Send(outPacketHeader, outMailHeader, response);
	      timestamp++;
	    }
	  }	
	  nextPR++;
	}
	else {
	  DestroyLock(index, currentMsg.fromMachineID, currentMsg.fromMailboxID);
	}
      }
      // This is a CreateCV Request
      else if(currentMsg.msg[1] == CREATECV && currentMsg.msg[2] == 'n') {
	char* name = new char[MAX_SIZE];
	
	// Pull the name out of the message
	for(int i = 3; i < MAX_SIZE - 3; i++) {
	  name[i-3] = currentMsg.msg[i];
	}
	
	printf("Name is %s.\n", name);
	
	int index = -1;
	
	// Check if I have this cv
	for(int i = 0; i < nextCV; i++) {
	  if(*cvs[i].condition.name ==  *name) {
	    index = i;
	  }
	}
	
	if(index == -1) {
	  // I don't have this lock...ask the other servers

	  // Initialize the new pending request
	  pendingRequests[nextPR].type = CREATECV;
	  pendingRequests[nextPR].numberOfNoReplies = 0;
	  pendingRequests[nextPR].originalClientMsg = new char[MAX_SIZE];
	  
	  sprintf(pendingRequests[nextPR].originalClientMsg, "%s", currentMsg.msg);
	  
	  pendingRequests[nextPR].clientMachineID = currentMsg.fromMachineID;
	  pendingRequests[nextPR].clientMailboxID = currentMsg.fromMailboxID;
	  pendingRequests[nextPR].timestamp = 0;
	  pendingRequests[nextPR].active = true;
	  pendingRequests[nextPR].pendingDYH = new List();
	  
	  char* response = new char[MAX_SIZE];
				
	  // Iterate through servers send them
	  for(int i = 0; i < numServers; i++) {
	    if(i != outPacketHeader.from) {
	      response = new char[MAX_SIZE];
	      sprintf(response, "dyht%di%dc%s", CREATECV-48, nextPR, name);
	      outMailHeader.length = strlen(response) + 1;
	      outPacketHeader.to = i;
	      outMailHeader.to = 0;
	      postOffice->Send(outPacketHeader, outMailHeader, response);
	      timestamp++;
	    }
	  }	
	  nextPR++;
	}
	else {
	  CreateCV(name, currentMsg.fromMachineID, currentMsg.fromMailboxID);
	}
      }
      // This is a Wait request
      else if(currentMsg.msg[1] == WAIT && currentMsg.msg[2] == 'c') {
	char* cvName = new char[MAX_SIZE];
	char* lockName = new char[MAX_SIZE];
				
	// Pull the name out of the message
	int i = 3;
	while(currentMsg.msg[i] != 'l') {
	  cvName[i-3] = currentMsg.msg[i];
	  i++;
	  cvName[i-3] = '\0';
	}
	i++;
	int j = i;
			
	while(i < MAX_SIZE - j) {
	  lockName[i-j] = currentMsg.msg[i];
	  i++;
	}
		
	printf("CV Name is %s.\n", cvName);
	printf("Lock Name is %s.\n", lockName);
	
	int cvIndex = -1;
	int lockIndex = -1;
	
	// Check if I have this CV
	for(int k = 0; k < nextCV; k++) {
	  if(*cvs[k].condition.name ==  *cvName) {
	    cvIndex = k;
	  }
	}
			
	// Check if I have this lock
	for(int k = 0; k < nextLock; k++) {
	  if(*lcks[k].lock.name == *lockName) {
	    lockIndex = k;
	  }
	}
			
	// I have both the CV and the lock
	if(cvIndex != -1 && lockIndex != -1) {
	  Wait(cvIndex, lockIndex, currentMsg.fromMachineID, currentMsg.fromMailboxID);
	}
	// I have the CV, but not the lock
	else if(cvIndex != -1 && lockIndex == -1) {
	  // I don't have the lock...ask the other servers

	  // Initialize the new pending request
	  pendingRequests[nextPR].type = WAIT;
	  pendingRequests[nextPR].numberOfNoReplies = 0;
	  pendingRequests[nextPR].originalClientMsg = new char[MAX_SIZE];
	  
	  sprintf(pendingRequests[nextPR].originalClientMsg, "%s", currentMsg.msg);
				
	  pendingRequests[nextPR].clientMachineID = currentMsg.fromMachineID;
	  pendingRequests[nextPR].clientMailboxID = currentMsg.fromMailboxID;
	  pendingRequests[nextPR].timestamp = 0;
	  pendingRequests[nextPR].active = true;
	  pendingRequests[nextPR].pendingDYH = new List();
				
	  char* response = new char[MAX_SIZE];
				
	  // Iterate through servers send them
	  for(int k = 0; k < numServers; k++) {
	    if(k != outPacketHeader.from) {
	      response = new char[MAX_SIZE];
	      sprintf(response, "dyht%di%dl%s", WAIT-48, nextPR, lockName);
	      outMailHeader.length = strlen(response) + 1;
	      outPacketHeader.to = k;
	      outMailHeader.to = 0;
	      postOffice->Send(outPacketHeader, outMailHeader, response);
	      timestamp++;
	    }
	  }
	  nextPR++;
	}
	else if(cvIndex == -1) {
	  // I don't have the cv...ask the other servers

	  // Initialize the new pending request
	  pendingRequests[nextPR].type = WAIT;
	  pendingRequests[nextPR].numberOfNoReplies = 0;
	  pendingRequests[nextPR].originalClientMsg = new char[MAX_SIZE];

	  sprintf(pendingRequests[nextPR].originalClientMsg, "%s", currentMsg.msg);

	  pendingRequests[nextPR].clientMachineID = currentMsg.fromMachineID;
	  pendingRequests[nextPR].clientMailboxID = currentMsg.fromMailboxID;
	  pendingRequests[nextPR].timestamp = 0;
	  pendingRequests[nextPR].active = true;
	  pendingRequests[nextPR].pendingDYH = new List();

	  char* response = new char[MAX_SIZE];

	  // Iterate through servers and send them
	  for(int k = 0; k < numServers; k++) {
	    if(k != outPacketHeader.from) {
	      response = new char[MAX_SIZE];
	      sprintf(response, "dyht%di%dc%s", WAIT-48, nextPR, cvName);
	      outMailHeader.length = strlen(response) + 1;
	      outPacketHeader.to = k;
	      outMailHeader.to = 0;
	      postOffice->Send(outPacketHeader, outMailHeader, response);
	      timestamp++;
	      printf("Sent DYH %s.\n", response);
	    }
	  }
	  nextPR++;
	}
      }
      // This is a Signal request
      else if(currentMsg.msg[1] == SIGNAL && currentMsg.msg[2] == 'c') {
	char* cvName = new char[MAX_SIZE];
	char* lockName = new char[MAX_SIZE];

	// Pull the name out of the message
	int i = 3;
	while(currentMsg.msg[i] != 'l') {
	  cvName[i-3] = currentMsg.msg[i];
	  i++;
	  cvName[i-3] = '\0';
	}
	i++;
	int j = i;

	while(i < MAX_SIZE - j) {
	  lockName[i-j] = currentMsg.msg[i];
	  i++;
	}

	printf("CV Name is %s.\n", cvName);
	printf("Lock name is %s.\n", lockName);

	int cvIndex = -1;
	int lockIndex = -1;

	// Check if I have this CV
	for(int k = 0; k < nextCV; k++) {
	  if(*cvs[k].condition.name == *cvName) {
	    cvIndex = k;
	  }
	}

	// Check if I have this lock
	for(int k = 0; k < nextLock; k++) {
	  if(*lcks[k].lock.name == *lockName) {
	    lockIndex = k;
	  }
	}

	// I have both the lock and the cv
	if(cvIndex != -1 && lockIndex != -1) {
	  printf("I have the CV and the Lock.\n");
	  Signal(cvIndex, lockIndex, currentMsg.fromMachineID, currentMsg.fromMailboxID);
	}
	// I have the CV but not the lock
	else if(cvIndex != -1 && lockIndex == -1) {
	  printf("I have the CV but not the Lock.\n");
	  // I don't have the lock...ask the other servers

	  // Initialize the new pending request
	  pendingRequests[nextPR].type = SIGNAL;
	  pendingRequests[nextPR].numberOfNoReplies = 0;
	  pendingRequests[nextPR].originalClientMsg = new char[MAX_SIZE];
	  
	  sprintf(pendingRequests[nextPR].originalClientMsg, "%s", currentMsg.msg);
	  
	  pendingRequests[nextPR].clientMachineID = currentMsg.fromMachineID;
	  pendingRequests[nextPR].clientMailboxID = currentMsg.fromMailboxID;
	  pendingRequests[nextPR].timestamp = 0;
	  pendingRequests[nextPR].active = true;
	  pendingRequests[nextPR].pendingDYH = new List();

	  char* response = new char[MAX_SIZE];

	  // Iterate through servers send them
	  for(int k = 0; k < numServers; k++) {
	    if(k != outPacketHeader.from) {
	      response = new char[MAX_SIZE];
	      sprintf(response, "dyht%di%dl%s", SIGNAL-48, nextPR, lockName);
	      outMailHeader.length = strlen(response) + 1;
	      outPacketHeader.to = k;
	      outMailHeader.to = 0;
	      postOffice->Send(outPacketHeader, outMailHeader, response);
	      timestamp++;
	    }
	  }
	  nextPR++;
	}
	// I do not have the CV
	else if(cvIndex == -1) {
	  printf("I do not have the CV.\n");
	  // I don't have the cv...ask the other servers

	  // Initialize the new pending request
	  pendingRequests[nextPR].type = SIGNAL;
	  pendingRequests[nextPR].numberOfNoReplies = 0;
	  pendingRequests[nextPR].originalClientMsg = new char[MAX_SIZE];
	  
	  sprintf(pendingRequests[nextPR].originalClientMsg, "%s", currentMsg.msg);
	  
	  pendingRequests[nextPR].clientMachineID = currentMsg.fromMachineID;
	  pendingRequests[nextPR].clientMailboxID = currentMsg.fromMailboxID;
	  pendingRequests[nextPR].timestamp = 0;
	  pendingRequests[nextPR].active = true;
	  pendingRequests[nextPR].pendingDYH = new List();

	  char* response = new char[MAX_SIZE];

	  // Iterate through servers send them
	  for(int k = 0; k < numServers; k++) {
	    if(k != outPacketHeader.from) {
	      response = new char[MAX_SIZE];
	      sprintf(response, "dyht%di%dc%s", SIGNAL-48, nextPR, cvName);
	      outMailHeader.length = strlen(response) + 1;
	      outPacketHeader.to = k;
	      outMailHeader.to = 0;
	      postOffice->Send(outPacketHeader, outMailHeader, response);
	      timestamp++;
	    }
	  }
	  nextPR++;
	}
      }
      // This is a Broadcast request
      else if(currentMsg.msg[1] == BROADCAST && currentMsg.msg[2] == 'c') {
	char* cvName = new char[MAX_SIZE];
	char* lockName = new char[MAX_SIZE];

	// Pull the name out of the message
	int i = 3;
	while(currentMsg.msg[i] != 'l') {
	  cvName[i-3] = currentMsg.msg[i];
	  i++;
	  cvName[i-3] = '\0';
	}
	i++;
	int j = i;

	while(i < MAX_SIZE - j) {
	  lockName[i-j] = currentMsg.msg[i];
	  i++;
	}

	printf("CV Name is %s.\n", cvName);
	printf("Lock name is %s.\n", lockName);

	int cvIndex = -1;
	int lockIndex = -1;

	// Check if I have this CV
	for(int k = 0; k < nextCV; k++) {
	  if(*cvs[k].condition.name == *cvName) {
	    cvIndex = k;
	  }
	}

	// Check if I have this lock
	for(int k = 0; k < nextLock; k++) {
	  if(*lcks[k].lock.name == *lockName) {
	    lockIndex = k;
	  }
	}

	// I have both the CV and the lock
	if(cvIndex != -1 && lockIndex != -1) {
	  Broadcast(cvIndex, lockIndex, currentMsg.fromMachineID, currentMsg.fromMailboxID);
	}
	// I have the CV but not the lock
	else if(cvIndex != -1 && lockIndex == -1) {
	  // I don't have the lock...ask the other servers

	  // Initialize the new pending request
	  pendingRequests[nextPR].type = BROADCAST;
	  pendingRequests[nextPR].numberOfNoReplies = 0;
	  pendingRequests[nextPR].originalClientMsg = new char[MAX_SIZE];
	  
	  sprintf(pendingRequests[nextPR].originalClientMsg, "%s", currentMsg.msg);
	  
	  pendingRequests[nextPR].clientMachineID = currentMsg.fromMachineID;
	  pendingRequests[nextPR].clientMailboxID = currentMsg.fromMailboxID;
	  pendingRequests[nextPR].timestamp = 0;
	  pendingRequests[nextPR].active = true;
	  pendingRequests[nextPR].pendingDYH = new List();

	  char* response = new char[MAX_SIZE];

	  // Iterate through servers send them
	  for(int k = 0; k < numServers; k++) {
	    if(k != outPacketHeader.from) {
	      response = new char[MAX_SIZE];
	      sprintf(response, "dyht%di%dl%s", BROADCAST-48, nextPR, lockName);
	      outMailHeader.length = strlen(response) + 1;
	      outPacketHeader.to = k;
	      outMailHeader.to = 0;
	      postOffice->Send(outPacketHeader, outMailHeader, response);
	      timestamp++;
	    }
	  }
	  nextPR++;
	}
	// I do not have the CV
	else if(cvIndex == -1) {
	  // I don't have the cv...ask the other servers

	  // Initialize the new pending request
	  pendingRequests[nextPR].type = BROADCAST;
	  pendingRequests[nextPR].numberOfNoReplies = 0;
	  pendingRequests[nextPR].originalClientMsg = new char[MAX_SIZE];
	  
	  sprintf(pendingRequests[nextPR].originalClientMsg, "%s", currentMsg.msg);
	  
	  pendingRequests[nextPR].clientMachineID = currentMsg.fromMachineID;
	  pendingRequests[nextPR].clientMailboxID = currentMsg.fromMailboxID;
	  pendingRequests[nextPR].timestamp = 0;
	  pendingRequests[nextPR].active = true;
	  pendingRequests[nextPR].pendingDYH = new List();

	  char* response = new char[MAX_SIZE];

	  // Iterate through servers send them
	  for(int k = 0; k < numServers; k++) {
	    if(k != outPacketHeader.from) {
	      response = new char[MAX_SIZE];
	      sprintf(response, "dyht%di%dc%s", BROADCAST-48, nextPR, cvName);
	      outMailHeader.length = strlen(response) + 1;
	      outPacketHeader.to = k;
	      outMailHeader.to = 0;
	      postOffice->Send(outPacketHeader, outMailHeader, response);
	      timestamp++;
	    }
	  }
	  nextPR++;
	}
      }
      // This is a DestroyCV request
      else if(currentMsg.msg[1] == DESTROYCV && currentMsg.msg[2] == 'n') {
	char* name = new char[MAX_SIZE];
	
	// Pull the name out of the message
	for(int i = 3; i < MAX_SIZE - 3; i++) {
	  name[i-3] = currentMsg.msg[i];
	}
	
	printf("Name is %s.\n", name);
	
	int index = -1;
	
	// Check if I have this lock
	for(int i = 0; i < nextCV; i++) {
	  if(*cvs[i].condition.name ==  *name) {
	    index = i;
	  }
	}
	
	if(index == -1) {
	  // I don't have this lock...ask the other servers
	  
	  // Initialize the new pending request
	  pendingRequests[nextPR].type = DESTROYCV;
	  pendingRequests[nextPR].numberOfNoReplies = 0;
	  pendingRequests[nextPR].originalClientMsg = new char[MAX_SIZE];
	  
	  sprintf(pendingRequests[nextPR].originalClientMsg, "%s", currentMsg.msg);
	  
	  pendingRequests[nextPR].clientMachineID = currentMsg.fromMachineID;
	  pendingRequests[nextPR].clientMailboxID = currentMsg.fromMailboxID;
	  pendingRequests[nextPR].timestamp = 0;
	  pendingRequests[nextPR].active = true;
	  pendingRequests[nextPR].pendingDYH = new List();
	  
	  char* response = new char[MAX_SIZE];
				
	  // Iterate through servers send them
	  for(int i = 0; i < numServers; i++) {
	    if(i != outPacketHeader.from) {
	      response = new char[MAX_SIZE];
	      sprintf(response, "dyht%di%dc%s", DESTROYCV-48, nextPR, name);
	      outMailHeader.length = strlen(response) + 1;
	      outPacketHeader.to = i;
	      outMailHeader.to = 0;
	      postOffice->Send(outPacketHeader, outMailHeader, response);
	      timestamp++;
	    }
	  }	
	  nextPR++;
	}
	else {
	  DestroyCV(index, currentMsg.fromMachineID, currentMsg.fromMailboxID);
	}
      }
      // This is a CreateMV request
      else if(currentMsg.msg[1] == CREATEMV && (currentMsg.msg[2] == 't' ||
						currentMsg.msg[2] == 'h' ||
						currentMsg.msg[2] == 'k')) {
	char* name = new char[MAX_SIZE];
	int arraySize = -1;
	int nextIndex;

	// Pull out the array size
	if(currentMsg.msg[2] == 't') {
	  arraySize = currentMsg.msg[3]-48;
	  nextIndex = 4;
	}
	else if(currentMsg.msg[2] == 'h') {
	  arraySize = (currentMsg.msg[3]-48)*10 + currentMsg.msg[4]-48;
	  nextIndex = 5;
	}
	else if(currentMsg.msg[3] == 'k') {
	  arraySize = (currentMsg.msg[3]-48)*100 + (currentMsg.msg[4]-48)*10 + currentMsg.msg[5]-48;
	  nextIndex = 6;
	}
	nextIndex++;
	
	// Pull the name out of the message
	for(int i = nextIndex; i < MAX_SIZE; i++) {
	  name[i-nextIndex] = currentMsg.msg[i];
	}
	
	printf("Name is %s.\n", name);
	
	int index = -1;
	
	// Check if I have this MV
	for(int i = 0; i < nextMVPos; i++) {
	  if(*mvs[i].name ==  *name) {
	    index = i;
	  }
	}
	
	if(index == -1) {
	  // I don't have this MV...ask the other servers

	  // Initialize the new pending request
	  pendingRequests[nextPR].type = CREATEMV;
	  pendingRequests[nextPR].numberOfNoReplies = 0;
	  pendingRequests[nextPR].originalClientMsg = new char[MAX_SIZE];
	  
	  sprintf(pendingRequests[nextPR].originalClientMsg, "%s", currentMsg.msg);
	  
	  pendingRequests[nextPR].clientMachineID = currentMsg.fromMachineID;
	  pendingRequests[nextPR].clientMailboxID = currentMsg.fromMailboxID;
	  pendingRequests[nextPR].timestamp = 0;
	  pendingRequests[nextPR].active = true;
	  pendingRequests[nextPR].pendingDYH = new List();
	  
	  char* response = new char[MAX_SIZE];
				
	  // Iterate through servers send them
	  for(int i = 0; i < numServers; i++) {
	    if(i != outPacketHeader.from) {
	      response = new char[MAX_SIZE];
	      sprintf(response, "dyht%di%dm%s", CREATEMV-48, nextPR, name);
	      outMailHeader.length = strlen(response) + 1;
	      outPacketHeader.to = i;
	      outMailHeader.to = 0;
	      postOffice->Send(outPacketHeader, outMailHeader, response);
	      timestamp++;
	    }
	  }	
	  nextPR++;
	}
	else {
	  CreateMV(name, currentMsg.fromMachineID, currentMsg.fromMailboxID);
	}
      }
      // This is a set request
      else if(currentMsg.msg[1] == SET1 && currentMsg.msg[2] == SET2 && currentMsg.msg[3] == 'n') {
	char* name = new char[MAX_SIZE];

	// Pull the name out of the message
	int i = 4;
	while(currentMsg.msg[i] != 'i') {
	  name[i-4] = currentMsg.msg[i];
	  i++;
	  name[i-4] = '\0';
	}
	printf("Name %s. i pos %i.\n", name, i);

	i++;

	int digits = 0;
	int j = i;
	int total = 0;
	int factor = 1;
	int arrayIndex = -1;

	while(currentMsg.msg[i] != 'a') {
	  digits++;
	  i++;
	}

	// Find the value
	while(digits > 0) {
	  total += (currentMsg.msg[j + digits - 1]-48)*factor;
	  factor = factor*10;
	  digits--;
	}
	i++;

	j = i;

	while(currentMsg.msg[i] != '\0') {
	  digits++;
	  i++;
	}

	// Find the array index to be changed
	if(digits == 1) {
	  arrayIndex = currentMsg.msg[j]-48;
	}
	else if(digits == 2) {
	  arrayIndex = (currentMsg.msg[j]-48)*10 + (currentMsg.msg[j+1]-48);
	}
	else if(digits == 3) {
	  arrayIndex = (currentMsg.msg[j]-48)*100 + (currentMsg.msg[j+1]-48)*10 + (currentMsg.msg[j+2]-48);
	}

	printf("Value: %i, Array Index: %i.\n", total, arrayIndex);

	int index = -1;

	// Check if I have this MV
	for(int k = 0; k < nextMVPos; k++) {
	  if(*mvs[k].name == *name) {
	    index = k;
	  }
	}

	if(index == -1) {
	  // I don't have this mv...ask the other servers

	  // Initialize the new pending request
	  pendingRequests[nextPR].type = SET;
	  pendingRequests[nextPR].numberOfNoReplies = 0;
	  pendingRequests[nextPR].originalClientMsg = new char[MAX_SIZE];
	  
	  sprintf(pendingRequests[nextPR].originalClientMsg, "%s", currentMsg.msg);
	  
	  pendingRequests[nextPR].clientMachineID = currentMsg.fromMachineID;
	  pendingRequests[nextPR].clientMailboxID = currentMsg.fromMailboxID;
	  pendingRequests[nextPR].timestamp = 0;
	  pendingRequests[nextPR].active = true;
	  pendingRequests[nextPR].pendingDYH = new List();
	  
	  char* response = new char[MAX_SIZE];
				
	  // Iterate through servers send them
	  for(int k = 0; k < numServers; k++) {
	    if(k != outPacketHeader.from) {
	      response = new char[MAX_SIZE];
	      sprintf(response, "dyht%di%dm%s", SET, nextPR, name);
	      outMailHeader.length = strlen(response) + 1;
	      outPacketHeader.to = k;
	      outMailHeader.to = 0;
	      postOffice->Send(outPacketHeader, outMailHeader, response);
	      timestamp++;
	      printf("Sent a DYH %s.\n", response);
	    }
	  }	
	  nextPR++;
	}
	else {
	  Set(index, total, arrayIndex, currentMsg.fromMachineID, currentMsg.fromMailboxID);
	}
      }
      // This is a Get request
      else if(currentMsg.msg[1] == GET1 && currentMsg.msg[2] == GET2 && currentMsg.msg[3] == 'i') {
	char* name = new char[MAX_SIZE];
	int i = 4;
	int digits = 0;
	int arrayIndex = -1;

	// Pull the arrayIndex out of the message
	while(currentMsg.msg[i] != 'i') {
	  digits++;
	  i++;
	}

	if(digits == 1) {
	  arrayIndex = currentMsg.msg[4] - 48;
	}
	else if(digits == 2) {
	  arrayIndex = (currentMsg.msg[4]-48)*10 + (currentMsg.msg[5]-48);
	}
	else if(digits == 3) {
	  arrayIndex = (currentMsg.msg[4]-48)*1000 + (currentMsg.msg[5]-48)*10 + (currentMsg.msg[6]-48);
	}
	i++;
	int j = i;

	// Pull the name out of the message
	for(i; i < MAX_SIZE; i++) {
	  name[i-j] = currentMsg.msg[i];
	}

	printf("Name is %s.\n", name);

	int index = -1;

	// Check if I have this MV
	for(int k = 0; k < nextMVPos; k++) {
	  if(*mvs[k].name == *name) {
	    index = i;
	  }
	}

	if(index == -1) {
	  // I don't have this MV...ask the other servers

	  // Initialize the new pending request
	  pendingRequests[nextPR].type = GET;
	  pendingRequests[nextPR].numberOfNoReplies = 0;
	  pendingRequests[nextPR].originalClientMsg = new char[MAX_SIZE];
	  
	  sprintf(pendingRequests[nextPR].originalClientMsg, "%s", currentMsg.msg);
	  
	  pendingRequests[nextPR].clientMachineID = currentMsg.fromMachineID;
	  pendingRequests[nextPR].clientMailboxID = currentMsg.fromMailboxID;
	  pendingRequests[nextPR].timestamp = 0;
	  pendingRequests[nextPR].active = true;
	  pendingRequests[nextPR].pendingDYH = new List();
	  
	  char* response = new char[MAX_SIZE];
				
	  // Iterate through servers send them
	  for(int k = 0; k < numServers; k++) {
	    if(k != outPacketHeader.from) {
	      response = new char[MAX_SIZE];
	      sprintf(response, "dyht%di%dm%s", GET, nextPR, name);
	      outMailHeader.length = strlen(response) + 1;
	      outPacketHeader.to = k;
	      outMailHeader.to = 0;
	      postOffice->Send(outPacketHeader, outMailHeader, response);
	      timestamp++;
	      printf("Sent DYH %s.\n", response);
	    }
	  }	
	  nextPR++;
	}
	else {
	  Get(index, arrayIndex, currentMsg.fromMachineID, currentMsg.fromMailboxID);
	}
      }
      // This is a DestroyMV request
      else if(currentMsg.msg[1] == DESTROYMV1 && currentMsg.msg[2] == DESTROYMV2 && currentMsg.msg[3] == 'n') {
	char* name = new char[MAX_SIZE];
	
	// Pull the name out of the message
	for(int i = 4; i < MAX_SIZE - 3; i++) {
	  name[i-4] = currentMsg.msg[i];
	}
	
	printf("Name is %s.\n", name);
	
	int index = -1;
	
	// Check if I have this lock
	for(int i = 0; i < nextMVPos; i++) {
	  if(*mvs[i].name ==  *name) {
	    index = i;
	  }
	}
	
	if(index == -1) {
	  // I don't have this MV...ask the other servers

	  // Initialize the new pending request
	  pendingRequests[nextPR].type = DESTROYMV;
	  pendingRequests[nextPR].numberOfNoReplies = 0;
	  pendingRequests[nextPR].originalClientMsg = new char[MAX_SIZE];
	  
	  sprintf(pendingRequests[nextPR].originalClientMsg, "%s", currentMsg.msg);
	  
	  pendingRequests[nextPR].clientMachineID = currentMsg.fromMachineID;
	  pendingRequests[nextPR].clientMailboxID = currentMsg.fromMailboxID;
	  pendingRequests[nextPR].timestamp = 0;
	  pendingRequests[nextPR].active = true;
	  pendingRequests[nextPR].pendingDYH = new List();
	  
	  char* response = new char[MAX_SIZE];
				
	  // Iterate through servers send them
	  for(int i = 0; i < numServers; i++) {
	    if(i != outPacketHeader.from) {
	      response = new char[MAX_SIZE];
	      sprintf(response, "dyht%di%dm%s", DESTROYMV, nextPR, name);
	      outMailHeader.length = strlen(response) + 1;
	      outPacketHeader.to = i;
	      outMailHeader.to = 0;
	      postOffice->Send(outPacketHeader, outMailHeader, response);
	      timestamp++;
	    }
	  }	
	  nextPR++;
	}
	else {
	  DestroyMV(index, currentMsg.fromMachineID, currentMsg.fromMailboxID);
	}
      }
    }
    // This is a DYH request
    else if(currentMsg.msg[0] == 'd' && currentMsg.msg[1] == 'y' &&
	    currentMsg.msg[2] == 'h') {
      printf("Received a DYH message %s.\n",currentMsg.msg);

      // This is a DYH CreateLock request
      if(currentMsg.msg[3] == 't' && currentMsg.msg[4] == CREATELOCK &&
	 currentMsg.msg[5] == 'i') {
	printf("Received a DYH for CreateLock.\n");
	int nextPos = 6;
	int digits = 0;
	int pos = 0;
	int index = -1;

	// Pull out the PR index
	while(currentMsg.msg[nextPos] != 'l') {
	  nextPos++;
	}

	digits = nextPos - 6;

	if(digits == 1) {
	  index = currentMsg.msg[6] - 48;
	}
	else if(digits == 2) {
	  index = (currentMsg.msg[7]-48)*10 + currentMsg.msg[6]-48;
	}
	else if(digits == 3) {
	  index = (currentMsg.msg[8]-48)*100 + (currentMsg.msg[7]-48)*10 + currentMsg.msg[6]-48;
	}
	nextPos++;

	if(currentMsg.msg[nextPos-1] == 'l') {
	  char* name = new char[MAX_SIZE];
	  char* response = new char[MAX_SIZE];

	  // Pull the name out of the message
	  for(int i = nextPos; i < MAX_SIZE; i++) {
	    name[i - nextPos] = currentMsg.msg[i];
	  }

	  printf("Name %s.\n", name);

	  // Check if I have this lock
	  int index2 = -1;
	  for(int i = 0; i < nextLock; i++) {
	    if(*lcks[i].lock.name == *name) {
	      index2 = i;
	    }
	  }
	  outPacketHeader.to = currentMsg.fromMachineID;
	  outMailHeader.to = currentMsg.fromMailboxID;
	  printf("Sending to %i %i.\n", outPacketHeader.to, outMailHeader.to);
	  if(index2 == -1) {
	    sprintf(response, "rnt%di%dl%s", CREATELOCK-48, index, name);
	    outMailHeader.length = strlen(response) + 1;
	    printf("Sending a message %s.\n", response);
	    postOffice->Send(outPacketHeader, outMailHeader, response);
	  }
	  else {
	    sprintf(response, "ryt%di%dl%s", CREATELOCK-48, index, name);
	    outMailHeader.length = strlen(response) + 1;
	    printf("Sending a message %s.\n", response);
	    postOffice->Send(outPacketHeader, outMailHeader, response);
	  }
	}
      }
      // This is an Acquire DYH request
      else if(currentMsg.msg[3] == 't' && currentMsg.msg[4] == ACQUIRE && currentMsg.msg[5] == 'i') {
	printf("Received a DYH for Acquire.\n");
	int nextPos = 6;
	int digits = 0;
	int pos = 0;
	int index = -1;
	
	// Pull out the PR position
	while(currentMsg.msg[nextPos] != 'l') {
	  nextPos++;
	}

	digits = nextPos - 6;

	if(digits == 1) {
	  index = currentMsg.msg[6] - 48;
	}
	else if(digits == 2) {
	  index = (currentMsg.msg[6]-48)*10 + currentMsg.msg[7]-48;
	}
	else if(digits == 3) {
	  index = (currentMsg.msg[6]-48)*100 + (currentMsg.msg[7]-48)*10 + currentMsg.msg[8]-48;
	}
	nextPos++;

	if(currentMsg.msg[nextPos-1] == 'l') {
	  char* name = new char[MAX_SIZE];
	  char* response = new char[MAX_SIZE];

	  // Pull the name out of the message
	  for(int i = nextPos; i < MAX_SIZE; i++) {
	    name[i - nextPos] = currentMsg.msg[i];
	  }

	  printf("Name %s.\n", name);

	  // Check if I have this lock
	  int index2 = -1;
	  for(int i = 0; i < nextLock; i++) {
	    if(*lcks[i].lock.name == *name) {
	      index2 = i;
	    }
	  }
	  outPacketHeader.to = currentMsg.fromMachineID;
	  outMailHeader.to = currentMsg.fromMailboxID;
	  printf("Sending to %i %i.\n", outPacketHeader.to, outMailHeader.to);
	  // I do not have the lock
	  if(index2 == -1) {
	    sprintf(response, "rnt%di%dl%s", ACQUIRE-48, index, name);
	    outMailHeader.length = strlen(response) + 1;
	    printf("Sending a message %s.\n", response);
	    postOffice->Send(outPacketHeader, outMailHeader, response);
	  }
	  // I do have the lock
	  else {
	    sprintf(response, "ryt%di%dl%s", ACQUIRE-48, index, name);
	    outMailHeader.length = strlen(response) + 1;
	    printf("Sending a message %s.\n", response);
	    postOffice->Send(outPacketHeader, outMailHeader, response);
	  }
	}
      }
      // This is a Release DYH request
      else if(currentMsg.msg[3] == 't' && currentMsg.msg[4] == RELEASE && currentMsg.msg[5] == 'i') {
	printf("Received a DYH for Release.\n");
	int nextPos = 6;
	int digits = 0;
	int pos = 0;
	int index = -1;

	// Pull out the PR index
	while(currentMsg.msg[nextPos] != 'l') {
	  nextPos++;
	}

	digits = nextPos - 6;

	if(digits == 1) {
	  index = currentMsg.msg[6] - 48;
	}
	else if(digits == 2) {
	  index = (currentMsg.msg[6]-48)*10 + currentMsg.msg[7]-48;
	}
	else if(digits == 3) {
	  index = (currentMsg.msg[6]-48)*100 + (currentMsg.msg[7]-48)*10 + currentMsg.msg[8]-48;
	}
	nextPos++;

	if(currentMsg.msg[nextPos-1] == 'l') {
	  char* name = new char[MAX_SIZE];
	  char* response = new char[MAX_SIZE];

	  // Pull the name out of the message
	  for(int i = nextPos; i < MAX_SIZE; i++) {
	    name[i - nextPos] = currentMsg.msg[i];
	  }

	  printf("Name %s.\n", name);

	  // Check if I have this lock
	  int index2 = -1;
	  for(int i = 0; i < nextLock; i++) {
	    if(*lcks[i].lock.name == *name) {
	      index2 = i;
	    }
	  }
	  outPacketHeader.to = currentMsg.fromMachineID;
	  outMailHeader.to = currentMsg.fromMailboxID;
	  printf("Sending to %i %i.\n", outPacketHeader.to, outMailHeader.to);
	  // I do not have the lock
	  if(index2 == -1) {
	    sprintf(response, "rnt%di%dl%s", RELEASE-48, index, name);
	    outMailHeader.length = strlen(response) + 1;
	    printf("Sending a message %s.\n", response);
	    postOffice->Send(outPacketHeader, outMailHeader, response);
	  }
	  // I do have the lock
	  else {
	    sprintf(response, "ryt%di%dl%s", RELEASE-48, index, name);
	    outMailHeader.length = strlen(response) + 1;
	    printf("Sending a message %s.\n", response);
	    postOffice->Send(outPacketHeader, outMailHeader, response);
	  }
	}
      }
      // This is a DestroyLock DYH request
      else if(currentMsg.msg[3] == 't' && currentMsg.msg[4] == DESTROYLOCK && currentMsg.msg[5] == 'i') {
	printf("Received a DYH for DestroyLock.\n");
	int nextPos = 6;
	int digits = 0;
	int pos = 0;
	int index = -1;

	// Pull out the PR index
	while(currentMsg.msg[nextPos] != 'l') {
	  nextPos++;
	}

	digits = nextPos - 6;

	if(digits == 1) {
	  index = currentMsg.msg[6] - 48;
	}
	else if(digits == 2) {
	  index = (currentMsg.msg[7]-48)*10 + currentMsg.msg[6]-48;
	}
	else if(digits == 3) {
	  index = (currentMsg.msg[8]-48)*100 + (currentMsg.msg[7]-48)*10 + currentMsg.msg[6]-48;
	}
	nextPos++;

	if(currentMsg.msg[nextPos-1] == 'l') {
	  char* name = new char[MAX_SIZE];
	  char* response = new char[MAX_SIZE];

	  // Pull the name out of the message
	  for(int i = nextPos; i < MAX_SIZE; i++) {
	    name[i - nextPos] = currentMsg.msg[i];
	  }

	  printf("Name %s.\n", name);

	  // Check if I have this lock
	  int index2 = -1;
	  for(int i = 0; i < nextLock; i++) {
	    if(*lcks[i].lock.name == *name) {
	      index2 = i;
	    }
	  }
	  outPacketHeader.to = currentMsg.fromMachineID;
	  outMailHeader.to = currentMsg.fromMailboxID;
	  printf("Sending to %i %i.\n", outPacketHeader.to, outMailHeader.to);
	  // I do not have the lock
	  if(index2 == -1) {
	    sprintf(response, "rnt%di%dl%s", DESTROYLOCK-48, index, name);
	    outMailHeader.length = strlen(response) + 1;
	    printf("Sending a message %s.\n", response);
	    postOffice->Send(outPacketHeader, outMailHeader, response);
	  }
	  // I do have the lock
	  else {
	    sprintf(response, "ryt%di%dl%s", DESTROYLOCK-48, index, name);
	    outMailHeader.length = strlen(response) + 1;
	    printf("Sending a message %s.\n", response);
	    postOffice->Send(outPacketHeader, outMailHeader, response);
	  }
	}
      }
      // This is a CreateCV DYH request
      else if(currentMsg.msg[3] == 't' && currentMsg.msg[4] == CREATECV && currentMsg.msg[5] == 'i') {
	printf("Received a DYH for CreateCV.\n");
	int nextPos = 6;
	int digits = 0;
	int pos = 0;
	int index = -1;

	// Pull out the PR index
	while(currentMsg.msg[nextPos] != 'c') {
	  nextPos++;
	  digits++;
	}

	if(digits == 1) {
	  index = currentMsg.msg[6] - 48;
	}
	else if(digits == 2) {
	  index = (currentMsg.msg[6]-48)*10 + currentMsg.msg[7]-48;
	}
	else if(digits == 3) {
	  index = (currentMsg.msg[6]-48)*100 + (currentMsg.msg[7]-48)*10 + currentMsg.msg[8]-48;
	}
	nextPos++;

	if(currentMsg.msg[nextPos-1] == 'c') {
	  char* name = new char[MAX_SIZE];
	  char* response = new char[MAX_SIZE];

	  // Pull the name out of the message
	  for(int i = nextPos; i < MAX_SIZE; i++) {
	    name[i - nextPos] = currentMsg.msg[i];
	  }

	  printf("Name %s.\n", name);

	  // Check if I have this CV
	  int index2 = -1;
	  for(int i = 0; i < nextCV; i++) {
	    if(*cvs[i].condition.name == *name) {
	      index2 = i;
	    }
	  }
	  outPacketHeader.to = currentMsg.fromMachineID;
	  outMailHeader.to = currentMsg.fromMailboxID;
	  printf("Sending to %i %i.\n", outPacketHeader.to, outMailHeader.to);
	  // I do not have this CV
	  if(index2 == -1) {
	    sprintf(response, "rnt%di%dc%s", CREATECV-48, index, name);
	    outMailHeader.length = strlen(response) + 1;
	    printf("Sending a message %s.\n", response);
	    postOffice->Send(outPacketHeader, outMailHeader, response);
	  }
	  // I do have this CV
	  else {
	    sprintf(response, "ryt%di%dc%s", CREATECV-48, index, name);
	    outMailHeader.length = strlen(response) + 1;
	    printf("Sending a message %s.\n", response);
	    postOffice->Send(outPacketHeader, outMailHeader, response);
	  }
	}
      }
      // This is a Wait DYH request
      else if(currentMsg.msg[3] == 't' && currentMsg.msg[4] == WAIT && currentMsg.msg[5] == 'i') {
	printf("Received a DYH for Wait.\n");
	int nextPos = 6;
	int digits = 0;
	int pos = 0;
	int index = -1;

	// Pull out the PR index
	while(currentMsg.msg[nextPos] != 'c' && currentMsg.msg[nextPos] != 'l') {
	  nextPos++;
	  digits++;
	}

	if(digits == 1) {
	  index = currentMsg.msg[6] - 48;
	}
	else if(digits == 2) {
	  index = (currentMsg.msg[6]-48)*10 + currentMsg.msg[7]-48;
	}
	else if(digits == 3) {
	  index = (currentMsg.msg[6]-48)*100 + (currentMsg.msg[7]-48)*10 + currentMsg.msg[8]-48;
	}
	nextPos++;

	// Figure out if this is a CV DYH or lock DYH
	int type = -1;
	if(currentMsg.msg[nextPos-1] == 'c') {
	  type = 0;
	}
	else if(currentMsg.msg[nextPos-1] == 'l') {
	  type = 1;
	}

	if(currentMsg.msg[nextPos-1] == 'c' || currentMsg.msg[nextPos-1] == 'l') {
	  char* name = new char[MAX_SIZE];
	  char* response = new char[MAX_SIZE];

	  // Pull the name out of the message
	  for(int i = nextPos; i < MAX_SIZE; i++) {
	    name[i - nextPos] = currentMsg.msg[i];
	  }

	  printf("Name %s.\n", name);

	  int index2 = -1;
	  if(type == 0) {
	    // Check if I have this CV
	    for(int i = 0; i < nextCV; i++) {
	      if(*cvs[i].condition.name == *name) {
		index2 = i;
	      }
	    }
	  }
	  else if(type == 1) {
	    // Check if I have this lock
	    for(int i = 0; i < nextLock; i++) {
	      if(*lcks[i].lock.name == *name) {
		index2 = i;
	      }
	    }
	  }

	  outPacketHeader.to = currentMsg.fromMachineID;
	  outMailHeader.to = currentMsg.fromMailboxID;
	  printf("Sending to %i %i.\n", outPacketHeader.to, outMailHeader.to);
	  // I do not have this lock/CV
	  if(index2 == -1) {
	    if(type == 0) {
	      sprintf(response, "rnt%di%dc%s", WAIT-48, index, name);
	    }
	    else if(type == 1) {
	      sprintf(response, "rnt%di%dl%s", WAIT-48, index, name);
	    }
	    outMailHeader.length = strlen(response) + 1;
	    printf("Sending a message %s.\n", response);
	    postOffice->Send(outPacketHeader, outMailHeader, response);
	  }
	  // I do have this lock/CV
	  else {
	    if(type == 0) {
	      sprintf(response, "ryt%di%dc%s", WAIT-48, index, name);
	    }
	    else if(type == 1) {
	      sprintf(response, "ryt%di%dl%s", WAIT-48, index, name);
	    }
	    outMailHeader.length = strlen(response) + 1;
	    printf("Sending a message %s.\n", response);
	    postOffice->Send(outPacketHeader, outMailHeader, response);
	  }
	}
      }
      // This is a Wait DYH request
      else if(currentMsg.msg[3] == 't' && currentMsg.msg[4] == SIGNAL && currentMsg.msg[5] == 'i') {
	printf("Received a DYH for Signal.\n");
	int nextPos = 6;
	int digits = 0;
	int pos = 0;
	int index = -1;

	// Pull out the PR index
	while(currentMsg.msg[nextPos] != 'c' && currentMsg.msg[nextPos] != 'l') {
	  nextPos++;
	  digits++;
	}

	if(digits == 1) {
	  index = currentMsg.msg[6] - 48;
	}
	else if(digits == 2) {
	  index = (currentMsg.msg[6]-48)*10 + currentMsg.msg[7]-48;
	}
	else if(digits == 3) {
	  index = (currentMsg.msg[6]-48)*100 + (currentMsg.msg[7]-48)*10 + currentMsg.msg[8]-48;
	}
	nextPos++;

	// Figure out if this is a CV DYH or lock DYH
	int type = -1;
	if(currentMsg.msg[nextPos-1] == 'c') {
	  type = 0;
	}
	else if(currentMsg.msg[nextPos-1] == 'l') {
	  type = 1;
	}

	if(currentMsg.msg[nextPos-1] == 'c' || currentMsg.msg[nextPos-1] == 'l') {
	  char* name = new char[MAX_SIZE];
	  char* response = new char[MAX_SIZE];

	  // Pull the name out of the message
	  for(int i = nextPos; i < MAX_SIZE; i++) {
	    name[i - nextPos] = currentMsg.msg[i];
	  }

	  printf("Name %s.\n", name);

	  int index2 = -1;
	  if(type == 0) {
	    // Check if I have this CV
	    for(int i = 0; i < nextCV; i++) {
	      if(*cvs[i].condition.name == *name) {
		index2 = i;
	      }
	    }
	  }
	  else if(type == 1) {
	    // Check if I have this lock
	    for(int i = 0; i < nextLock; i++) {
	      if(*lcks[i].lock.name == *name) {
		index2 = i;
	      }
	    }
	  }

	  outPacketHeader.to = currentMsg.fromMachineID;
	  outMailHeader.to = currentMsg.fromMailboxID;
	  printf("Sending to %i %i.\n", outPacketHeader.to, outMailHeader.to);
	  // I do not have this lock/CV
	  if(index2 == -1) {
	    if(type == 0) {
	      sprintf(response, "rnt%di%dc%s", SIGNAL-48, index, name);
	    }
	    else if(type == 1) {
	      sprintf(response, "rnt%di%dl%s", SIGNAL-48, index, name);
	    }
	    outMailHeader.length = strlen(response) + 1;
	    printf("Sending a message %s.\n", response);
	    postOffice->Send(outPacketHeader, outMailHeader, response);
	  }
	  // I do have this lock/CV
	  else {
	    if(type == 0) {
	      sprintf(response, "ryt%di%dc%s", SIGNAL-48, index, name);
	    }
	    else if(type == 1) {
	      sprintf(response, "ryt%di%dl%s", SIGNAL-48, index, name);
	    }
	    outMailHeader.length = strlen(response) + 1;
	    printf("Sending a message %s.\n", response);
	    postOffice->Send(outPacketHeader, outMailHeader, response);
	  }
	}
      }
      // This is a Broadcast DYH request
      else if(currentMsg.msg[3] == 't' && currentMsg.msg[4] == BROADCAST && currentMsg.msg[5] == 'i') {
	printf("Received a DYH for Wait.\n");
	int nextPos = 6;
	int digits = 0;
	int pos = 0;
	int index = -1;

	// Pull out the PR index
	while(currentMsg.msg[nextPos] != 'c' && currentMsg.msg[nextPos] != 'l') {
	  nextPos++;
	  digits++;
	}

	if(digits == 1) {
	  index = currentMsg.msg[6] - 48;
	}
	else if(digits == 2) {
	  index = (currentMsg.msg[6]-48)*10 + currentMsg.msg[7]-48;
	}
	else if(digits == 3) {
	  index = (currentMsg.msg[6]-48)*100 + (currentMsg.msg[7]-48)*10 + currentMsg.msg[8]-48;
	}
	nextPos++;

	// Figure out if this is a CV DYH or lock DYH
	int type = -1;
	if(currentMsg.msg[nextPos-1] == 'c') {
	  type = 0;
	}
	else if(currentMsg.msg[nextPos-1] == 'l') {
	  type = 1;
	}

	if(currentMsg.msg[nextPos-1] == 'c' || currentMsg.msg[nextPos-1] == 'l') {
	  char* name = new char[MAX_SIZE];
	  char* response = new char[MAX_SIZE];

	  // Pull the name out of the message
	  for(int i = nextPos; i < MAX_SIZE; i++) {
	    name[i - nextPos] = currentMsg.msg[i];
	  }

	  printf("Name %s.\n", name);

	  int index2 = -1;
	  if(type == 0) {
	    // Check if I have this CV
	    for(int i = 0; i < nextCV; i++) {
	      if(*cvs[i].condition.name == *name) {
		index2 = i;
	      }
	    }
	  }
	  else if(type == 1) {
	    // Check if I have this lock
	    for(int i = 0; i < nextLock; i++) {
	      if(*lcks[i].lock.name == *name) {
		index2 = i;
	      }
	    }
	  }

	  outPacketHeader.to = currentMsg.fromMachineID;
	  outMailHeader.to = currentMsg.fromMailboxID;
	  printf("Sending to %i %i.\n", outPacketHeader.to, outMailHeader.to);
	  // I do not have this lock/CV
	  if(index2 == -1) {
	    if(type == 0) {
	      sprintf(response, "rnt%di%dc%s", BROADCAST-48, index, name);
	    }
	    else if(type == 1) {
	      sprintf(response, "rnt%di%dl%s", BROADCAST-48, index, name);
	    }
	    outMailHeader.length = strlen(response) + 1;
	    printf("Sending a message %s.\n", response);
	    postOffice->Send(outPacketHeader, outMailHeader, response);
	  }
	  // I do have this lock/CV
	  else {
	    if(type == 0) {
	      sprintf(response, "ryt%di%dc%s", BROADCAST-48, index, name);
	    }
	    else if(type == 1) {
	      sprintf(response, "ryt%di%dl%s", BROADCAST-48, index, name);
	    }
	    outMailHeader.length = strlen(response) + 1;
	    printf("Sending a message %s.\n", response);
	    postOffice->Send(outPacketHeader, outMailHeader, response);
	  }
	}
      }
      // This is a DestroyCV DYH request
      else if(currentMsg.msg[3] == 't' && currentMsg.msg[4] == DESTROYCV && currentMsg.msg[5] == 'i') {
	printf("Received a DYH for DestroyCV.\n");
	int nextPos = 6;
	int digits = 0;
	int pos = 0;
	int index = -1;

	// Pull out the PR index
	while(currentMsg.msg[nextPos] != 'c') {
	  nextPos++;
	  digits++;
	}

	if(digits == 1) {
	  index = currentMsg.msg[6] - 48;
	}
	else if(digits == 2) {
	  index = (currentMsg.msg[6]-48)*10 + currentMsg.msg[7]-48;
	}
	else if(digits == 3) {
	  index = (currentMsg.msg[6]-48)*100 + (currentMsg.msg[7]-48)*10 + currentMsg.msg[8]-48;
	}
	nextPos++;

	if(currentMsg.msg[nextPos-1] == 'c') {
	  char* name = new char[MAX_SIZE];
	  char* response = new char[MAX_SIZE];

	  // Pull the name out of the message
	  for(int i = nextPos; i < MAX_SIZE; i++) {
	    name[i - nextPos] = currentMsg.msg[i];
	  }

	  printf("Name %s.\n", name);

	  // Check if I have this CV
	  int index2 = -1;
	  for(int i = 0; i < nextCV; i++) {
	    if(*cvs[i].condition.name == *name) {
	      index2 = i;
	    }
	  }
	  outPacketHeader.to = currentMsg.fromMachineID;
	  outMailHeader.to = currentMsg.fromMailboxID;
	  printf("Sending to %i %i.\n", outPacketHeader.to, outMailHeader.to);
	  // I do not have the CV
	  if(index2 == -1) {
	    sprintf(response, "rnt%di%dc%s", DESTROYCV-48, index, name);
	    outMailHeader.length = strlen(response) + 1;
	    printf("Sending a message %s.\n", response);
	    postOffice->Send(outPacketHeader, outMailHeader, response);
	  }
	  // I do have the CV
	  else {
	    sprintf(response, "ryt%di%dc%s", DESTROYCV-48, index, name);
	    outMailHeader.length = strlen(response) + 1;
	    printf("Sending a message %s.\n", response);
	    postOffice->Send(outPacketHeader, outMailHeader, response);
	  }
	}
      }
      // This is a CreateMV DYH request
      else if(currentMsg.msg[3] == 't' && currentMsg.msg[4] == CREATEMV && currentMsg.msg[5] == 'i') {
	printf("Received a DYH for CreateMV.\n");
	int nextPos = 6;
	int digits = 0;
	int pos = 0;
	int index = -1;

	// Pull out the PR index
	while(currentMsg.msg[nextPos] != 'm') {
	  nextPos++;
	  digits++;
	}

	if(digits == 1) {
	  index = currentMsg.msg[6] - 48;
	}
	else if(digits == 2) {
	  index = (currentMsg.msg[6]-48)*10 + currentMsg.msg[7]-48;
	}
	else if(digits == 3) {
	  index = (currentMsg.msg[6]-48)*100 + (currentMsg.msg[7]-48)*10 + currentMsg.msg[8]-48;
	}
	nextPos++;

	if(currentMsg.msg[nextPos-1] == 'm') {
	  char* name = new char[MAX_SIZE];
	  char* response = new char[MAX_SIZE];

	  // Pull the name out of the message
	  for(int i = nextPos; i < MAX_SIZE; i++) {
	    name[i - nextPos] = currentMsg.msg[i];
	  }

	  printf("Name %s.\n", name);

	  // Check if I have this lock
	  int index2 = -1;
	  for(int i = 0; i < nextMVPos; i++) {
	    if(*mvs[i].name == *name) {
	      index2 = i;
	    }
	  }
	  outPacketHeader.to = currentMsg.fromMachineID;
	  outMailHeader.to = currentMsg.fromMailboxID;
	  printf("Sending to %i %i.\n", outPacketHeader.to, outMailHeader.to);
	  // I do not have the MV
	  if(index2 == -1) {
	    sprintf(response, "rnt%di%dm%s", CREATEMV-48, index, name);
	    outMailHeader.length = strlen(response) + 1;
	    printf("Sending a message %s.\n", response);
	    postOffice->Send(outPacketHeader, outMailHeader, response);
	  }
	  // I do not have the MV
	  else {
	    sprintf(response, "ryt%di%dm%s", CREATEMV-48, index, name);
	    outMailHeader.length = strlen(response) + 1;
	    printf("Sending a message %s.\n", response);
	    postOffice->Send(outPacketHeader, outMailHeader, response);
	  }
	}
      }
      // This is a DestroyMV DYH request
      else if(currentMsg.msg[3] == 't' && currentMsg.msg[4] == DESTROYMV1 && currentMsg.msg[5] == DESTROYMV2 && currentMsg.msg[6] == 'i') {
	printf("Received a DYH for DestroyMV.\n");
	int nextPos = 7;
	int digits = 0;
	int pos = 0;
	int index = -1;

	// Pull out the PR index
	while(currentMsg.msg[nextPos] != 'm') {
	  nextPos++;
	  digits++;
	}

	if(digits == 1) {
	  index = currentMsg.msg[7] - 48;
	}
	else if(digits == 2) {
	  index = (currentMsg.msg[7]-48)*10 + currentMsg.msg[8]-48;
	}
	else if(digits == 3) {
	  index = (currentMsg.msg[7]-48)*100 + (currentMsg.msg[8]-48)*10 + currentMsg.msg[9]-48;
	}
	nextPos++;

	if(currentMsg.msg[nextPos-1] == 'm') {
	  char* name = new char[MAX_SIZE];
	  char* response = new char[MAX_SIZE];

	  // Pull the name out of the message
	  for(int i = nextPos; i < MAX_SIZE; i++) {
	    name[i - nextPos] = currentMsg.msg[i];
	  }

	  printf("Name %s.\n", name);

	  // Check if I have this lock
	  int index2 = -1;
	  for(int i = 0; i < nextMVPos; i++) {
	    if(*mvs[i].name == *name) {
	      index2 = i;
	    }
	  }
	  outPacketHeader.to = currentMsg.fromMachineID;
	  outMailHeader.to = currentMsg.fromMailboxID;
	  printf("Sending to %i %i.\n", outPacketHeader.to, outMailHeader.to);
	  // I do not have this MV
	  if(index2 == -1) {
	    sprintf(response, "rnt%di%dm%s", DESTROYMV, index, name);
	    outMailHeader.length = strlen(response) + 1;
	    printf("Sending a message %s.\n", response);
	    postOffice->Send(outPacketHeader, outMailHeader, response);
	  }
	  // I do have the MV
	  else {
	    sprintf(response, "ryt%di%dm%s", DESTROYMV, index, name);
	    outMailHeader.length = strlen(response) + 1;
	    printf("Sending a message %s.\n", response);
	    postOffice->Send(outPacketHeader, outMailHeader, response);
	  }
	}
      }
      // This is a Set DYH request
      else if(currentMsg.msg[3] == 't' && currentMsg.msg[4] == SET1 && currentMsg.msg[5] == SET2 && currentMsg.msg[6] == 'i') {
	printf("Received a DYH for SET.\n");
	int nextPos = 7;
	int digits = 0;
	int pos = 0;
	int index = -1;

	// Pull out the PR index
	while(currentMsg.msg[nextPos] != 'm') {
	  nextPos++;
	  digits++;
	}

	if(digits == 1) {
	  index = currentMsg.msg[7] - 48;
	}
	else if(digits == 2) {
	  index = (currentMsg.msg[7]-48)*10 + currentMsg.msg[8]-48;
	}
	else if(digits == 3) {
	  index = (currentMsg.msg[7]-48)*100 + (currentMsg.msg[8]-48)*10 + currentMsg.msg[9]-48;
	}
	nextPos++;

	if(currentMsg.msg[nextPos-1] == 'm') {
	  char* name = new char[MAX_SIZE];
	  char* response = new char[MAX_SIZE];

	  // Pull the name out of the message
	  for(int i = nextPos; i < MAX_SIZE; i++) {
	    name[i - nextPos] = currentMsg.msg[i];
	  }

	  printf("Name %s.\n", name);

	  // Check if I have this mv
	  int index2 = -1;
	  for(int i = 0; i < nextMVPos; i++) {
	    if(*mvs[i].name == *name) {
	      index2 = i;
	    }
	  }
	  outPacketHeader.to = currentMsg.fromMachineID;
	  outMailHeader.to = currentMsg.fromMailboxID;
	  printf("Sending to %i %i.\n", outPacketHeader.to, outMailHeader.to);
	  // I do not have the MV
	  if(index2 == -1) {
	    sprintf(response, "rnt%di%dm%s", SET, index, name);
	    outMailHeader.length = strlen(response) + 1;
	    printf("Sending a message %s.\n", response);
	    postOffice->Send(outPacketHeader, outMailHeader, response);
	  }
	  // I do have the MV
	  else {
	    sprintf(response, "ryt%di%dm%s", SET, index, name);
	    outMailHeader.length = strlen(response) + 1;
	    printf("Sending a message %s.\n", response);
	    postOffice->Send(outPacketHeader, outMailHeader, response);
	  }
	}
      }
      // This is a Get DYH request
      else if(currentMsg.msg[3] == 't' && currentMsg.msg[4] == GET1 && currentMsg.msg[5] == GET2 && currentMsg.msg[6] == 'i') {
	printf("Received a DYH for DestroyMV.\n");
	int nextPos = 7;
	int digits = 0;
	int pos = 0;
	int index = -1;

	// Pull out the PR index
	while(currentMsg.msg[nextPos] != 'm') {
	  nextPos++;
	  digits++;
	}

	if(digits == 1) {
	  index = currentMsg.msg[7] - 48;
	}
	else if(digits == 2) {
	  index = (currentMsg.msg[7]-48)*10 + currentMsg.msg[8]-48;
	}
	else if(digits == 3) {
	  index = (currentMsg.msg[7]-48)*100 + (currentMsg.msg[8]-48)*10 + currentMsg.msg[9]-48;
	}
	nextPos++;

	if(currentMsg.msg[nextPos-1] == 'm') {
	  char* name = new char[MAX_SIZE];
	  char* response = new char[MAX_SIZE];

	  // Pull the name out of the message
	  for(int i = nextPos; i < MAX_SIZE; i++) {
	    name[i - nextPos] = currentMsg.msg[i];
	  }

	  printf("Name %s.\n", name);

	  // Check if I have this mv
	  int index2 = -1;
	  for(int i = 0; i < nextMVPos; i++) {
	    if(*mvs[i].name == *name) {
	      index2 = i;
	    }
	  }
	  outPacketHeader.to = currentMsg.fromMachineID;
	  outMailHeader.to = currentMsg.fromMailboxID;
	  printf("Sending to %i %i.\n", outPacketHeader.to, outMailHeader.to);
	  // I do not have the MV
	  if(index2 == -1) {
	    sprintf(response, "rnt%di%dm%s", GET, index, name);
	    outMailHeader.length = strlen(response) + 1;
	    printf("Sending a message %s.\n", response);
	    postOffice->Send(outPacketHeader, outMailHeader, response);
	  }
	  // I do have the MV
	  else {
	    sprintf(response, "ryt%di%dm%s", GET, index, name);
	    outMailHeader.length = strlen(response) + 1;
	    printf("Sending a message %s.\n", response);
	    postOffice->Send(outPacketHeader, outMailHeader, response);
	  }
	}
      }
    }
    // This is a reply message
    else if(currentMsg.msg[0] == 'r') {
      printf("Received a reply message.\n");

      // This is a CreateLock reply message
      if(currentMsg.msg[2] == 't' && currentMsg.msg[3] == CREATELOCK && currentMsg.msg[4] == 'i'){
	printf("Received a CreateLock reply message.\n");
	int i = 5;
	int digits = 0;
	int prIndex;
	char* name = new char[MAX_SIZE];

	// pull out the pr index
	while(currentMsg.msg[i] != 'l') {
	  i++;
	  digits++;
	}

	printf("Current Msg %s.\n", currentMsg.msg);

	if(digits == 1) {
	  prIndex = currentMsg.msg[5] - 48;
	}
	else if(digits == 2) {
	  prIndex = ((currentMsg.msg[5]-48)*10) + (currentMsg.msg[6] - 48);
	}
	else if(digits == 3) {
	  prIndex = ((currentMsg.msg[5]-48)*100) + ((currentMsg.msg[6]-48)*10) +
	    (currentMsg.msg[7] - 48);
	}
	i++;

	// pull out the name
	for(int j = i; j < MAX_SIZE; j++) {
	  name[j - i] = currentMsg.msg[j];
	}
	
	printf("prIndex is %i.\n", prIndex);

	// check that the index is valid and active
	if(prIndex >= 0 && prIndex < nextPR && pendingRequests[prIndex].active) {
	  
	  if(currentMsg.msg[1] == 'y') {
	    printf("Received a yes.\n");
	    pendingRequests[prIndex].active = false;
	    outMailHeader.from = pendingRequests[prIndex].clientMailboxID;
	    outPacketHeader.from = pendingRequests[prIndex].clientMachineID;
	    outMailHeader.to = currentMsg.fromMailboxID;
	    outPacketHeader.to = currentMsg.fromMachineID;
	    outMailHeader.length = strlen(pendingRequests[prIndex].originalClientMsg) + 1;
	    postOffice->Send(outPacketHeader, outMailHeader, pendingRequests[prIndex].originalClientMsg);
	    outMailHeader.from = 0;
	    outPacketHeader.from = machineID;
	  }
	  else if(currentMsg.msg[1] == 'n') {
	    printf("Received a no for %i.\n", prIndex);
	    pendingRequests[prIndex].numberOfNoReplies++;
	    if(pendingRequests[prIndex].numberOfNoReplies == (numServers - 1)) {
	      printf("Creating Lock %s.\n", name);
	      CreateLock(name, pendingRequests[prIndex].clientMachineID, pendingRequests[prIndex].clientMailboxID);
	      pendingRequests[prIndex].active = false;
	    }
	  }
	}
      }  
      // this is an acquire reply
      if(currentMsg.msg[2] == 't' && currentMsg.msg[3] == ACQUIRE && currentMsg.msg[4] == 'i'){
	printf("Received a Acquire reply message.\n");
	int i = 5;
	int digits = 0;
	int prIndex;
	char* name = new char[MAX_SIZE];

	// pull out the pr index
	while(currentMsg.msg[i] != 'l') {
	  i++;
	  digits++;
	}

	printf("Current Msg %s.\n", currentMsg.msg);

	if(digits == 1) {
	  prIndex = currentMsg.msg[5] - 48;
	}
	else if(digits == 2) {
	  prIndex = ((currentMsg.msg[5]-48)*10) + (currentMsg.msg[6] - 48);
	}
	else if(digits == 3) {
	  prIndex = ((currentMsg.msg[5]-48)*100) + ((currentMsg.msg[6]-48)*10) +
	    (currentMsg.msg[7] - 48);
	}
	i++;

	// pull out the name
	for(int j = i; j < MAX_SIZE; j++) {
	  name[j - i] = currentMsg.msg[j];
	}
	
	printf("prIndex is %i.\n", prIndex);

	// check that we have a valid pending request
	if(prIndex >= 0 && prIndex < nextPR && pendingRequests[prIndex].active) {
	  
	  if(currentMsg.msg[1] == 'y') {
	    printf("Received a yes.\n");
	    pendingRequests[prIndex].active = false;
	    outMailHeader.from = pendingRequests[prIndex].clientMailboxID;
	    outPacketHeader.from = pendingRequests[prIndex].clientMachineID;
	    outMailHeader.to = currentMsg.fromMailboxID;
	    outPacketHeader.to = currentMsg.fromMachineID;
	    outMailHeader.length = strlen(pendingRequests[prIndex].originalClientMsg) + 1;
	    postOffice->Send(outPacketHeader, outMailHeader, pendingRequests[prIndex].originalClientMsg);
	    outMailHeader.from = 0;
	    outPacketHeader.from = machineID;
	  }
	  else if(currentMsg.msg[1] == 'n') {
	    printf("Received a no for %i.\n", prIndex);
	    pendingRequests[prIndex].numberOfNoReplies++;
	    if(pendingRequests[prIndex].numberOfNoReplies == (numServers - 1)) {
	      outMailHeader.to = pendingRequests[prIndex].clientMailboxID;
	      outPacketHeader.to = pendingRequests[prIndex].clientMachineID;
	      
	      char* response = new char[MAX_SIZE];
	      sprintf(response, "e");

	      outMailHeader.length = strlen(response) + 1;
	      postOffice->Send(outPacketHeader, outMailHeader, response);
	   
	      pendingRequests[prIndex].active = false;
	    }
	  }
	}
      }  
      // This is a release reply
      if(currentMsg.msg[2] == 't' && currentMsg.msg[3] == RELEASE && currentMsg.msg[4] == 'i'){
	printf("Received a Release reply message.\n");
	int i = 5;
	int digits = 0;
	int prIndex;
	char* name = new char[MAX_SIZE];

	// pull out the PR index
	while(currentMsg.msg[i] != 'l') {
	  i++;
	  digits++;
	}

	printf("Current Msg %s.\n", currentMsg.msg);

	if(digits == 1) {
	  prIndex = currentMsg.msg[5] - 48;
	}
	else if(digits == 2) {
	  prIndex = ((currentMsg.msg[5]-48)*10) + (currentMsg.msg[6] - 48);
	}
	else if(digits == 3) {
	  prIndex = ((currentMsg.msg[5]-48)*100) + ((currentMsg.msg[6]-48)*10) +
	    (currentMsg.msg[7] - 48);
	}
	i++;

	// Pull out the name
	for(int j = i; j < MAX_SIZE; j++) {
	  name[j - i] = currentMsg.msg[j];
	}
	
	printf("prIndex is %i.\n", prIndex);

	// Check that this PR is active
	if(prIndex >= 0 && prIndex < nextPR && pendingRequests[prIndex].active) {
	  
	  if(currentMsg.msg[1] == 'y') {
	    printf("Received a yes.\n");
	    pendingRequests[prIndex].active = false;
	    outMailHeader.from = pendingRequests[prIndex].clientMailboxID;
	    outPacketHeader.from = pendingRequests[prIndex].clientMachineID;
	    outMailHeader.to = currentMsg.fromMailboxID;
	    outPacketHeader.to = currentMsg.fromMachineID;
	    outMailHeader.length = strlen(pendingRequests[prIndex].originalClientMsg) + 1;
	    postOffice->Send(outPacketHeader, outMailHeader, pendingRequests[prIndex].originalClientMsg);
	    outMailHeader.from = 0;
	    outPacketHeader.from = machineID;
	    
	  }
	  else if(currentMsg.msg[1] == 'n') {
	    printf("Received a no for %i.\n", prIndex);
	    pendingRequests[prIndex].numberOfNoReplies++;
	    if(pendingRequests[prIndex].numberOfNoReplies == (numServers - 1)) {
	      outMailHeader.to = pendingRequests[prIndex].clientMailboxID;
	      outPacketHeader.to = pendingRequests[prIndex].clientMachineID;
	      
	      char* response = new char[MAX_SIZE];
	      sprintf(response, "e");

	      outMailHeader.length = strlen(response) + 1;
	      postOffice->Send(outPacketHeader, outMailHeader, response);
	   
	      pendingRequests[prIndex].active = false;
	    }
	  }
	}
      }  
      // This is a DestroyLock reply
      if(currentMsg.msg[2] == 't' && currentMsg.msg[3] == DESTROYLOCK && currentMsg.msg[4] == 'i'){
	printf("Received a DestroyLock reply message.\n");
	int i = 5;
	int digits = 0;
	int prIndex;
	char* name = new char[MAX_SIZE];

	// Pull out the PR index
	while(currentMsg.msg[i] != 'l') {
	  i++;
	  digits++;
	}

	printf("Current Msg %s.\n", currentMsg.msg);

	if(digits == 1) {
	  prIndex = currentMsg.msg[5] - 48;
	}
	else if(digits == 2) {
	  prIndex = ((currentMsg.msg[5]-48)*10) + (currentMsg.msg[6] - 48);
	}
	else if(digits == 3) {
	  prIndex = ((currentMsg.msg[5]-48)*100) + ((currentMsg.msg[6]-48)*10) +
	    (currentMsg.msg[7] - 48);
	}
	i++;

	// Pull out the name
	for(int j = i; j < MAX_SIZE; j++) {
	  name[j - i] = currentMsg.msg[j];
	}
	
	printf("prIndex is %i.\n", prIndex);

	// Check that this PR is valid
	if(prIndex >= 0 && prIndex < nextPR && pendingRequests[prIndex].active) {
	  
	  if(currentMsg.msg[1] == 'y') {
	    printf("Received a yes.\n");
	    pendingRequests[prIndex].active = false;
	    outMailHeader.from = pendingRequests[prIndex].clientMailboxID;
	    outPacketHeader.from = pendingRequests[prIndex].clientMachineID;
	    outMailHeader.to = currentMsg.fromMailboxID;
	    outPacketHeader.to = currentMsg.fromMachineID;
	    outMailHeader.length = strlen(pendingRequests[prIndex].originalClientMsg) + 1;
	    postOffice->Send(outPacketHeader, outMailHeader, pendingRequests[prIndex].originalClientMsg);
	    outMailHeader.from = 0;
	    outPacketHeader.from = machineID;
	  }
	  else if(currentMsg.msg[1] == 'n') {
	    printf("Received a no for %i.\n", prIndex);
	    pendingRequests[prIndex].numberOfNoReplies++;
	    if(pendingRequests[prIndex].numberOfNoReplies == (numServers - 1)) {
	      outMailHeader.to = pendingRequests[prIndex].clientMailboxID;
	      outPacketHeader.to = pendingRequests[prIndex].clientMachineID;
	      
	      char* response = new char[MAX_SIZE];
	      sprintf(response, "e");

	      outMailHeader.length = strlen(response) + 1;
	      postOffice->Send(outPacketHeader, outMailHeader, response);
	   
	      pendingRequests[prIndex].active = false;
	    }
	  }
	}
      }
      // This is a CreateCV reply
      else if(currentMsg.msg[2] == 't' && currentMsg.msg[3] == CREATECV && currentMsg.msg[4] == 'i') {
	printf("Received a CreateCV reply message.\n");
	int i = 5;
	int digits = 0;
	int prIndex;
	char* name = new char[MAX_SIZE];

	// Pull out the PR index
	while(currentMsg.msg[i] != 'c') {
	  i++;
	  digits++;
	}
	printf("Current Msg %s.\n", currentMsg.msg);
	
	if(digits == 1) {
	  prIndex = currentMsg.msg[5] - 48;
	}
	else if(digits == 2) {
	  prIndex = ((currentMsg.msg[5]-48)*10) + (currentMsg.msg[6]-48);
	}
	else if(digits == 3) {
	  prIndex = ((currentMsg.msg[5]-48)*100) + ((currentMsg.msg[6]-48)*10) + (currentMsg.msg[7]-48);
	}
	i++;

	// Pull out the name
	for(int j = i; j < MAX_SIZE; j++) {
	  name[j-i] = currentMsg.msg[j];
	}

	// Check that the PR is active
	if(prIndex >= 0 && prIndex < nextPR && pendingRequests[prIndex].active) {
	  printf("Got an active reply.\n");
	  if(currentMsg.msg[1] == 'y') {
	    printf("Received a yes.\n");
	    pendingRequests[prIndex].active = false;
	    outMailHeader.from = pendingRequests[prIndex].clientMailboxID;
	    outPacketHeader.from = pendingRequests[prIndex].clientMachineID;
	    outMailHeader.to = currentMsg.fromMailboxID;
	    outPacketHeader.to = currentMsg.fromMachineID;
	    outMailHeader.length = strlen(pendingRequests[prIndex].originalClientMsg) + 1;
	    postOffice->Send(outPacketHeader, outMailHeader, pendingRequests[prIndex].originalClientMsg);
	    outMailHeader.from = 0;
	    outPacketHeader.from = machineID;
	  }
	  else if(currentMsg.msg[1] == 'n') {
	    printf("Received a no for %i.\n", prIndex);
	    pendingRequests[prIndex].numberOfNoReplies++;
	    if(pendingRequests[prIndex].numberOfNoReplies == (numServers - 1)) {
	      printf("Creating CV %s.\n", name);
	      CreateCV(name, pendingRequests[prIndex].clientMachineID, pendingRequests[prIndex].clientMailboxID);
	      pendingRequests[prIndex].active = false;
	    }
	  }
	}
      }
      // This is a Wait reply
      else if(currentMsg.msg[2] == 't' && currentMsg.msg[3] == WAIT && currentMsg.msg[4] == 'i') {
	printf("Received a WAIT reply message.\n");
	int i = 5;
	int digits = 0;
	int prIndex;
	int type;
	char* name = new char[MAX_SIZE];

	// Pulling out the PR index
	while(currentMsg.msg[i] != 'c' && currentMsg.msg[i] != 'l') {
	  i++;
	  digits++;
	}
	printf("Current Msg %s.\n", currentMsg.msg);

	if(currentMsg.msg[i] == 'c') {
	  type = 0;
	}
	else if(currentMsg.msg[i] == 'l') {
	  type = 1;
	}
	
	if(digits == 1) {
	  prIndex = currentMsg.msg[5] - 48;
	}
	else if(digits == 2) {
	  prIndex = ((currentMsg.msg[5]-48)*10) + (currentMsg.msg[6]-48);
	}
	else if(digits == 3) {
	  prIndex = ((currentMsg.msg[5]-48)*100) + ((currentMsg.msg[6]-48)*10) + (currentMsg.msg[7]-48);
	}
	i++;

	// Pulling out the name
	for(int j = i; j < MAX_SIZE; j++) {
	  name[j-i] = currentMsg.msg[j];
	}

	// Checking if this PR is active
	if(prIndex >= 0 && prIndex < nextPR && pendingRequests[prIndex].active) {
	  printf("Got an active reply.\n");
	  if(currentMsg.msg[1] == 'y') {
	    printf("Received a yes.\n");
	    
	    if(type == 0) {
	      printf("Received a yes.\n");
	      pendingRequests[prIndex].active = false;
	      outMailHeader.from = pendingRequests[prIndex].clientMailboxID;
	      outPacketHeader.from = pendingRequests[prIndex].clientMachineID;
	      outMailHeader.to = currentMsg.fromMailboxID;
	      outPacketHeader.to = currentMsg.fromMachineID;
	      outMailHeader.length = strlen(pendingRequests[prIndex].originalClientMsg) + 1;
	      postOffice->Send(outPacketHeader, outMailHeader, pendingRequests[prIndex].originalClientMsg);
	      outMailHeader.from = 0;
	      outPacketHeader.from = machineID;
	    }
	    else if(type == 1) {
	      char* response = new char[MAX_SIZE];
	      waitingThread *thread = new waitingThread;
	      thread->machineID = pendingRequests[prIndex].clientMachineID;
	      thread->mailboxNum = pendingRequests[prIndex].clientMailboxID;
	      
	      int cvIndex = -1;
	      for(int k = 0; k < nextCV; k++) {
		if(*cvs[k].condition.name == *name) {
		  cvIndex = k;
		}
	      }

	      // Check that the index is valid
	      if(cvIndex < 0 || cvIndex >= nextCV) {
		printf("ERROR: The given CV index is not valid.\n");
		sprintf(response, "e%d", BADINDEX);
		outPacketHeader.to = pendingRequests[prIndex].clientMachineID;
		outMailHeader.to = pendingRequests[prIndex].clientMailboxID;
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
	      }
	      // Check that the CV has not been deleted
	      else if(cvs[cvIndex].isDeleted) {
		printf("ERROR: This condition has been deleted.\n");
		sprintf(response, "e%d", DELETED);
		outPacketHeader.to = pendingRequests[prIndex].clientMachineID;
		outMailHeader.to = pendingRequests[prIndex].clientMailboxID;
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
	      }
	      // Check that the CV is not to be deleted
	      else if(cvs[cvIndex].toBeDeleted) {
		printf("ERROR: This condition is to be deleted.\n");
		sprintf(response, "e%d", TOBEDELETED);
		outPacketHeader.to = pendingRequests[prIndex].clientMachineID;
		outMailHeader.to = pendingRequests[prIndex].clientMailboxID;
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
	      }
	      else {
		cvs[cvIndex].numActiveThreads++;
		cvs[cvIndex].condition.waitList->Append((void *)thread);
		sprintf(response, "s%di%s", RELEASE-48, name);
		outPacketHeader.to = currentMsg.fromMachineID;
		outMailHeader.to = currentMsg.fromMailboxID;
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
		pendingRequests[prIndex].active = false;
	      }
	    }
	  }
	  else if(currentMsg.msg[1] == 'n') {
	    printf("Received a no for %i.\n", prIndex);
	    pendingRequests[prIndex].numberOfNoReplies++;
	    if(pendingRequests[prIndex].numberOfNoReplies == (numServers - 1)) {
	      outMailHeader.to = pendingRequests[prIndex].clientMailboxID;
	      outPacketHeader.to = pendingRequests[prIndex].clientMachineID;
	      
	      char* response = new char[MAX_SIZE];
	      sprintf(response, "e");

	      outMailHeader.length = strlen(response) + 1;
	      postOffice->Send(outPacketHeader, outMailHeader, response);
	      pendingRequests[prIndex].active = false;
	    }
	  }
	}
      }
      else if(currentMsg.msg[2] == 't' && currentMsg.msg[3] == SIGNAL && currentMsg.msg[4] == 'i') {
	printf("Received a SIGNAL reply message.\n");
	int i = 5;
	int digits = 0;
	int prIndex;
	int type;
	char* name = new char[MAX_SIZE];

	// Pulling out the PR index
	while(currentMsg.msg[i] != 'c' && currentMsg.msg[i] != 'l') {
	  i++;
	  digits++;
	}
	printf("Current Msg %s.\n", currentMsg.msg);

	if(currentMsg.msg[i] == 'c') {
	  type = 0;
	}
	else if(currentMsg.msg[i] == 'l') {
	  type = 1;
	}
	
	if(digits == 1) {
	  prIndex = currentMsg.msg[5] - 48;
	}
	else if(digits == 2) {
	  prIndex = ((currentMsg.msg[5]-48)*10) + (currentMsg.msg[6]-48);
	}
	else if(digits == 3) {
	  prIndex = ((currentMsg.msg[5]-48)*100) + ((currentMsg.msg[6]-48)*10) + (currentMsg.msg[7]-48);
	}
	i++;

	// Pulling out the name
	for(int j = i; j < MAX_SIZE; j++) {
	  name[j-i] = currentMsg.msg[j];
	}

	// Checking if this PR is active
	if(prIndex >= 0 && prIndex < nextPR && pendingRequests[prIndex].active) {
	  printf("Got an active reply.\n");
	  if(currentMsg.msg[1] == 'y') {
	    printf("Received a yes.\n");
	    
	    if(type == 0) {
	      printf("Received a yes.\n");
	      pendingRequests[prIndex].active = false;
	      outMailHeader.from = pendingRequests[prIndex].clientMailboxID;
	      outPacketHeader.from = pendingRequests[prIndex].clientMachineID;
	      outMailHeader.to = currentMsg.fromMailboxID;
	      outPacketHeader.to = currentMsg.fromMachineID;
	      outMailHeader.length = strlen(pendingRequests[prIndex].originalClientMsg) + 1;
	      postOffice->Send(outPacketHeader, outMailHeader, pendingRequests[prIndex].originalClientMsg);
	      outMailHeader.from = 0;
	      outPacketHeader.from = machineID;
	    }
	    else if(type == 1) {
	      char* response = new char[MAX_SIZE];
	      waitingThread *thread = new waitingThread;
	      thread->machineID = pendingRequests[prIndex].clientMachineID;
	      thread->mailboxNum = pendingRequests[prIndex].clientMailboxID;
	      

	      int cvIndex = -1;
	      for(int k = 0; k < nextCV; k++) {
		if(*cvs[k].condition.name == *name) {
		  cvIndex = k;
		}
	      }

	       // Check that the index is valid
	      if(cvIndex < 0 || cvIndex >= nextCV) {
		printf("ERROR: The given CV index is not valid.\n");
		sprintf(response, "e%d", BADINDEX);
		outPacketHeader.to = pendingRequests[prIndex].clientMachineID;
		outMailHeader.to = pendingRequests[prIndex].clientMailboxID;
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
	      }
	      // Check that the CV has not been deleted
	      else if(cvs[cvIndex].isDeleted) {
		printf("ERROR: This condition has been deleted.\n");
		sprintf(response, "e%d", DELETED);
		outPacketHeader.to = pendingRequests[prIndex].clientMachineID;
		outMailHeader.to = pendingRequests[prIndex].clientMailboxID;
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
	      }
	      // Check that the CV is not to be deleted
	      else if(cvs[cvIndex].toBeDeleted) {
		printf("ERROR: This condition is to be deleted.\n");
		sprintf(response, "e%d", TOBEDELETED);
		outPacketHeader.to = pendingRequests[prIndex].clientMachineID;
		outMailHeader.to = pendingRequests[prIndex].clientMailboxID;
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
	      }
	      else {
		cvs[cvIndex].numActiveThreads--;
		thread=(waitingThread *)cvs[cvIndex].condition.waitList->Remove();
		if(cvs[cvIndex].condition.waitList->IsEmpty()) {
		  cvs[cvIndex].condition.lockIndex = -1;
		}
		
		// Check if it should now be deleted
		if(cvs[cvIndex].toBeDeleted && (cvs[cvIndex].numActiveThreads == 0)) {
		  cvs[cvIndex].isDeleted = true;
		  printf("Deleted condition %i.\n", cvIndex);
		}
		
		sprintf(response, "s%dn%s", ACQUIRE-48, name);
		outPacketHeader.to = pendingRequests[prIndex].clientMachineID;
		outMailHeader.to = pendingRequests[prIndex].clientMailboxID;
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
		pendingRequests[prIndex].active = false;
	      }
	    }
	  }
	  else if(currentMsg.msg[1] == 'n') {
	    printf("Received a no for %i.\n", prIndex);
	    pendingRequests[prIndex].numberOfNoReplies++;
	    if(pendingRequests[prIndex].numberOfNoReplies == (numServers - 1)) {
	      outMailHeader.to = pendingRequests[prIndex].clientMailboxID;
	      outPacketHeader.to = pendingRequests[prIndex].clientMachineID;
	      
	      char* response = new char[MAX_SIZE];
	      sprintf(response, "e");

	      outMailHeader.length = strlen(response) + 1;
	      postOffice->Send(outPacketHeader, outMailHeader, response);
	      pendingRequests[prIndex].active = false;
	    }
	  }
	}
      }
      else if(currentMsg.msg[2] == 't' && currentMsg.msg[3] == BROADCAST && currentMsg.msg[4] == 'i') {
	printf("Received a BROADCAST reply message.\n");
	int i = 5;
	int digits = 0;
	int prIndex;
	int type;
	char* name = new char[MAX_SIZE];

	// Pulling out the PR index
	while(currentMsg.msg[i] != 'c' && currentMsg.msg[i] != 'l') {
	  i++;
	  digits++;
	}
	printf("Current Msg %s.\n", currentMsg.msg);

	if(currentMsg.msg[i] == 'c') {
	  type = 0;
	}
	else if(currentMsg.msg[i] == 'l') {
	  type = 1;
	}
	
	if(digits == 1) {
	  prIndex = currentMsg.msg[5] - 48;
	}
	else if(digits == 2) {
	  prIndex = ((currentMsg.msg[5]-48)*10) + (currentMsg.msg[6]-48);
	}
	else if(digits == 3) {
	  prIndex = ((currentMsg.msg[5]-48)*100) + ((currentMsg.msg[6]-48)*10) + (currentMsg.msg[7]-48);
	}
	i++;

	// Pulling out the name
	for(int j = i; j < MAX_SIZE; j++) {
	  name[j-i] = currentMsg.msg[j];
	}

	// Checking if this PR is active
	if(prIndex >= 0 && prIndex < nextPR && pendingRequests[prIndex].active) {
	  printf("Got an active reply.\n");
	  if(currentMsg.msg[1] == 'y') {
	    printf("Received a yes.\n");
	    
	    if(type == 0) {
	      printf("Received a yes.\n");
	      pendingRequests[prIndex].active = false;
	      outMailHeader.from = pendingRequests[prIndex].clientMailboxID;
	      outPacketHeader.from = pendingRequests[prIndex].clientMachineID;
	      outMailHeader.to = currentMsg.fromMailboxID;
	      outPacketHeader.to = currentMsg.fromMachineID;
	      outMailHeader.length = strlen(pendingRequests[prIndex].originalClientMsg) + 1;
	      postOffice->Send(outPacketHeader, outMailHeader, pendingRequests[prIndex].originalClientMsg);
	      outMailHeader.from = 0;
	      outPacketHeader.from = machineID;
	    }
	    else if(type == 1) {
	      char* response = new char[MAX_SIZE];
	      waitingThread *thread = new waitingThread;
	      thread->machineID = pendingRequests[prIndex].clientMachineID;
	      thread->mailboxNum = pendingRequests[prIndex].clientMailboxID;
	      
	      int cvIndex = -1;
	      for(int k = 0; k < nextCV; k++) {
		if(*cvs[k].condition.name == *name) {
		  cvIndex = k;
		}
	      }

	       // Check that the index is valid
	      if(cvIndex < 0 || cvIndex >= nextCV) {
		printf("ERROR: The given CV index is not valid.\n");
		sprintf(response, "e%d", BADINDEX);
		outPacketHeader.to = pendingRequests[prIndex].clientMachineID;
		outMailHeader.to = pendingRequests[prIndex].clientMailboxID;
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
	      }
	      // Check that the CV has not been deleted
	      else if(cvs[cvIndex].isDeleted) {
		printf("ERROR: This condition has been deleted.\n");
		sprintf(response, "e%d", DELETED);
		outPacketHeader.to = pendingRequests[prIndex].clientMachineID;
		outMailHeader.to = pendingRequests[prIndex].clientMailboxID;
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
	      }
	      // Check that the CV is not to be deleted
	      else if(cvs[cvIndex].toBeDeleted) {
		printf("ERROR: This condition is to be deleted.\n");
		sprintf(response, "e%d", TOBEDELETED);
		outPacketHeader.to = pendingRequests[prIndex].clientMachineID;
		outMailHeader.to = pendingRequests[prIndex].clientMailboxID;
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);
	      }
	      else {
		outMailHeader.to = pendingRequests[prIndex].clientMailboxID;
		outPacketHeader.to = pendingRequests[prIndex].clientMachineID;

		if(cvs[cvIndex].condition.waitList->IsEmpty()) {
		  sprintf(response, "s");
		}

		while(!cvs[cvIndex].condition.waitList->IsEmpty()) {
		  printf("Calling Acquire on waiting threads.\n");
		  thread = (waitingThread *)cvs[cvIndex].condition.waitList->Remove();
		  sprintf(response, "s%dn%s", ACQUIRE-48, name);
		}
		postOffice->Send(outPacketHeader, outMailHeader, response);
	      }
	    }
	  }
	  else if(currentMsg.msg[1] == 'n') {
	    printf("Received a no for %i.\n", prIndex);
	    pendingRequests[prIndex].numberOfNoReplies++;
	    if(pendingRequests[prIndex].numberOfNoReplies == (numServers - 1)) {
	      outMailHeader.to = pendingRequests[prIndex].clientMailboxID;
	      outPacketHeader.to = pendingRequests[prIndex].clientMachineID;
	      
	      char* response = new char[MAX_SIZE];
	      sprintf(response, "e");

	      outMailHeader.length = strlen(response) + 1;
	      postOffice->Send(outPacketHeader, outMailHeader, response);
	      pendingRequests[prIndex].active = false;
	    }
	  }
	}
      }
      // This is a DestroyCV reply
      else if(currentMsg.msg[2] == 't' && currentMsg.msg[3] == DESTROYCV && currentMsg.msg[4] == 'i') {
	printf("Received a DestroyCV reply message.\n");
	int i = 5;
	int digits = 0;
	int prIndex;
	char* name = new char[MAX_SIZE];

	// Pull out the PR index
	while(currentMsg.msg[i] != 'c') {
	  i++;
	  digits++;
	}
	printf("Current Msg %s.\n", currentMsg.msg);
	
	if(digits == 1) {
	  prIndex = currentMsg.msg[5] - 48;
	}
	else if(digits == 2) {
	  prIndex = ((currentMsg.msg[5]-48)*10) + (currentMsg.msg[6]-48);
	}
	else if(digits == 3) {
	  prIndex = ((currentMsg.msg[5]-48)*100) + ((currentMsg.msg[6]-48)*10) + (currentMsg.msg[7]-48);
	}
	i++;

	// Pull out the name
	for(int j = i; j < MAX_SIZE; j++) {
	  name[j-i] = currentMsg.msg[j];
	}

	// Check that the PR is active
	if(prIndex >= 0 && prIndex < nextPR && pendingRequests[prIndex].active) {
	  printf("Got an active reply.\n");
	  if(currentMsg.msg[1] == 'y') {
	    printf("Received a yes.\n");
	    pendingRequests[prIndex].active = false;
	    outMailHeader.from = pendingRequests[prIndex].clientMailboxID;
	    outPacketHeader.from = pendingRequests[prIndex].clientMachineID;
	    outMailHeader.to = currentMsg.fromMailboxID;
	    outPacketHeader.to = currentMsg.fromMachineID;
	    outMailHeader.length = strlen(pendingRequests[prIndex].originalClientMsg) + 1;
	    postOffice->Send(outPacketHeader, outMailHeader, pendingRequests[prIndex].originalClientMsg);
	    outMailHeader.from = 0;
	    outPacketHeader.from = machineID;
	  }
	  else if(currentMsg.msg[1] == 'n') {
	    printf("Received a no for %i.\n", prIndex);
	    pendingRequests[prIndex].numberOfNoReplies++;
	    if(pendingRequests[prIndex].numberOfNoReplies == (numServers - 1)) {
	      outMailHeader.to = pendingRequests[prIndex].clientMailboxID;
	      outPacketHeader.to = pendingRequests[prIndex].clientMachineID;
	      
	      char* response = new char[MAX_SIZE];
	      sprintf(response, "e");

	      outMailHeader.length = strlen(response) + 1;
	      postOffice->Send(outPacketHeader, outMailHeader, response);
	   
	      pendingRequests[prIndex].active = false;
	    }
	  }
	}
      }
      // This is a CreateMV reply
      else if(currentMsg.msg[2] == 't' && currentMsg.msg[3] == CREATEMV && currentMsg.msg[4] == 'i') {
	printf("Received a CreateMV reply message.\n");
	int i = 5;
	int digits = 0;
	int prIndex;
	char* name = new char[MAX_SIZE];

	// Pull out the PR index
	while(currentMsg.msg[i] != 'm') {
	  i++;
	  digits++;
	}
	printf("Current Msg %s.\n", currentMsg.msg);
	
	if(digits == 1) {
	  prIndex = currentMsg.msg[5] - 48;
	}
	else if(digits == 2) {
	  prIndex = ((currentMsg.msg[5]-48)*10) + (currentMsg.msg[6]-48);
	}
	else if(digits == 3) {
	  prIndex = ((currentMsg.msg[5]-48)*100) + ((currentMsg.msg[6]-48)*10) + (currentMsg.msg[7]-48);
	}
	i++;

	// Pull out the name
	for(int j = i; j < MAX_SIZE; j++) {
	  name[j-i] = currentMsg.msg[j];
	}

	// Check that the PR is active
	if(prIndex >= 0 && prIndex < nextPR && pendingRequests[prIndex].active) {
	  printf("Got an active reply.\n");
	  if(currentMsg.msg[1] == 'y') {
	    printf("Received a yes.\n");
	    pendingRequests[prIndex].active = false;
	    outMailHeader.from = pendingRequests[prIndex].clientMailboxID;
	    outPacketHeader.from = pendingRequests[prIndex].clientMachineID;
	    outMailHeader.to = currentMsg.fromMailboxID;
	    outPacketHeader.to = currentMsg.fromMachineID;
	    outMailHeader.length = strlen(pendingRequests[prIndex].originalClientMsg) + 1;
	    postOffice->Send(outPacketHeader, outMailHeader, pendingRequests[prIndex].originalClientMsg);
	    outMailHeader.from = 0;
	    outPacketHeader.from = machineID;
	  }
	  else if(currentMsg.msg[1] == 'n') {
	    printf("Received a no for %i.\n", prIndex);
	    pendingRequests[prIndex].numberOfNoReplies++;
	    if(pendingRequests[prIndex].numberOfNoReplies == (numServers - 1)) {
	      printf("Creating MV %s.\n", name);
	      CreateMV(name, pendingRequests[prIndex].clientMachineID, pendingRequests[prIndex].clientMailboxID);
	      pendingRequests[prIndex].active = false;
	    }
	  }
	}
      }
      // This is a DestroyMV reply
      else if(currentMsg.msg[2] == 't' && currentMsg.msg[3] == DESTROYMV1 && currentMsg.msg[4] == DESTROYMV2 && currentMsg.msg[5] == 'i') {
	printf("Received a DestroyCV reply message.\n");
	int i = 6;
	int digits = 0;
	int prIndex;
	char* name = new char[MAX_SIZE];

	// Pull out the PR index
	while(currentMsg.msg[i] != 'm') {
	  i++;
	  digits++;
	}
	printf("Current Msg %s.\n", currentMsg.msg);
	
	if(digits == 1) {
	  prIndex = currentMsg.msg[6] - 48;
	}
	else if(digits == 2) {
	  prIndex = ((currentMsg.msg[6]-48)*10) + (currentMsg.msg[7]-48);
	}
	else if(digits == 3) {
	  prIndex = ((currentMsg.msg[6]-48)*100) + ((currentMsg.msg[7]-48)*10) + (currentMsg.msg[8]-48);
	}
	i++;

	// Pull out the name
	for(int j = i; j < MAX_SIZE; j++) {
	  name[j-i] = currentMsg.msg[j];
	}

	// Check that the PR is active
	if(prIndex >= 0 && prIndex < nextPR && pendingRequests[prIndex].active) {
	  printf("Got an active reply.\n");
	  if(currentMsg.msg[1] == 'y') {
	    printf("Received a yes.\n");
	    pendingRequests[prIndex].active = false;
	    outMailHeader.from = pendingRequests[prIndex].clientMailboxID;
	    outPacketHeader.from = pendingRequests[prIndex].clientMachineID;
	    outMailHeader.to = currentMsg.fromMailboxID;
	    outPacketHeader.to = currentMsg.fromMachineID;
	    outMailHeader.length = strlen(pendingRequests[prIndex].originalClientMsg) + 1;
	    postOffice->Send(outPacketHeader, outMailHeader, pendingRequests[prIndex].originalClientMsg);
	    outMailHeader.from = 0;
	    outPacketHeader.from = machineID;
	  }
	  else if(currentMsg.msg[1] == 'n') {
	    printf("Received a no for %i.\n", prIndex);
	    pendingRequests[prIndex].numberOfNoReplies++;
	    if(pendingRequests[prIndex].numberOfNoReplies == (numServers - 1)) {
	      outMailHeader.to = pendingRequests[prIndex].clientMailboxID;
	      outPacketHeader.to = pendingRequests[prIndex].clientMachineID;
	      
	      char* response = new char[MAX_SIZE];
	      sprintf(response, "e");

	      outMailHeader.length = strlen(response) + 1;
	      postOffice->Send(outPacketHeader, outMailHeader, response);
	   
	      pendingRequests[prIndex].active = false;
	    }
	  }
	}
      }
      // This is a Set reply
      else if(currentMsg.msg[2] == 't' && currentMsg.msg[3] == SET1 && currentMsg.msg[4] == SET2 && currentMsg.msg[5] == 'i') {
	printf("Received a Set reply message.\n");
	int i = 6;
	int digits = 0;
	int prIndex;
	char* name = new char[MAX_SIZE];

	// Pull out the PR index
	while(currentMsg.msg[i] != 'm') {
	  i++;
	  digits++;
	}
	printf("Current Msg %s.\n", currentMsg.msg);
	
	if(digits == 1) {
	  prIndex = currentMsg.msg[6] - 48;
	}
	else if(digits == 2) {
	  prIndex = ((currentMsg.msg[6]-48)*10) + (currentMsg.msg[7]-48);
	}
	else if(digits == 3) {
	  prIndex = ((currentMsg.msg[6]-48)*100) + ((currentMsg.msg[7]-48)*10) + (currentMsg.msg[8]-48);
	}
	i++;

	// Pull out the name
	for(int j = i; j < MAX_SIZE; j++) {
	  name[j-i] = currentMsg.msg[j];
	}

	// Check that this is an active PR
	if(prIndex >= 0 && prIndex < nextPR && pendingRequests[prIndex].active) {
	  printf("Got an active reply.\n");
	  if(currentMsg.msg[1] == 'y') {
	    printf("Received a yes.\n");
	    pendingRequests[prIndex].active = false;
	    outMailHeader.from = pendingRequests[prIndex].clientMailboxID;
	    outPacketHeader.from = pendingRequests[prIndex].clientMachineID;
	    outMailHeader.to = currentMsg.fromMailboxID;
	    outPacketHeader.to = currentMsg.fromMachineID;
	    outMailHeader.length = strlen(pendingRequests[prIndex].originalClientMsg) + 1;
	    postOffice->Send(outPacketHeader, outMailHeader, pendingRequests[prIndex].originalClientMsg);
	    outMailHeader.from = 0;
	    outPacketHeader.from = machineID;
	  }
	  else if(currentMsg.msg[1] == 'n') {
	    printf("Received a no for %i.\n", prIndex);
	    pendingRequests[prIndex].numberOfNoReplies++;
	    if(pendingRequests[prIndex].numberOfNoReplies == (numServers - 1)) {
	      outMailHeader.to = pendingRequests[prIndex].clientMailboxID;
	      outPacketHeader.to = pendingRequests[prIndex].clientMachineID;
	      
	      char* response = new char[MAX_SIZE];

	      sprintf(response, "e");
	      outMailHeader.length = strlen(response) + 1;
	      postOffice->Send(outPacketHeader, outMailHeader, response);
	      pendingRequests[prIndex].active = false;
	    }
	  }
	}
      }
      else if(currentMsg.msg[2] == 't' && currentMsg.msg[3] == GET1 && currentMsg.msg[4] == GET2 && currentMsg.msg[5] == 'i') {
	printf("Received a Get reply message.\n");
	int i = 6;
	int digits = 0;
	int prIndex;
	char* name = new char[MAX_SIZE];

	// Pull out the PR index
	while(currentMsg.msg[i] != 'm') {
	  i++;
	  digits++;
	}
	printf("Current Msg %s.\n", currentMsg.msg);
	
	if(digits == 1) {
	  prIndex = currentMsg.msg[6] - 48;
	}
	else if(digits == 2) {
	  prIndex = ((currentMsg.msg[6]-48)*10) + (currentMsg.msg[7]-48);
	}
	else if(digits == 3) {
	  prIndex = ((currentMsg.msg[6]-48)*100) + ((currentMsg.msg[7]-48)*10) + (currentMsg.msg[8]-48);
	}
	i++;

	// Pull out the name
	for(int j = i; j < MAX_SIZE; j++) {
	  name[j-i] = currentMsg.msg[j];
	}

	// Check that this is an active PR
	if(prIndex >= 0 && prIndex < nextPR && pendingRequests[prIndex].active) {
	  printf("Got an active reply.\n");
	  if(currentMsg.msg[1] == 'y') {
	    printf("Received a yes.\n");
	    pendingRequests[prIndex].active = false;
	    outMailHeader.from = pendingRequests[prIndex].clientMailboxID;
	    outPacketHeader.from = pendingRequests[prIndex].clientMachineID;
	    outMailHeader.to = currentMsg.fromMailboxID;
	    outPacketHeader.to = currentMsg.fromMachineID;
	    outMailHeader.length = strlen(pendingRequests[prIndex].originalClientMsg) + 1;
	    postOffice->Send(outPacketHeader, outMailHeader, pendingRequests[prIndex].originalClientMsg);
	    outMailHeader.from = 0;
	    outPacketHeader.from = machineID;
	  }
	  else if(currentMsg.msg[1] == 'n') {
	    printf("Received a no for %i.\n", prIndex);
	    pendingRequests[prIndex].numberOfNoReplies++;
	    if(pendingRequests[prIndex].numberOfNoReplies == (numServers - 1)) {
	      outMailHeader.to = pendingRequests[prIndex].clientMailboxID;
	      outPacketHeader.to = pendingRequests[prIndex].clientMachineID;
	      
	      char* response = new char[MAX_SIZE];

	      sprintf(response, "e");
	      outMailHeader.length = strlen(response) + 1;
	      postOffice->Send(outPacketHeader, outMailHeader, response);
	      pendingRequests[prIndex].active = false;
	    }
	  }
	}
      }
    }
  }
}
*/
