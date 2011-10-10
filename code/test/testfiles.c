/* testfiles.c
 *	Simple program to test the file handling system calls
 */

#include "syscall.h"
/* TESTS TO MAKE 
	*Yield
	*CreateLock
	*DestroyLock
	*CreateCV
	*DestroyCV
	*AcquireLock
	*ReleaseLock
	*Wait
	*Signal
	*Broadcast
	*Fork
	Exec
	*Exit
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



/*Yield Test */
	yield_Test(){
		Write("\n\nTesting Yield syscall. Calling Yield()\n", sizeof("\n\nTesting Yield syscall. Calling Yield()\n"), ConsoleOutput);
		Yield();
		Write("Yield syscall successful. End of Yield test\n\n", sizeof("Yield syscall successful. End of Yield test\n\n"), ConsoleOutput);
	}

/* Lock Tests */
/* Create Lock */

/* Default Test */

	create_Lock_Test() {
	  Write("\n\nTesting CreateLock syscall. Calling CreateLock()\n", sizeof("\n\nTesting CreateLock syscall. Calling CreateLock()\n"), ConsoleOutput);
		lock_1 =  CreateLock();
		Write("Lock syscall successful.\n", sizeof("Lock syscall successful.\n"), ConsoleOutput);
	}


/* Create Lots of Locks Test */
	create_Many_Locks_Test(){
		Write("Testing Many Lock syscalls. Calling CreateLock() 9 times\n", sizeof("Testing Many Lock syscalls. Calling CreateLock() 10 times\n"), ConsoleOutput);
		lock_2 = CreateLock();
		lock_3 = CreateLock();
		lock_4 = CreateLock();
		lock_5 = CreateLock();
		lock_6 = CreateLock();
		lock_7 = CreateLock();
		lock_8 = CreateLock();
		lock_9 = CreateLock();
		lock_10 = CreateLock();
		Write("Many locks created finished!\n", sizeof("Many locks created finished!\n"),ConsoleOutput);
	}
	
	/*Destroy Lock Tests*/
	void destroy_Lock_Test(){
		Write("Testing Destroy Lock syscall. Calling DestroyLock()\n", sizeof("Testing Destroy Lock syscall. Calling DestroyLock()\n"), ConsoleOutput);
		DestroyLock(lock_1);
	}

	void destroy_Lock_Bad_Index_Test(){
		Write("Testing Destroy Lock syscall with a negative index.\n", sizeof("Testing Destroy Lock syscall with a negative index.\n"), ConsoleOutput);
		DestroyLock(-1);
		Write("Testing Destroy Lock sys call with a lock index of 12 (Larger than next index)\n", sizeof("Testing Destroy Lock sys call with a lock index of 12 (Larger than next index)\n"), ConsoleOutput);
		DestroyLock(12);
	}

	void destroy_Lock_Wrong_Process(){	/* TODO Not sure how to create separate space */

	}

	void destroy_Lock_Already_Deleted(){
		Write("Testing Destroy Lock on already destroyed lock\n", sizeof("Testing Destroy Lock on already destroyed lock\n"), ConsoleOutput);
		DestroyLock(lock_1);
	}

	/*void destroy_Lock_To_Be_Deleted(){			Not sure how to test since I can't access array
		Write("Testing Destroy Lock on a lock to be deleted\n", sizeof("Testing Destroy Lock on a lock to be deleted\n"), ConsoleOutput);
		locks[lock_2].toBeDeleted = true;
		DestroyLock(lock_2);
	}*/

