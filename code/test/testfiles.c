/* testfiles.c
 *	Simple program to test the file handling system calls
 */

#include "syscall.h"

/* Globals */

int lock_1;
int lock_2;
int lock_3;


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
	}

/* Fork */
  void forkThread1(){
    Write("Forked test thread1\n", 20, ConsoleOutput);
    Exit(0);
  }
  
  void forkThread2() {
    Write("Forked test thread2\n", 20, ConsoleOutput);
    Exit(0);
  }

  fork_Test() {
    Write("\n\nTesting Fork syscall. Calling Fork()\n", sizeof("\n\nTesting Fork syscall. Calling Fork()\n"), ConsoleOutput);

    Fork((void *)forkThread1);
    Fork((void *)forkThread2);
  }
  
/* Exec */
  exec_Test() {
    Write("\n\nTesting Exec syscall. Calling Exec()\n", sizeof("\n\nTesting Exec syscall. Calling Exec()\n"), ConsoleOutput);
    /*TODO: some shit to test fork. figure out mother fucker*/
  }
  

int main() {
	OpenFileId fd;
  int bytesread;
  char buf[20];
	/* Yield Test */
	yield_Test();

	/* Lock Tests */
	create_Lock_Test();
  
  /* Forking bitches */
  fork_Test();
  
  /* Executing all kinds of threads */
  exec_Test();
  
    Create("testfile", 8);
    fd = Open("testfile", 8);

    Write("Testing a Write syscall.\n", 25, fd );
    Close(fd);


    fd = Open("testfile", 8);
    Write("Testing a Read syscall.\n", 24, fd);
    bytesread = Read( buf, 100, fd );
    Write( buf, bytesread, ConsoleOutput );
    Close(fd);
}
