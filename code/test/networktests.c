/* networktests.c
 * Simple program to test the networking for Project 3
 */

#include "syscall.h"

/* TESTS TO MAKE
 
*CreateLock
*DestroyLock
*CreateCV
*DestroyCV
*AcquireLock
*ReleaseLock
*Wait
*Signal
*Broadcast
*CreateMV
*DestroyMV
*Set
*Get
*/

/* Globals */
int lock_1;
int lock_2;
int lock_3;
int lock_4;
int lock_5;
int lock_6;
int lock_7;
int lock_8;
int lock_9;
int lock_10;
int lock_11;

int cv_1;
int cv_2;
int cv_3;
int cv_4;
int cv_5;
int cv_6;
int cv_7;
int cv_8;
int cv_9;
int cv_10;
int cv_11;

int mv_1;
int mv_2;
int mv_3;
int mv_4;
int mv_5;
int mv_6;
int mv_7;
int mv_8;
int mv_9;
int mv_10;
int mv_11;

/* Lock Tests */

/* Create Lock */

/* Default Test creates a lock that will later be deleted */

void create_Lock_Test() {
  Write("\n\nTesting CreateLock. Calling CreateLock('a', 1)\n", sizeof("\n\nTesting CreateLock. Calling CreateLock('a', 1)\n"), ConsoleOutput);
  lock_1 = CreateLock("a", 1);
  Write("Lock syscall successful.\n", sizeof("Lock syscall successful.\n"), ConsoleOutput);
}

/* Create Lots of Locks Test which will be used for various other tests */
void create_Many_Locks_Test() {
  Write("Testing Many Lock syscalls. Calling CreateLock() 9 times\n", sizeof("Testing Many Lock syscalls. Calling CreateLock() 9 times\n"), ConsoleOutput);
  lock_2 = CreateLock("b", 1);
  lock_3 = CreateLock("c", 1);
  lock_4 = CreateLock("d", 1);
  lock_5 = CreateLock("e", 1);
  lock_6 = CreateLock("f", 1);
  lock_7 = CreateLock("g", 1);
  lock_8 = CreateLock("h", 1);
  lock_9 = CreateLock("i", 1);
  lock_10 = CreateLock("j", 1);
  Write("Many locks created finished!\n", sizeof("Many locks created finished!\n"),ConsoleOutput);
}

/* Create Lock with the same name */
void create_Same_Lock() {
  Write("Testing making a lock with the same name.\n", sizeof("Testing making a lock with the same name.\n"), ConsoleOutput);
  lock_11 = CreateLock("a", 1);
  if(lock_11 == lock_1) {
    Write("SUCCESS the indices match.\n", sizeof("SUCCESS the indices match.\n"), ConsoleOutput);
  }
}

/* Default test destroys the create lock in the default create test */
void destroy_Lock_Test() {
  Write("Testing Destroy Lock. Calling DestroyLock()\n", sizeof("Testing Destroy Lock. Calling DestroyLock()\n"), ConsoleOutput);
  DestroyLock(lock_1);
}

/* Checks to make sure that a bad index will be blocked from reaching the system call */
void destroy_Lock_Bad_Index_Test() {
  Write("Testing Destroy Lock syscall with a negative index.\n", sizeof("Testing Destroy Lock syscall with a negative index.\n"), ConsoleOutput);
  DestroyLock(-1);
  Write("Testing Destroy Lock sys call with a lock index of 12 (Larger than next index)\n", sizeof("Testing Destroy Lock sys call with a lock index of 12 (Larger than next index)\n"), ConsoleOutput);
  DestroyLock(12);
}

/* Checks to make sure a already deleted lock deletion will be blocked from reaching the system call */
void destroy_Lock_Already_Deleted(){
  Write("Testing Destroy Lock on already destroyed lock\n", sizeof("Testing Destroy Lock on already destroyed lock\n"), ConsoleOutput);
  DestroyLock(lock_1);
}

