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
  Fork((void *)forkThread1);
  Fork((void *)forkThread2);
	Write("Exec finished forking threads, now trying to create a lock\n", sizeof("Exec finished forking threads, now trying to create a lock\n"), ConsoleOutput);
	lock_1 = CreateLock();
	Write("Exec created a lock!\n", sizeof("Exec created a lock!\n"), ConsoleOutput);
  Exit(0);
}
