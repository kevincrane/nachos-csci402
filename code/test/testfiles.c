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
	create_Lock_Test(){
		lock_1 =  CreateLock();
	}



int main() {
	OpenFileId fd;
  int bytesread;
  char buf[20];
	/* Yield Test */
	yield_Test();

	/* Lock Tests */
	create_Lock_Test();
  

    Create("testfile", 8);
    fd = Open("testfile", 8);

    Write("testing a write\n", 16, fd );
    Close(fd);


    fd = Open("testfile", 8);
    bytesread = Read( buf, 100, fd );
    Write( buf, bytesread, ConsoleOutput );
    Close(fd);
}
