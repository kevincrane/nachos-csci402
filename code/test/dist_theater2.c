/* dist_theater2.c
 * Second of several launch files for running the distributed movie theater
 */
 
#include "syscall.h"
#include "creates.h"

int main() {
 /* initTheater();*/   /* Define all global values for theater */
  
  Exec("../test/the_ticketclerk", sizeof("../test/the_ticketclerk"));
  Exec("../test/the_ticketclerk", sizeof("../test/the_ticketclerk"));
  Exec("../test/the_ticketclerk", sizeof("../test/the_ticketclerk"));
  Exec("../test/the_ticketclerk", sizeof("../test/the_ticketclerk"));
  Exec("../test/the_ticketclerk", sizeof("../test/the_ticketclerk"));
  
  Exec("../test/the_tickettaker", sizeof("../test/the_tickettaker"));
  Exec("../test/the_tickettaker", sizeof("../test/the_tickettaker"));
  Exec("../test/the_tickettaker", sizeof("../test/the_tickettaker"));
  
  Exec("../test/the_movietech", sizeof("../test/the_movietech"));
  
  Exit(0);
}