/* CV's */
	void create_CV_Test(){
		Write("END OF LOCK TESTS\n\n", sizeof("END OF LOCK TESTS\n\n"), ConsoleOutput);
		Write("Testing Create CV. Calling CreateCV()\n", sizeof("Testing Create CV. Calling CreateCV()\n"), ConsoleOutput);
		cv_1 = CreateCV();
	}

  void create_Many_CVs_Test(){
		Write("Testing Create CV 9 more times\n", sizeof("Testing Create CV 9 more times\n"), ConsoleOutput);
		cv_2 = CreateCV();
		cv_3 = CreateCV();
		cv_4 = CreateCV();
		cv_5 = CreateCV();
		cv_6 = CreateCV();
		cv_7 = CreateCV();
		cv_8 = CreateCV();
		cv_9 = CreateCV();
		cv_10 = CreateCV();
	}

	void destroy_CV_Test(){
		Write("Testing Destroy CV. Destroying cv_1\n", sizeof("Testing Destroy CV. Destroying cv_1\n"), ConsoleOutput);
		DestroyCV(cv_1);
	}

	void destroy_CV_Bad_Index_Test(){
		Write("Testing Destroy CV on a negative index\n", sizeof("Testing Destroy CV on a negative index\n"), ConsoleOutput);
		DestroyCV(-1);
		Write("Testing Destroy CV on index of 12 (Larger than next index)\n", sizeof("Testing Destroy CV on index of 12 (Larger than next index)\n"), ConsoleOutput);
		DestroyCV(12);
		Write("Testing Destroy CV on index 11\n", sizeof("Testing Destroy CV on index 11\n"), ConsoleOutput);
		DestroyCV(11);
	}

	void destroy_CV_Wrong_Process(){		/* TODO see lock comment above */
	}

	void destroy_CV_Already_Deleted(){
		Write("Testing Destroy CV on already deleted CV, cv_1\n", sizeof("Testing Destroy CV on already deleted CV, cv_1\n"), ConsoleOutput);
		DestroyCV(cv_1);
	}

	/*destroy_CV_To_Be_Deleted();

	destroy_Lock_Thread_Using_CV();*/ 

	/* Acquire */
	void acquire_Lock_Test(){
		Write("Testing Acquire on lock_2\n", sizeof("Testing Acquire on lock_2\n"), ConsoleOutput);
		Acquire(lock_2);
	}

	void acquire_Lock_Bad_Index(){
		Write("Testing Acquire on negative index\n", sizeof("Testing Acquire on negative index\n"), ConsoleOutput);
		Acquire(-1);
		Write("Testing Acquire on out of bounds index\n", sizeof("Testing Acquire on out of bounds index\n"), ConsoleOutput);
		Acquire(12);
	}
	
	void acquire_Lock_Wrong_Process(){}
	
	void acquire_Lock_Already_Deleted(){
		Write("Testing Acquire on on deleted lock, lock_1\n", sizeof("Testing Acquire on on deleted lock, lock_1\n"), ConsoleOutput);
		Acquire(lock_1);
	}
	
	void acquire_Lock_To_Be_Deleted(){}
	
	/* Release */
	void release_Lock_Test(){
		Write("Testing Release on lock_2\n", sizeof("Testing Release on lock_2\n"), ConsoleOutput);
		Release(lock_2);
	}
	
	void release_Lock_Without_Acquire(){
		Write("Testing Release on lock_2 again\n", sizeof("Testing Relese on lock_2 again\n"), ConsoleOutput);
		Release(lock_2);
	}
	
	void release_Lock_Bad_Index(){
		Write("Testing Release on negative index\n", sizeof("Testing Release on negative index\n"), ConsoleOutput);
		Release(-1);
		Write("Testing Release on out of bounds index\n", sizeof("Testing Release on out of bounds index\n"), ConsoleOutput);
		Release(12);
	}
	
	void release_Lock_Already_Deleted(){
		Write("Testing Release on deleted lock, lock_1\n", sizeof("Testing Release on deleted lock, lock_1\n"), ConsoleOutput);
		Release(lock_1);
	}
	



	
/* Wait */
	void wait_Test(){
		Write("Testing Wait, Nachos should end now\n", sizeof("Testing Wait, Nachos should end now\n"), ConsoleOutput);
		Acquire(lock_2);
		Wait(cv_2, lock_2);
		Write("wait_Test has woke up\n", sizeof("wait_Test has woke up\n"), ConsoleOutput);
	}

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
	
	void wait_Already_Deleted(){
		Write("Testing Wait on a deleted cv\n", sizeof("Testing Wait on a deleted cv\n"), ConsoleOutput);
		Wait(cv_1, lock_2);
		Write("Testing Wait on a deleted lock\n", sizeof("Testing Wait on a deleted lock\n"), ConsoleOutput);
		Wait(cv_2, lock_1);
	}
	void wait_Wrong_Process(){}

/* Signal */
	void signal_Test(){
		Write("Testing Signal on cv_2\n", sizeof("Testing Signal on cv_2\n"), ConsoleOutput);
		Acquire(lock_2);
		Signal(cv_2, lock_2);
	}

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
/* Fork */
  void forkThread1(){
    Write("Forked test thread1\n", 20, ConsoleOutput);
  }
  
  void forkThread2() {
    Write("Forked test thread2\n", 20, ConsoleOutput);
    Exit(0);
  }
  
  void forkThread3() {
    Write("Forked test thread3\n", 20, ConsoleOutput);
    Exit(0);
  }

	void forkTest1() {
		Write("Fork Test 1 ready\n", sizeof("Fork Test 1 ready\n"), ConsoleOutput);
		
		Acquire(lock_3);
		Write("Fork Test 1 Acquiring lock_3\n", sizeof("Fork Test 1 Acquiring lock_3\n"), ConsoleOutput);
		Wait(cv_3, lock_3);
		Write("Hi, #1 got out of wait succsssfully!\n", sizeof("Hi, #1 got out of wait succsssfully!\n"), ConsoleOutput);
	}

	void forkTest2() {
		Write("Fork Test 2 ready\n", sizeof("Fork Test 1 ready\n"), ConsoleOutput);
		Acquire(lock_3);
		Write("Fork Test 2 Acquiring lock_3\n", sizeof("Fork Test 1 Acquiring lock_3\n"), ConsoleOutput);
		Signal(cv_3, lock_3);
		Write("Fork Test 1 Signaled lock_3\n", sizeof("Fork Test 1 Signaled lock_3\n"), ConsoleOutput);
	}
	
	fork_2_Threads() {
		Write("Forking 2 threads for testing purposes\n", sizeof("Forking 2 threads for testing purposes\n"), ConsoleOutput);
		Fork((void *)forkTest1);
		Fork((void *)forkTest2);
	}

  fork_Test() {
    Write("\n\nTesting Fork syscall. Calling Fork()\n", sizeof("\n\nTesting Fork syscall. Calling Fork()\n"), ConsoleOutput);

    Fork((void *)forkThread1);
    Fork((void *)forkThread2);
    Fork((void *)forkThread3);
  }
  
