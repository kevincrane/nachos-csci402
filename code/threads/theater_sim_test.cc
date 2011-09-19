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
Semaphore B_Done("bd",0);
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
	B_Done.V();
}

// Test C
Semaphore C_Done("cd", 0);
Semaphore testc("tc", 0);
Semaphore testc2("tc2", 0);
Semaphore testc3("tc3", 0);
void TestCCustomer()
{
	testc.P();
	int myTicketClerk = 0;
	
	// Added to get code to work
  ticketClerkLock[myTicketClerk]->Acquire();
	//Copied code
	ticketClerkCV[myTicketClerk]->Signal(ticketClerkLock[myTicketClerk]);         // You're free to carry on noble ticketClerk
  ticketClerkLock[myTicketClerk]->Release();                                    // Fly free ticketClerk. You were a noble friend.
  
}

void TestCCustomer2()
{
	int myTicketClerk = 0;
	testc2.P();
	printf("Customer 2 began\n");
	// Code to test, copied over
	ticketClerkLineLock->Acquire();
	printf("Customer 2 acquired clerk line lock\n");
	ticketClerkLineCount[myTicketClerk]++;
	ticketClerkLineCV[myTicketClerk]->Wait(ticketClerkLineLock);
	printf("Clerk has grabbed me\n");
	C_Done.V();
}
void TestCClerk()
{
	ticketClerkLineCount[0] = 0;
	
	
	ticketClerkLock[0]->Acquire();
	testc.V();
	testc2.V();
	ticketClerkCV[0]->Wait(ticketClerkLock[0]);
	ticketClerkLock[0]->Release();
	printf("Clerk has been released!\n");
	ticketClerkLineLock->Acquire();
    if(ticketClerkLineCount[0] > 0) {	// There's a customer in my line
      ticketClerkState[0] = 1;        // I must be busy, decrement line count
      ticketClerkLineCount[0]--;
      printf("TicketClerk %i has a line length %i and is signaling a customer.\n", 
          0, ticketClerkLineCount[0]+1);
      ticketClerkLineCV[0]->Signal(ticketClerkLineLock); 
			ticketClerkLineLock->Release();
		}
	printf("Clerk is done\n");
	
}


// Test D
Semaphore testd("td", 0);
Semaphore D_Done("dd", 0);
Semaphore done("d", 0);
void TestD_1()
{
	// Set 2 ticket clerks to have open lines
	ticketClerkState[0] = 1;
	ticketClerkState[1] = 1;
	
	// Randomly set their line lengths
	ticketClerkLineCount[0] = rand() % 5;
	ticketClerkLineCount[1] = rand() % 4;
	
	// Print lengths
	printf("Line 0: %i | Line 1: %i\n", ticketClerkLineCount[0], ticketClerkLineCount[1]);
	testd.P();
	
	// Initialize Customer variables
	int custIndex = 1;
	int groupIndex = 1;
	
	// Code to test
	ticketClerkLineLock->Acquire();
	int myTicketClerk = 0; // Default back-up value, in case all ticketClerks on break

	int shortestTCLine = -1;     // default the first line to current shortest
	int shortestTCLineLength = 100000;
	for(int i=0; i<MAX_TC; i++) {
	  if((ticketClerkState[i] == 1) && (shortestTCLineLength > ticketClerkLineCount[i])) {
	    //current line is shorter
	    shortestTCLine = i;
	    shortestTCLineLength = ticketClerkLineCount[i];
	  }
	}
	// Found the TicketClerk with the shortest line
	myTicketClerk = shortestTCLine;   
	// Get in the shortest line
	ticketClerkLineCount[myTicketClerk]++;
	printf("Customer %i is getting in line %i| Length : %i\n", custIndex, myTicketClerk, ticketClerkLineCount[myTicketClerk]);
	ticketClerkLineLock->Release();
	D_Done.V();
}

void TestD_2()
{
	testd.V();
	// Initialize variables
	int custIndex = 2;
	int groupIndex = 2;
	
	// Code to test
	ticketClerkLineLock->Acquire();
	int myTicketClerk = 0; // Default back-up value, in case all ticketClerks on break
	int shortestTCLine = -1;     // default the first line to current shortest
	int shortestTCLineLength = 100000;
	for(int i=0; i<MAX_TC; i++) {
	  if((ticketClerkState[i] == 1) && (shortestTCLineLength > ticketClerkLineCount[i])) {
	    //current line is shorter
	    shortestTCLine = i;
	    shortestTCLineLength = ticketClerkLineCount[i];
	  }
	}
	// Found the TicketClerk with the shortest line
	myTicketClerk = shortestTCLine;
  
	// Get in the shortest line
	ticketClerkLineCount[myTicketClerk]++;
	printf("Customer %i is getting in line %i| Length : %i\n", custIndex, myTicketClerk, ticketClerkLineCount[myTicketClerk]);
	ticketClerkLineLock->Release();
	D_Done.V();
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
	printf("---------------------------------------------------------------------------\n");
	printf("Test A is Complete \n");
	printf("---------------------------------------------------------------------------\n");
	
	// Test B
	// Manager test to get clerks off break 
	t = new Thread("Test B");
	t->Fork((VoidFunctionPtr)TestBManagerOffBreak,0);
	
	t = new Thread("Test B Clerk");
	t->Fork((VoidFunctionPtr)TestBClerkOnBreak,0);


	B_Done.P();
	printf("---------------------------------------------------------------------------\n");
	printf("Test B is Complete \n");	
	printf("---------------------------------------------------------------------------\n");
	// Test C
	// Clerks wait for customer to signal them to move on
	t = new Thread("Test C 1");
	t->Fork((VoidFunctionPtr)TestCCustomer,0);
	
	t = new Thread("Test C 2");
	t->Fork((VoidFunctionPtr)TestCClerk,0);
	
	t = new Thread("Test C 3");
	t->Fork((VoidFunctionPtr)TestCCustomer2,0);
	C_Done.P();
	printf("---------------------------------------------------------------------------\n");
	printf("Test C is Complete \n");
	printf("---------------------------------------------------------------------------\n");
	// Test D
	// Customers always choose shortest line
	for(int i = 0; i<30; i++)
	{
		t = new Thread("Test D 1");
		t->Fork((VoidFunctionPtr)TestD_1, 0);
		
		t = new Thread("Test D 2");
		t->Fork((VoidFunctionPtr)TestD_2, 0);
		
		D_Done.P();
		D_Done.P();
		printf("\n");
	}	
	printf("---------------------------------------------------------------------------\n");
	printf("Test D is Complete \n");
	printf("---------------------------------------------------------------------------\n");
	

}