/* Checks to make sure an in use lock can't be deleted yet */
void destroy_Lock_Thread_Using_Lock(){
  Write("Destroying Lock 10!!!\n", sizeof("Destroying Lock 10!!!\n"), ConsoleOutput);
  DestroyLock(lock_10);
}

/* Setup for previous test */
void destroy_Lock_Using_Lock_Setup(){
  Write("Acquiring unused lock_10 to check destroy_Lock\n", sizeof("Acquiring unused lock_10 to check destroy_Lock\n"), ConsoleOutput);
  Acquire(lock_10);
}

/* MVs */
/* Default test to create a mv to later be destroyed */
void create_MV_Test() {
  Write("Testing Create MV. Calling CreateMV('a',1,5)\n", sizeof("Testing Create MV. Calling CreateMV('a',1,5)\n"), ConsoleOutput);
  mv_1 = CreateMV("a",1,5);
}

void create_Many_MVs_Test() {
  Write("Testing Create MV 9 more times\n", sizeof("Testing Create MV 9 more times\n"), ConsoleOutput);
  mv_2 = CreateMV("b",1, 5);
  mv_3 = CreateMV("c",1,10);
  mv_4 = CreateMV("d",1,5);
  mv_5 = CreateMV("e",1, 125);
  mv_6 = CreateMV("f",1, 12);
  mv_7 = CreateMV("g",1,23);
  mv_8 = CreateMV("h",1,1);
  mv_9 = CreateMV("i",1,1);
  mv_10 = CreateMV("j",1,1);
}

void create_Same_MV() {
  Write("Testing Create MV with the same name.\n", sizeof("Testing Create MV with the same name.\n"), ConsoleOutput);
  mv_11 = CreateMV("a",1,5);
  if(mv_1 == mv_11){
    Write("SUCCESS the indices match\n", sizeof("SUCCESS the indices match\n"), ConsoleOutput);
  }
}

void set_MV_Test() {
  SetMV(mv_1, 4, 1);
}

void get_MV_Test() {
  int temp;
  temp = GetMV(mv_1, 1);
  if(temp == 4) {
    Write("SUCCESS.\n", sizeof("SUCCESS.\n"), ConsoleOutput);
  }
}

void get_Bad_Index_Test() {
  int temp;
  Write("Testing getting an MV with a bad index.\n", sizeof("Testing getting an MV with a bad index.\n"), ConsoleOutput);
  temp = GetMV(-1, 1);
}



/*  CV's */
/* Default test to create a cv to later be destroyed */
void create_CV_Test(){
  Write("Testing Create CV. Calling CreateCV()\n", sizeof("Testing Create CV. Calling CreateCV()\n"), ConsoleOutput);
  cv_1 = CreateCV("a", 1);
}

/* Create a lot of cv's to be used in later tests */
void create_Many_CVs_Test(){
  Write("Testing Create CV 9 more times\n", sizeof("Testing Create CV 9 more times\n"), ConsoleOutput);
  cv_2 = CreateCV("b", 1);
  cv_3 = CreateCV("c", 1);
  cv_4 = CreateCV("d", 1);
  cv_5 = CreateCV("e", 1);
  cv_6 = CreateCV("f", 1);
  cv_7 = CreateCV("g", 1);
  cv_8 = CreateCV("h", 1);
  cv_9 = CreateCV("i", 1);
  cv_10 = CreateCV("j", 1);
}

/* Create a CV with the same name */
void cv_Same_Name() {
  cv_11 = CreateCV("a", 1);  
  if(cv_1 == cv_11) {
    Write("SUCCESS the indices are the same.\n", sizeof("SUCCESS the indices are the same.\n"), ConsoleOutput);
  }
}

/* Destroy the cv created in the default test */
void destroy_CV_Test(){
  Write("Testing Destroy CV. Destroying cv_1\n", sizeof("Testing Destroy CV. Destroying cv_1\n"), ConsoleOutput);
  DestroyCV(cv_1);
}

