/* execTest.c
 *	Quick and easy program to test the Exec syscall
 *  - Enters the new process, forks two test threads and tests a Write
 */

#include "syscall.h"

int cv_1;

int lock_1;

/* Test fork threads */
void forkThread1(){
  Write("Forked test thread1 in exec\n", 28, ConsoleOutput);
  Exit(0);
}

void forkThread2() {
  Write("Forked test thread2 in exec\n", 28, ConsoleOutput);
  Exit(0);
}

/* Starts executing here */
int main() {
	Write("Running test of Exec syscall.\n", 30, ConsoleOutput);
  Fork((void *)forkThread1, 0);
  Fork((void *)forkThread2, 0);
	Write("Exec finished forking threads, now trying to create a lock\n", sizeof("Exec finished forking threads, now trying to create a lock\n"), ConsoleOutput);
	lock_1 = CreateLock();
	Print("lock_1 position created at %i\n", lock_1, -1, -1);
	Write("Exec created a lock!\n", sizeof("Exec created a lock!\n"), ConsoleOutput);
	
	/* Testing to make sure none of the syscalls can be called from the wrong process after being created by the 1st process */
	/* Lock */
	Print("Trying to destroy lock 5 from wrong process\n", -1, -1, -1);
	DestroyLock(5);
	/* CV */
	Print("Trying to destroy cv 5 from wrong process\n", -1, -1, -1);
	DestroyCV(5);
	/* Acquire */
	Print("Trying to acquire lock 6 from wrong process\n", -1, -1, -1);
	Acquire(6);
	/* Release */
	Print("Trying to release lock 6 from wrong process\n", -1, -1, -1);
	Release(6);
	/* Wait */
	Print("Trying to wait on cv 6 from wrong process\n", -1, -1, -1);
	Wait(6, 6);
	/* Signal */
	Print("Trying to signal on cv 6 from wrong process\n", -1, -1, -1);
	Signal(6, 6);
	/* Broadcast */
	Print("Trying to broadcast on cv 6 from wrong process\n", -1, -1, -1);
	Broadcast(6, 6);
	/* Leave new process after tests */
	Exit(0);
}
