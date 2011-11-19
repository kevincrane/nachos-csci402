/* dist_theater1.c
 * First of several launch files for running the distributed movie theater
 */
 
#include "syscall.h"
#include "creates.h"

int main() {
 /* initTheater();*/   /* Define all global values for theater */
  
  /* TODO: Manually tweak values like group head, customer numbers, nextGroup, etc to force this bitch to work */
  
  /* After manually tweaking values, just blitz through 10 customers or 4 groups or something */
  /* Up to 10 entities in this bitch */
  
  Exec("../test/the_customer", sizeof("../test/the_customer"));
  Exec("../test/the_customer", sizeof("../test/the_customer"));
  Exec("../test/the_customer", sizeof("../test/the_customer"));
  Exec("../test/the_customer", sizeof("../test/the_customer"));
  Exec("../test/the_customer", sizeof("../test/the_customer"));
  Exec("../test/the_customer", sizeof("../test/the_customer"));
  Exec("../test/the_customer", sizeof("../test/the_customer"));
  Exec("../test/the_customer", sizeof("../test/the_customer"));
/*  Exec("../test/the_customer", sizeof("../test/the_customer"));*/
/*  Exec("../test/the_customer", sizeof("../test/the_customer"));*/
  
  Exit(0);
}
