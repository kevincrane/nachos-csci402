/*This tests multiple instances of the networktest program for program run */
#include "syscall.h"

int main()
{
	Exec("../test/networktests",20);
	Exec("../test/networktests",20);

	Exit(0);
}
