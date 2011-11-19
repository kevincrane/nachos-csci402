/*This tests multiple instances of the networktest program for program run */
#include "syscall.h"

int main()
{
	Exec("../test/networktests",20);
	Yield();
	Exec("../test/networktests",20);
	Yield();
/*	Exec("../test/networktests",20);
	Yield();
	Exec("../test/networktests",20);
	Yield();
	Exec("../test/networktests",20);*/
				

	Exit(0);
}
