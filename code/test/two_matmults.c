#include "syscall.h"

/*Starts executing here*/
int main() {
  Write("Matmult 1!\n", sizeof("Matmult 1!\n"), ConsoleOutput);
  Exec("../test/matmult", sizeof("../test/matmult"));
  Write("Matmult 2!\n", sizeof("Matmult 2!\n"), ConsoleOutput);
  Exec("../test/testfiles", sizeof("../test/testfiles"));
  
  Write("Matmult 3!\n", sizeof("Matmult 2!\n"), ConsoleOutput);
  Exec("../test/matmult", sizeof("../test/matmult"));
  
  Write("Matmult 4!\n", sizeof("Matmult 2!\n"), ConsoleOutput);
  Exec("../test/matmult", sizeof("../test/matmult"));
  
  Write("Matmult 2!\n", sizeof("Matmult 2!\n"), ConsoleOutput);
  Exec("../test/matmult", sizeof("../test/matmult"));
  Write("Matmult 2!\n", sizeof("Matmult 2!\n"), ConsoleOutput);
  Exec("../test/matmult", sizeof("../test/matmult"));
  Write("Matmult 2!\n", sizeof("Matmult 2!\n"), ConsoleOutput);
  Exec("../test/matmult", sizeof("../test/matmult"));
  Write("Matmult 2!\n", sizeof("Matmult 2!\n"), ConsoleOutput);
  Exec("../test/matmult", sizeof("../test/matmult"));
  
  
  Exit(0);
}
