/* testfiles.c
 *	Simple program to test the file handling system calls
 */

#include "syscall.h"

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


/*Yield Test */
	yield_Test(){
		Write("\n\nTesting Yield syscall. Calling Yield()\n", sizeof("\n\nTesting Yield syscall. Calling Yield()\n"), ConsoleOutput);
		Yield();
		Write("Yield syscall successful. End of Yield test\n\n", sizeof("Yield syscall successful. End of Yield test\n\n"), ConsoleOutput);
	}

/* Lock Tests */
/* Create Lock */

/* Default Test */
	create_Lock_Test(){
		Write("Testing Lock syscall. Calling CreateLock()\n", sizeof("Testing Lock syscall. Calling CreateLock()\n"), ConsoleOutput);
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
	



int main() {
	OpenFileId fd;
  int bytesread;
  char buf[20];
	/* Yield Test */
	yield_Test();

	/* Lock Tests */
	create_Lock_Test();
  create_Many_Locks_Test();

    Create("testfile", 8);
    fd = Open("testfile", 8);

    Write("testing a write\n", 16, fd );
    Close(fd);


    fd = Open("testfile", 8);
    bytesread = Read( buf, 100, fd );
    Write( buf, bytesread, ConsoleOutput );
    Close(fd);
}
