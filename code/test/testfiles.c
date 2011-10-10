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
	ReleaseLock
	Wait
	Signal
	Broadcast
	Fork
	Exec
	Exit
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
	
	void release_Lock_To_Be_Deleted(){}
	
/* Fork */
  void forkThread1(){
    Write("Forked test thread1\n", 20, ConsoleOutput);
    Exit(0);
  }
  
  void forkThread2() {
    Write("Forked test thread2\n", 20, ConsoleOutput);
    Exit(0);
  }
  
  void forkThread3() {
    Write("Forked test thread3\n", 20, ConsoleOutput);
    Exit(0);
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
	yield_Test();

	/* Lock Tests */
	create_Lock_Test();

  create_Many_Locks_Test();

	destroy_Lock_Test();

	destroy_Lock_Bad_Index_Test();

	destroy_Lock_Wrong_Process();

	destroy_Lock_Already_Deleted();

	/*destroy_Lock_To_Be_Deleted();

	destroy_Lock_Thread_Using_Lock();*/

	/* CV Tests */
	create_CV_Test();

  create_Many_CVs_Test();

	destroy_CV_Test();

	destroy_CV_Bad_Index_Test();

	destroy_CV_Wrong_Process();

	destroy_CV_Already_Deleted();

	/*destroy_CV_To_Be_Deleted();

	destroy_Lock_Thread_Using_CV();*/ 

	
	/* Acquire/Release Tests */
	acquire_Lock_Test();

	acquire_Lock_Bad_Index();
	/* acquire_Lock_Wrong_Process(); */
	
	acquire_Lock_Already_Deleted();
	
	/* acquire_Lock_To_Be_Deleted(); */
	
	release_Lock_Test();
	
	/* release_Lock_Without_Acquire();    TODO CURRENTLY SEG FAULTS!!!! - Ryan */
	
	release_Lock_Bad_Index();
	
	release_Lock_Already_Deleted();
	
	/* void release_Lock_To_Be_Deleted() */
	
  /* Forking bitches */
  fork_Test();
  
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