/* Test to make sure a bad index will be blocked from reaching the system call */
void destroy_CV_Bad_Index_Test(){
  Write("Testing Destroy CV on a negative index\n", sizeof("Testing Destroy CV on a negative index\n"), ConsoleOutput);
  DestroyCV(-1);
  Write("Testing Destroy CV on index of 12 (Larger than next index)\n", sizeof("Testing Destroy CV on index of 12 (Larger than next index)\n"), ConsoleOutput);
  DestroyCV(12);
  Write("Testing Destroy CV on index 11\n", sizeof("Testing Destroy CV on index 11\n"), ConsoleOutput);
  DestroyCV(11);
}

/* Test to make sure a already deleted cv will be blocked from reaching the system call  */
void destroy_CV_Already_Deleted(){
  Write("Testing Destroy CV on already deleted CV, cv_1\n", sizeof("Testing Destroy CV on already deleted CV, cv_1\n"), ConsoleOutput);
  DestroyCV(cv_1);
}
	
/* Setup for next test */
void destroy_CV_Setup(){
  Write("Acquiring lock_9 and waiting on cv_9\n", sizeof("Acquiring lock_9 and waiting on cv_9\n"), ConsoleOutput);
  Acquire(lock_9);
  Wait(cv_9, lock_9);
  Exit(0);
}

/* Test to make sure that a active cv will not be deleted, but marked for deletion */
destroy_CV_Thread_Using_CV(){
  Write("Testing Destroy CV on waiting cv_9\n", sizeof("Testing Destroy CV on waiting cv_9\n"), ConsoleOutput);
  DestroyCV(cv_9);
}

/* Acquire */
/* Default acquire test acquiring lock_2  */
void acquire_Lock_Test(){
  Write("Testing Acquire on lock_2\n", sizeof("Testing Acquire on lock_2\n"), ConsoleOutput);
  Acquire(lock_2);
}

/* Test to make sure that bad index will be blocked before the syscall */
void acquire_Lock_Bad_Index(){
  Write("Testing Acquire on negative index\n", sizeof("Testing Acquire on negative index\n"), ConsoleOutput);
  Acquire(-1);
  Write("Testing Acquire on out of bounds index\n", sizeof("Testing Acquire on out of bounds index\n"), ConsoleOutput);
  Acquire(12);
}
	
/* Test to make sure that already deleted lock cannot be acquired */
void acquire_Lock_Already_Deleted(){
	Write("Testing Acquire on on deleted lock, lock_1\n", sizeof("Testing Acquire on on deleted lock, lock_1\n"), ConsoleOutput);
	Acquire(lock_1);
}
	
/*-------------------------- Release */
/* Default test releasing the lock in a previous test */
void release_Lock_Test(){
	Write("Testing Release on lock_2\n", sizeof("Testing Release on lock_2\n"), ConsoleOutput);
	Release(lock_2);
}

/* Release a lock that does not have the lock */
void release_Lock_Without_Acquire(){
	Write("Testing Release on lock_2 again\n", sizeof("Testing Relese on lock_2 again\n"), ConsoleOutput);
	Release(lock_2);
}
	
/* Test to check that bad index will be blocked before syscall */
void release_Lock_Bad_Index(){
	Write("Testing Release on negative index\n", sizeof("Testing Release on negative index\n"), ConsoleOutput);
	Release(-1);
	Write("Testing Release on out of bounds index\n", sizeof("Testing Release on out of bounds index\n"), ConsoleOutput);
	Release(12);
}
	
/* Test to check that release won't be called on an already deleted lock */
void release_Lock_Already_Deleted(){
	Write("Testing Release on deleted lock, lock_1\n", sizeof("Testing Release on deleted lock, lock_1\n"), ConsoleOutput);
	Release(lock_1);
}

	
/*-------------------------- Wait */
/* Default test to make sure wait syscall works and will be used for later test */
void wait_Test(){
	Write("Testing Wait, Nachos should not end\n", sizeof("Testing Wait, Nachos should end now\n"), ConsoleOutput);
	
	Wait(cv_2, lock_2);
	Release(lock_2);
	Write("wait_Test has woke up\n", sizeof("   wait_Test has woke up\n"), ConsoleOutput);
	Exit(0);	
}

