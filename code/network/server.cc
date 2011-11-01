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

int nextLock = 0;
int nextCV = 0;
int nextMVPos = 0;

class serverLock {

public:
  char* name;
  bool available;
  int machineID;
  int mailboxNum;
  List* waitList;

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

class serverCV {

public:
  char* name;
  int lockIndex;
  int machineID;
  int mailboxNum;
  List* waitList;


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

class waitingThread {
public:
  int machineID;
  int mailboxNum;

  waitingThread::waitingThread() {
    machineID = -1;
    mailboxNum = -1;
  }
};

struct lockData {
  serverLock lock;
  bool isDeleted;
  bool toBeDeleted;
  int numActiveThreads;
};

struct cvData {
  serverCV condition;
  bool isDeleted;
  bool toBeDeleted;
  int numActiveThreads;
};

struct mvData {
  int values[1000];
  int size;
  bool isDeleted;
  char* name;
};

lockData lcks[MAX_LOCKS];
cvData   cvs[MAX_CVS];
mvData   mvs[MAX_MVS];

void CreateLock(char* msg) {
  
  lockData myLock;
  char* response = new char[MAX_SIZE];
  char* name = new char[MAX_SIZE - 3];

  printf("MSG %s.\n", msg);

  for(int i = 3; i < MAX_SIZE + 1; i++) {
    name[i-3] = msg[i];
  }

  printf("Name %s.\n", name);

  printf("NextLock %i.\n", nextLock);

  for(int i = 0; i < nextLock; i++) {
    printf("Lock %i %s.\n", i, lcks[i].lock.name);
    if(*lcks[i].lock.name == *name) {
      sprintf(response, "s%d", i);
      outMailHeader.length = strlen(response) + 1;
      postOffice->Send(outPacketHeader,outMailHeader, response);
      return;
    }
  } 

  myLock.isDeleted = false;
  myLock.toBeDeleted = false;
  myLock.numActiveThreads = 0;
  myLock.lock.name = new char[MAX_SIZE];
  
  sprintf(myLock.lock.name, "%s", name);

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

void Acquire(int index) {

  char* response = new char[MAX_SIZE];

  if(index < 0 || index >= nextLock) {
		printf("ERROR: Bad index on acquire\n");
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  else if(lcks[index].isDeleted) {
		printf("ERROR: Lock is already deleted\n");
    sprintf(response, "e%d", DELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  else if(lcks[index].toBeDeleted) {
		printf("ERROR: Lock is already to be deleted\n");
    sprintf(response, "e%d", TOBEDELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  if(lcks[index].lock.machineID == inPacketHeader.from && lcks[index].lock.mailboxNum == inMailHeader.from) {
    printf("This thread already has the lock.\n");
    sprintf(response, "%d", OWNER);
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  lcks[index].numActiveThreads++;

  if(lcks[index].lock.available) {
    lcks[index].lock.available = false;
    lcks[index].lock.machineID = inPacketHeader.from;
    lcks[index].lock.mailboxNum = inMailHeader.from;
    sprintf(response, "%d", SUCCESS);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }
  else {
		DEBUG('s', "Lock %i is unavailable, time to wait\n", index);
    waitingThread* wt = new waitingThread;
    wt->machineID = inPacketHeader.from;
    wt->mailboxNum = inMailHeader.from;
    lcks[index].lock.waitList->Append((void*)wt);
  } 
}

void Release(int index) {

  char* response = new char[MAX_SIZE];
  waitingThread* thread;

  if(index < 0 || index >= nextLock) {
		printf("ERROR: Bad index on release\n");
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

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

  if(lcks[index].lock.machineID != inPacketHeader.from || lcks[index].lock.mailboxNum != inMailHeader.from) {
    printf("ERROR: Not the lock owner!\n");
    sprintf(response, "e%d", WRONGPROCESS);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }
  
  if(!(lcks[index].lock.waitList->IsEmpty())) {
   	thread = (waitingThread*)lcks[index].lock.waitList->Remove();

    lcks[index].lock.machineID = thread->machineID;
    lcks[index].lock.mailboxNum = thread->mailboxNum;
    sprintf(response, "%d", SUCCESS);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    char* response2 = new char[MAX_SIZE];
    sprintf(response2, "%d", SUCCESS);
    outMailHeader.to = thread->mailboxNum;
    outPacketHeader.to = thread->machineID;
    outMailHeader.length = strlen(response2) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response2);
  }
  else {
    lcks[index].lock.available = true;
    lcks[index].lock.machineID = -1;
    lcks[index].lock.mailboxNum = -1;
    sprintf(response, "%d", SUCCESS);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
  }
  return;
}

void DestroyLock(int index) {

  char* response = new char[MAX_SIZE];
 
  if(index < 0 || index >= nextLock) {
		printf("ERROR: Bad index on destroy lock\n");
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  else if(lcks[index].isDeleted) {
		printf("ERROR: This lock is already deleted\n");
    sprintf(response, "e%d", DELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  else if(lcks[index].toBeDeleted) {
		printf("ERROR: This lock is already to be deleted\n");
    sprintf(response, "e%d", TOBEDELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  else if(lcks[index].numActiveThreads > 0) {
    lcks[index].toBeDeleted = true;
    sprintf(response, "e%d", TOBEDELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  else {
    lcks[index].isDeleted = true;
    sprintf(response, "s");
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
  }
  return;
}

void CreateCV(char* msg) {

  cvData myCV;
	char* response = new char[MAX_SIZE];
	char* name = new char[MAX_SIZE - 3];
	
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
	
	myCV.condition.name = name;
  myCV.isDeleted = false;
  myCV.toBeDeleted = false;
  myCV.numActiveThreads = 0;

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

void Wait(char* msg) {
	DEBUG('b', "Entered wait call on server side\n");
	char* index = new char[MAX_SIZE];
  char* response = new char[MAX_SIZE];
  waitingThread *thread = new waitingThread;
  thread->machineID = inPacketHeader.from;
  thread->mailboxNum = inMailHeader.from;
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
  if(cvIndex < 0 || cvIndex >= nextCV) {
    printf("ERROR: The given cv index is not valid.\n");
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  } 

  else if (lockIndex < 0 || lockIndex >= nextLock) {
    printf("ERROR: The given lock index is not valid.\n");
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  } 

  else if (cvs[cvIndex].isDeleted) {
    printf("ERROR: This condition has been deleted.\n");
    sprintf(response, "e%d", DELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  } 

  else if (cvs[cvIndex].toBeDeleted) {
    printf("ERROR: This condition is to be deleted.\n");
    sprintf(response, "e%d", TOBEDELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  } 

  else if (lcks[lockIndex].isDeleted) {
    printf("ERROR: This lock has been deleted.\n");
    sprintf(response, "e%d", DELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  } 

  else if (lcks[lockIndex].toBeDeleted) {
    printf("ERROR: This lock is to be deleted.\n");
    sprintf(response, "e%d", TOBEDELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }
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

void Signal(char* msg) {
	char* index = new char[MAX_SIZE];
  char* response = new char[MAX_SIZE];
  waitingThread *thread = new waitingThread;
  thread->machineID = inPacketHeader.from;
  thread->mailboxNum = inMailHeader.from;
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
  if(cvIndex < 0 || cvIndex >= nextCV) {
    printf("ERROR: The given cv index is not valid.\n");
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  } 

  else if (lockIndex < 0 || lockIndex >= nextLock) {
    printf("ERROR: The given lock index is not valid.\n");
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  } 

  else if (cvs[cvIndex].isDeleted) {
    printf("ERROR: This condition has been deleted.\n");
    sprintf(response, "e%d", DELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  } 

  else if (lcks[lockIndex].isDeleted) {
    printf("ERROR: This lock has been deleted.\n");
    sprintf(response, "e%d", DELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  } 
	else if(cvs[cvIndex].numActiveThreads < 1){
		printf("ERROR: This condition variable has no threads waiting\n");
		sprintf(response, "e%d", EMPTYLIST);
		outMailHeader.length = strlen(response) + 1;
		postOffice->Send(outPacketHeader, outMailHeader, response);	
		return;
	}
  else if(cvs[cvIndex].condition.waitList->IsEmpty()) {
    printf("ERROR: waitList is empty!\n");
    sprintf(response, "e%d", EMPTYLIST);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

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

  if(cvs[cvIndex].condition.waitList->IsEmpty()) {
    cvs[cvIndex].condition.lockIndex = -1;
  }  

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

void Broadcast(char* msg) {
	waitingThread *thread = new waitingThread;
  char* index = new char[MAX_SIZE];
  char* response = new char[MAX_SIZE];
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

  if(cvIndex < 0 || cvIndex >= nextCV) {
    printf("ERROR: The given cv index is not valid.\n");
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  } 

  else if (lockIndex < 0 || lockIndex >= nextLock) {
    printf("ERROR: The given lock index is not valid.\n");
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  } 

  else if (cvs[cvIndex].isDeleted) {
    printf("ERROR: This condition has been deleted.\n");
    sprintf(response, "e%d", DELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  } 

  else if (lcks[lockIndex].isDeleted) {
    printf("ERROR: This lock has been deleted.\n");
    sprintf(response, "e%d", DELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }  
 	else if(cvs[cvIndex].condition.waitList->IsEmpty()) {
    printf("ERROR: waitList is empty!\n");
    sprintf(response, "e%d", EMPTYLIST);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  else if(cvs[cvIndex].condition.lockIndex != lockIndex) {
    printf("ERROR: Wrong lock.\n");
    sprintf(response, "e%d", WRONGLOCK);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }
  while(!cvs[cvIndex].condition.waitList->IsEmpty()) {
		DEBUG('s', "Broadcast: Waiting for waitList to empty\n");
  	thread = (waitingThread *)cvs[cvIndex].condition.waitList->Remove();
		lcks[lockIndex].lock.waitList->Append((void *)thread);  
  }
	
  cvs[cvIndex].numActiveThreads = 0;
	cvs[cvIndex].condition.lockIndex = -1;
  if(cvs[cvIndex].toBeDeleted && (cvs[cvIndex].numActiveThreads == 0)) {
    cvs[cvIndex].isDeleted = true;
    printf("Deleted condition %i.\n", cvIndex);
  }
  sprintf(response, "%d", SUCCESS);
  outMailHeader.length = strlen(response) + 1;
  postOffice->Send(outPacketHeader, outMailHeader, response);
}

void DestroyCV(char* msg) {
	DEBUG('r', "Entered DestroyCV\n");
	char* index = new char[MAX_SIZE-3];
  char* response = new char[MAX_SIZE];
	DEBUG('r', "Msg: %s\n", msg);
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

  if(cvIndex < 0 || cvIndex >= nextCV) {
		printf("CV has a bad index!\n");
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  else if(cvs[cvIndex].isDeleted) {
		printf("CV is already deleted!\n");
    sprintf(response, "e%d", DELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  else if(cvs[cvIndex].toBeDeleted) {
		printf("CV is to be deleted!\n");
    sprintf(response, "e%d", TOBEDELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  else if(cvs[cvIndex].numActiveThreads > 0) {
		printf("CV has threads still waiting on it, set to be deleted!\n");
    cvs[cvIndex].toBeDeleted = true;
    sprintf(response, "e%d", TOBEDELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  else {
		printf("CV at %i is deleted!\n", cvIndex);
    cvs[cvIndex].isDeleted = true;
    sprintf(response, "s%d", SUCCESS);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
  }
  return;

}

void CreateMV(char* msg) {

  mvData myMV;

  char* response = new char[MAX_SIZE];
  char* name = new char[MAX_SIZE - 3];

  if(msg[2] == 't') {
    myMV.size = (msg[3]-48);
    for(int i = 5; i < MAX_SIZE + 1; i++) {
      name[i-5] = msg[i];
    }
  }
  else if(msg[2] == 'h') {
    myMV.size = ((msg[3]*10-48) + (msg[4]-48));
    for(int i = 6; i < MAX_SIZE + 1; i++) {
      name[i-6] = msg[i];
    }
  }
  else if(msg[2] == 'k') {
    myMV.size = ((msg[3]*100-48) + (msg[4]*10-48) + (msg[5]-48));
    for(int i = 7; i < MAX_SIZE + 1; i++) {
      name[i-7] = msg[i];
    }
  }  

  for(int i = 0; i < nextMVPos; i++) {
    if(*mvs[i].name == *name) {
			DEBUG('s', "CreateMV name found %s at index %i\n", name, i);
      sprintf(response, "s%d", i);
      outMailHeader.length = strlen(response) + 1;
      postOffice->Send(outPacketHeader,outMailHeader, response);
      return;
    }
  } 

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

void Set(int index, int value, int arrayIndex) {

  char* response = new char[MAX_SIZE];
	DEBUG('s', "Set index: %i, value: %i, arrayIndex: %i\n", index, value, arrayIndex);
  if(index < 0 || index >= nextMVPos) {
		DEBUG('s', "Set: Bad index\n");
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  if(arrayIndex < 0 || arrayIndex >= mvs[index].size-1) {
		DEBUG('s', "Set: Bad arrayIndex\n");
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  if(mvs[index].isDeleted) {
		DEBUG('s', "Set: Is Deleted\n");
    sprintf(response, "e%d", DELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }
  mvs[index].values[arrayIndex] = value;
	DEBUG('s', "Set value: %i, index: %i\n", mvs[index].values[arrayIndex], index);
  sprintf(response, "s");
  outMailHeader.length = strlen(response) + 1;
  postOffice->Send(outPacketHeader, outMailHeader, response);
  return;
}

void Get(int index, int arrayIndex) {

  char* response = new char[MAX_SIZE];
  if(index < 0 || index >= nextMVPos) {
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  if(arrayIndex < 0 || arrayIndex >= mvs[index].size-1) {
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

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

void DestroyMV(int index) {

  char* response = new char[MAX_SIZE];

  if(index < 0 || index >= nextMVPos) {
    sprintf(response, "e%d", BADINDEX);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

  else if(mvs[index].isDeleted) {
    sprintf(response, "e%d", DELETED);
    outMailHeader.length = strlen(response) + 1;
    postOffice->Send(outPacketHeader, outMailHeader, response);
    return;
  }

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

void Server() {

  while(true) {
    
    printf("***\nEntered Server loop.\n");
    message = new char[MAX_SIZE];

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


