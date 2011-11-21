#include "syscall.h"

int main()
{
	Exec("../test/terminatetest",21);
	Yield();
	Exec("../test/terminatetest",21);
	Yield();
/*	Exec("../test/networktests",20);
	Yield();
	Exec("../test/networktests",20);
	Yield();
	Exec("../test/networktests",20);*/
				

	Exit(0);
}