/* Test to make sure bad index will be blocked before it reaches syscall */
void wait_Bad_Index(){
	Write("Testing Wait on a negative cv index\n", sizeof("Testing Wait on a negative cv index\n"), ConsoleOutput);
	Wait(-1, lock_2);
	Write("Testing Wait on a out of bounds cv index\n", sizeof("Testing Wait on a out of bounds cv index\n"), ConsoleOutput);
	Wait(12, lock_2);
	Write("Testing Wait on a negative lock index\n", sizeof("Testing Wait on a negative lock index\n"), ConsoleOutput);
	Wait(cv_2, -1);
	Write("Testing Wait on a out of bounds lock index\n", sizeof("Testing Wait on a out of bounds lock index\n"), ConsoleOutput);
	Wait(cv_2, 12);
}
	
/* Test to make sure already deleted lock or cv will be blocked before it reaches syscall */
void wait_Already_Deleted(){
	Write("Testing Wait on a deleted cv\n", sizeof("Testing Wait on a deleted cv\n"), ConsoleOutput);
	Wait(cv_1, lock_2);
	Write("Testing Wait on a deleted lock\n", sizeof("Testing Wait on a deleted lock\n"), ConsoleOutput);
	Wait(cv_2, lock_1);
}

/*-------------------------- Signal */
/* Default test to check if signal syscall works */
void signal_Test(){
	Write("Testing Signal on cv_2\n", sizeof("Testing Signal on cv_2\n"), ConsoleOutput);
	Acquire(lock_2);
	Signal(cv_2, lock_2);
	Release(lock_2);
}

/* Test to check if bad index will be blocked before it reaches syscall */
void signal_Bad_Index(){
	Write("Testing Signal on negative cv index\n", sizeof("Testing Signal on negative cv index\n"), ConsoleOutput);
	Signal(-1, lock_2);
	Write("Testing Signal on a out of bounds cv index\n", sizeof("Testing Signal on a out of bounds cv index\n"), ConsoleOutput);
	Signal(12, lock_2);
	Write("Testing Signal on a out of bounds lock index\n", sizeof("Testing Signal on a out of bounds lock index\n"), ConsoleOutput);
	Signal(cv_2, 12);
	Write("Testing Signal on a negative lock index\n", sizeof("Testing Signal on a negative lock index\n"), ConsoleOutput);
	Signal(cv_2, -1);
}
	
/* Test to check that a already deleted cv signal call will be blocked before it reaches syscall */
void signal_Already_Deleted(){
	Write("Testing Signal on deleted cv\n", sizeof("Testing Signal on deleted cv\n"), ConsoleOutput);
	Signal(cv_1, lock_2);
	Write("Testing Signal on deleted lock\n", sizeof("Testing Signal on deleted lock\n"), ConsoleOutput);
	Signal(cv_2, lock_1);
}
	


/*-------------------------- Broadcast */
/* Setup to test deafult broadcast case */
void broadcast_Setup(){
	Write("Successfully forked Broadcast Setup, Acquiring lock_4\n", sizeof("Successfully forked Broadcast Setup, Acquiring lock_3\n"), ConsoleOutput);
	Acquire(lock_4);
	Wait(cv_4, lock_4);
		Release(lock_4);
Write("Exited wait thru Broadcast\n", sizeof("Exited wait thru Broadcast\n"), ConsoleOutput);	
		Exit(0);
}
	
	/* More setup */
void broadcast_Setup2(){
	Write("Successfully forked Broadcast Setup 2, Acquiring lock_4\n", sizeof("Successfully forked Broadcast Setup 2, Acquiring lock_4\n"), ConsoleOutput);
	Acquire(lock_4);
		Wait(cv_4, lock_4);
		Release(lock_4);
		Write("Exited wait thru Broadcast 2\n", sizeof("Exited wait thru Broadcast 2\n"), ConsoleOutput);
		Exit(0);
}
	
	/* Default test to make sure broadcast works */
