/* execTest.c
 *	Quick and easy program to test the Exec syscall
 *  - Enters the new process, forks two test threads and tests a Write
 */

#include "syscall.h"

/* Test fork threads */
void forkThread1(){
  Write("Forked test thread1\n", 20, ConsoleOutput);
  Exit(0);
}

void forkThread2() {
  Write("Forked test thread2\n", 20, ConsoleOutput);
  Exit(0);
}

/* Starts executing here */
int main() {
  Fork((void *)forkThread1);
  Write("Running test of Exec syscall.\n", 30, ConsoleOutput);
  Fork((void *)forkThread2);
  Exit(0);
}
