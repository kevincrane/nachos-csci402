// All test cases for theater simulation

#include "copyright.h"
#include "system.h"
#ifdef CHANGED
#include "synch.h"

#endif

// Test A to get manager to get funds in cash registers
Semaphore A_Done("A_Done",0);
void TestAManagerOneCash()
{
	// Add some funds to ticket clerk registers
	ticketClerkRegister[0] = 100;
	ticketClerkRegister[1] = 150;

	// Make manager total equal to 0
	totalRevenue = 0;

	//Pick a register to read from
	int registerToCheck = rand()%2;
	
	// Copied code directly from manager
	/*for(int i = 0; i<MAX_TC; i++)
	{
		ticketClerkLock[i]->Acquire();
		totalRevenue += ticketClerkRegister[i];
		printf("Manager collected [%i] from TicketClerk[%i].\n", ticketClerkRegister[i], i); 
		ticketClerkRegister[i] = 0;

		ticketClerkLock[i]->Release();
	}
	*/
	//Code modified to check proper acquiring
	ticketClerkLock[registerToCheck]->Acquire();
	totalRevenue += ticketClerkRegister[registerToCheck];
	printf("Manager collected [%i] from TicketClerk[%i].\n", ticketClerkRegister[registerToCheck], registerToCheck); 
	ticketClerkRegister[registerToCheck] = 0;

	ticketClerkLock[registerToCheck]->Release();
	
	//Check to see if register values match total revenue
	if(registerToCheck==0)
	{
		if(totalRevenue!=100)
			printf("ERROR: Manager read registers incorrectly.\n");
	}
	else if(registerToCheck==1)
	{
		if(totalRevenue!=150)
			printf("ERROR: Manager read registers incorrectly.\n");
	}
	
	//Check to see if coffers were emptied
	if(ticketClerkRegister[registerToCheck] != 0)
		printf("ERROR: Manager emptied registers incorrectly.\n");
	
	A_Done.V();
}

// Test B to have manager get clerk off break

Semaphore testb("tb", 0);
void TestBManagerOffBreak()
{	
	// Set TicketClerk line to 6 waiting in line
	ticketClerkLineCount[0] = 6;
	testb.P();
	// Set TicketClerk status to on break
	ticketClerkState[0] = 2;
	
	// Set flag to put TicketClerk back to work
	bool ticketClerkWorkNow = false;
	
	
	
	// Have manager signal ticketClerk
	// Copied from code
	DEBUG('p', "Manager: Acquiring ticketClerkLineLock %i to check line length > 5.\n", 0);
	ticketClerkLineLock->Acquire();
	DEBUG('p', "Manager: Got teh locks. \n");
	if(ticketClerkLineCount[0]>5)
	{
		DEBUG('p', "Manager: Found line longer than 5. Flagging ticketClerk%i to come off break.\n", 0);
		ticketClerkWorkNow = true;					//Set status to have clerk work
	}
	ticketClerkLineLock->Release();
	
	if(ticketClerkWorkNow && ticketClerkWorking<MAX_TT)
	{
		DEBUG('p', "Manager: Signalling a ticketClerk to come off break.\n");
		//Get TC off break
		ticketClerkBreakLock->Acquire();
		ticketClerkBreakCV->Signal(ticketClerkBreakLock);
		ticketClerkBreakLock->Release();
	}



}

void TestBClerkOnBreak()
{
	testb.V();
	DEBUG('p', "Got into clerk code\n");
	ticketClerkBreakLock->Acquire();
  ticketClerkBreakCV->Wait(ticketClerkBreakLock);
	
	printf("Test B Clerk got off break successfully\n");
}



void Theater_Sim_Test()
{
	Thread *t;
	printf("Starting tests.\n");
	
	init_values();
	// Test A
	// Manager test to only read from one cashier at a time
	for(int i = 0; i<30; i++)
	{
		t=new Thread("Manager");
		t->Fork((VoidFunctionPtr)TestAManagerOneCash,0);

		A_Done.P();
	}
	printf("Test A is Complete \n");
	
	// Test B
	// Manager test to get clerks off break 
	t = new Thread("Test B");
	t->Fork((VoidFunctionPtr)TestBManagerOffBreak,0);
	
	t = new Thread("Test B Clerk");
	t->Fork((VoidFunctionPtr)TestBClerkOnBreak,0);
	printf("Test B is Complete \n");

	
}