void broadcast_Test(){			
		Write("Testing Broadcast on cv_4\n", sizeof("Testing Broadcast on cv_3\n"), ConsoleOutput);
		/*Fork((void*)broadcast_Setup);
		Fork((void*)broadcast_Setup2);*/
		Yield();
		Yield();
		Acquire(lock_4);
		Broadcast(cv_4, lock_4);
		Release(lock_4);
		Write("Broadcasted on cv_4\n", sizeof("Broadcasted on cv_4\n"), ConsoleOutput);
} 

	/* Test to make sure a bad index will be blocked before it enters syscall */
void broadcast_Bad_Index(){
		Write("Testing Broadcast on negative cv index\n", sizeof("Testing Broadcast on negative cv index\n"), ConsoleOutput);
		Broadcast(-1, lock_4);
		Write("Testing Broadcast on out of bounds cv index\n", sizeof("Testing Broadcast on out of bounds cv index\n"), ConsoleOutput);
		Broadcast(12, lock_4);
		Write("Testing Broadcast on negative lock index\n", sizeof("Testing Broadcast on negative lock index\n"), ConsoleOutput);
		Broadcast(cv_4, -1);
		Write("Testing Broadcast on out of bounds lock index\n", sizeof("Testing Broadcast on out of bounds lock index\n"), ConsoleOutput);
		Broadcast(cv_4, 12);
	} 

	/* Test to make sure an already deleted cv or lock will be blocked before it enters syscall */
	void broadcast_Already_Deleted(){
		Write("Testing Broadcast on deleted cv\n", sizeof("Testing Broadcast on deleted cv\n"), ConsoleOutput);
		Broadcast(cv_1, lock_4);
		Write("Testing Broadcast on deleted lock\n", sizeof("Testing Broadcast on deleted lock\n"), ConsoleOutput);
		Broadcast(cv_4, lock_1);
	} 

