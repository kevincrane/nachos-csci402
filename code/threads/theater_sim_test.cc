// All test cases for theater simulation

#include "copyright.h"
#include "system.h"
#ifdef CHANGED
#include "synch.h"

#endif

Semaphore A_Done("A_Done",0);
void TestAManagerOneCash()
{



}





void Theater_Sim_Test()
{
	Thread *t;
	printf("Starting tests.\n");
	
	// Test A
	// Manager test to only read from one cashier at a time
	for(int i = 0; i<30; i++)
	{
		t=new Thread("Manager");
		t->Fork((VoidFunctionPtr)TestAManagerOneCash,0);

		A_Done.P();
	}

	printf("Test C is Complete \n");
}
