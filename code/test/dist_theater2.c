/* dist_theater2.c
 * Second of several launch files for running the distributed movie theater
 */
 
#include "syscall.h"
#include "creates.h"

int main() {
  
  Exec("../test/the_customer", sizeof("../test/the_customer"));
  Exec("../test/the_customer", sizeof("../test/the_customer"));
  Exec("../test/the_customer", sizeof("../test/the_customer"));
  Exec("../test/the_customer", sizeof("../test/the_customer"));
  Exec("../test/the_customer", sizeof("../test/the_customer"));
  Exec("../test/the_customer", sizeof("../test/the_customer"));
  Exec("../test/the_customer", sizeof("../test/the_customer"));
  Exec("../test/the_customer", sizeof("../test/the_customer"));
  
  
/*  Exec("../test/the_manager", sizeof("../test/the_manager"));*/
/*  Exec("../test/the_customer", sizeof("../test/the_customer"));*/
/*  Exec("../test/the_customer", sizeof("../test/the_customer"));*/
  
  Exit(0);
}
