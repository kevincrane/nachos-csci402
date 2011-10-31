#include "syscall.h"

/*Starts executing here*/
int main() {
  Write("Theater 1!\n", sizeof("Theater 1!\n"), ConsoleOutput);
  Exec("../test/theater_sim", sizeof("../test/theater_sim"));
  Write("Theater 2!\n", sizeof("Theater 2!\n"), ConsoleOutput);
  Exec("../test/theater_sim", sizeof("../test/theater_sim"));
  
  Exit(0);
}