/* Exec */
  exec_Test() {
    Write("\n\nTesting Exec syscall. Calling Exec()\n", sizeof("\n\nTesting Exec syscall. Calling Exec()\n"), ConsoleOutput);
    Exec("../test/exectest",sizeof("../test/exectest"));
    Write("\nDONESTICKS\n", 12, ConsoleOutput);
  }
  

int main() {
	OpenFileId fd;
  int bytesread;
  char buf[20];
  
	/* Yield Test */
	Write("\n*** YIELD TEST ***\n", sizeof("\n*** YIELD TEST ***\n"), ConsoleOutput);
	yield_Test();

	/* Lock Tests */
	Write("\n*** LOCK TEST ***\n", sizeof("\n*** LOCK TEST ***\n"), ConsoleOutput);
	create_Lock_Test();

  create_Many_Locks_Test();

	destroy_Lock_Test();

	destroy_Lock_Bad_Index_Test();

	destroy_Lock_Wrong_Process();

	destroy_Lock_Already_Deleted();

	/*destroy_Lock_To_Be_Deleted();

	destroy_Lock_Thread_Using_Lock();*/

	/* CV Tests */
	Write("\n*** CV TEST ***\n", sizeof("\n*** CV TEST ***\n"), ConsoleOutput);
	create_CV_Test();

  create_Many_CVs_Test();

	destroy_CV_Test();

	destroy_CV_Bad_Index_Test();

	destroy_CV_Wrong_Process();

	destroy_CV_Already_Deleted();

	/*destroy_CV_To_Be_Deleted();

	destroy_CV_Thread_Using_CV();*/ 

	
	/* Acquire/Release Tests */
	Write("\n*** ACQUIRE TEST ***\n", sizeof("\n*** ACQUIRE TEST ***\n"), ConsoleOutput);
	acquire_Lock_Test();

	acquire_Lock_Bad_Index();
	/* acquire_Lock_Wrong_Process(); */
	
	acquire_Lock_Already_Deleted();
	
	/* acquire_Lock_To_Be_Deleted(); */
	Write("\n*** RELEASE TEST ***\n", sizeof("\n*** RELEASE TEST ***\n"), ConsoleOutput);
	release_Lock_Test();
	
	release_Lock_Without_Acquire();   
	
	release_Lock_Bad_Index();
	
	release_Lock_Already_Deleted();
	

	

	/* CV Operation Tests */
	Write("\n*** WAIT TEST ***\n", sizeof("\n*** WAIT TEST ***\n"), ConsoleOutput);
	wait_Bad_Index();	

	wait_Already_Deleted();

	/* wait_Wrong_Process() */
	
	Fork((void *)wait_Test);

	Write("\n*** SIGNAL TEST ***\n", sizeof("\n*** SIGNAL TEST ***\n"), ConsoleOutput);
	
	signal_Test(); /* Signal thread waiting from wait_Test */
	
	signal_Bad_Index();


	/*signal_Already_Deleted();  TODO 

	signal_Wrong_Process();
	*/
	Write("\n*** BROADCAST TEST ***\n", sizeof("\n*** BROADCAST TEST ***\n"), ConsoleOutput);
	/*
	broadcast_Test();

	broadcast_Bad_Index();

	broadcast_Already_Deleted();

	broadcast_Wrong_Process();*/
	
  /* Forking bitches */
 	fork_Test();		

  
	fork_2_Threads();
  /* Executing all kinds of threads */
  exec_Test();
  
  Create("testfile", 8);
  fd = Open("testfile", 8);

  Write("Testing a Write syscall.\n", 25, ConsoleOutput );
  Close(fd);

  fd = Open("testfile", 8);
  Write("Testing a Read/Write syscall.\n", 30, fd);
  bytesread = Read( buf, 100, fd );
  Write( buf, bytesread, ConsoleOutput );
  Close(fd);
    
    Exit(0);
}