int main() {
 
  OpenFileId fd;
  int bytesread;
  char buf[20];
	int myid;
	int numTimes;
	int mvIndex;
	int lockIndex;
	int cvIndex;
	myid = -1;
	Print("Starting Tests\n", -1, -1, -1);
	/*Identify client */
	myid = Identify();
	Print("My super awesome id is %d\n", myid, -1, -1);
	
	/* Creation for main tests */
	lockIndex = CreateLock("MainLock", 8);
	cvIndex = CreateCV("MainCV", 6);
	mvIndex = CreateMV("MainMV", 6, 5);

  if(myid == 3){/* Currently will never enter this *//* Lock Tests */
  Write("\n*** LOCK TEST ***\n", sizeof("\n*** LOCK TEST ***\n"), ConsoleOutput);
  create_Lock_Test();

  create_Many_Locks_Test();

  create_Same_Lock();

  destroy_Lock_Test();
  
  destroy_Lock_Bad_Index_Test();

  destroy_Lock_Already_Deleted();

  destroy_Lock_Using_Lock_Setup();
	
  destroy_Lock_Thread_Using_Lock();

  /* MV Tests */
  Write("\n*** MV TEST ***\n", sizeof("\n*** MV TEST ***\n"), ConsoleOutput);
  create_MV_Test();
  
  create_Many_MVs_Test();
  
  create_Same_MV();

  set_MV_Test();
  
  get_MV_Test();

  get_Bad_Index_Test();

  /* CV Tests */
  Write("\n*** CV TEST ***\n", sizeof("\n*** CV TEST ***\n"), ConsoleOutput);
  create_CV_Test();

  create_Many_CVs_Test();

  cv_Same_Name();

  destroy_CV_Test();
  
  destroy_CV_Bad_Index_Test();

  destroy_CV_Already_Deleted();

  /* Acquire/Release Tests */
  Write("\n*** ACQUIRE TEST ***\n", sizeof("\n*** ACQUIRE TEST ***\n"), ConsoleOutput);
  acquire_Lock_Test();

  acquire_Lock_Bad_Index();
	
  acquire_Lock_Already_Deleted();
	
  Write("\n*** RELEASE TEST ***\n", sizeof("\n*** RELEASE TEST ***\n"), ConsoleOutput);
  release_Lock_Test();
	
  release_Lock_Without_Acquire();   
	
  release_Lock_Bad_Index();
	
  release_Lock_Already_Deleted();
	

  /* CV Operation Tests */
  /* Wait */
	}
	/*Write("\n*** WAIT TEST ***\n", sizeof("\n*** WAIT TEST ***\n"), ConsoleOutput);*/
	
	Acquire(lockIndex);
	Print("%i: Acquired the lock\n", myid, -1, -1);
	numTimes = GetMV(mvIndex, 1);
	Print("NumTimes: %i, mvIndex: %i\n", numTimes, mvIndex, -1);
	if(numTimes == 1){ /* Both clients have now reached this point */
		Print("Last client inside, about to broadcast\n", -1, -1, -1);
  	Broadcast(lockIndex, cvIndex);
		SetMV(mvIndex, 0, 1);
	}
	else{
		SetMV(mvIndex, numTimes + 1, 1);
		numTimes = GetMV(mvIndex, 1);
		Print("numTimes to %i\n", numTimes, -1, -1);
		Wait(lockIndex, cvIndex);
	}

	Release(lockIndex);
	Print("\n\n*****\n%i: Got outside wait cycle\n*****\n\n", myid, -1, -1);
	
  if(myid == 3){ /* Not being used yet */
	Yield();

  wait_Bad_Index();	

  wait_Already_Deleted();

  /* Signal */
  Write("\n*** SIGNAL TEST ***\n", sizeof("\n*** SIGNAL TEST ***\n"), ConsoleOutput);	

  signal_Test(); /* Signal thread waiting from wait_Test */

  signal_Bad_Index();

  signal_Already_Deleted();  
	
  /* Broadcast */
  Write("\n*** BROADCAST TEST ***\n", sizeof("\n*** BROADCAST TEST ***\n"), ConsoleOutput);
	
  broadcast_Test();		

  broadcast_Bad_Index();

  broadcast_Already_Deleted();
	
  /*Yield();*/
	
  Write("\n\n\n*** END OF TESTING BEFORE EXIT ***\n\n", sizeof("\n\n\n*** END OF TESTING BEFORE EXIT ***\n"), ConsoleOutput); 
  }
  Exit(0);
}


/*main() {

  Print("TESTING CREATE CV NORMATIVE.\n", -1, -1, -1);
  cvIndex1 = CreateCV("merp", sizeof("merp"));

  Print("TESTING ACQUIRE NORMATIVE.\n", -1, -1, -1);
  Acquire(index2);
  Print("TESTING ACQUIRE: calling acquire when the owner has already acquired.\n", -1, -1, -1);
  Acquire(index3);
  Print("TESTING ACQUIRE: when given a bad index.\n", -1, -1, -1);
  Acquire(325);
  
  Print("TESTING RELEASE NORMATIVE.\n", -1, -1, -1);
  Release(index2);

  Print("TESTING DESTROY LOCK NORMATIVE.\n", -1, -1, -1);
  DestroyLock(index);
  DestroyLock(index2);
  DestroyLock(index3);

  Print("TESTING CREATE MV NORMATIVE.\n", -1, -1, -1);
  index4 = CreateMV("c", sizeof("c"));

  Print("TESTING SET MV NORMATIVE.\n", -1, -1, -1);
  SetMV(index4, 2857);

  Print("TESTING GET MV NORMATIVE.\n", -1, -1, -1);
  GetMV(index4);

  Print("TESTING DESTROYMV NORMATIVE.\n", -1, -1, -1);
  DestroyMV(index4);

  return 0;
} */
