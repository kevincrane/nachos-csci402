/* theater_sim_test.c
 *	Tests for theater simulation
 */

#include "syscall.h"

/* CONSTANTS */
#define MAX_CUST 100        /* constant: maximum number of customers                */
#define MAX_TC 5            /* constant: defines maximum number of ticketClerks     */
#define MAX_TT 3            /* constant: defines maximum number of ticketTakers     */
#define MAX_CC 5            /* constant: defines maximum number of concessionClerks */
#define MAX_SEATS 25        /* constant: max number of seats in the theater         */
#define NUM_ROWS 5          /* constant: number of rows in the theater              */
#define NUM_COLS 5          /* constant: number of seats/row                        */

#define TICKET_PRICE 12     /* constant: price of a movie ticket */
#define POPCORN_PRICE 5     /* constant: price of popcorn        */
#define SODA_PRICE 4        /* constant: price of soda           */

/* Globals created for testing */
int   ticketClerkRegister[MAX_TC];
int totalRevenue;
int registerToCheck;
int ticketClerkLock[MAX_TC];
int ticketClerkLineCount[MAX_TC];
int ticketClerkState[MAX_TC];
int ticketClerkWorking;
int ticketClerkWorkNow;
int ticketClerkBreakLock[MAX_TC];
int ticketClerkBreakCV[MAX_TC];
int ticketClerkLineLock;
int ticketClerkCV[MAX_TC];
int ticketClerkLock[MAX_TC];
int custIndex;
int groupIndex;
int myTicketClerk;
int shortestTCLine;
int shortestTCLineLength;
int concessionClerkLock[MAX_CC];
int concessionClerkRegister[MAX_CC];
int ticketClerkLineCV[MAX_TC];

int mainCV;
int mainLock;

void init_values(){
  int i;
	mainCV = CreateCV();
	mainLock = CreateLock();
	
  /* Initialize ticketClerk values */
  ticketClerkLineLock = CreateLock();
  for(i=0; i<MAX_TC; i++) 
  {
    ticketClerkLineCV[i] = CreateCV();		/* instantiate line condition variables */
    ticketClerkLock[i] = CreateLock();
    ticketClerkCV[i] = CreateCV();
    ticketClerkBreakLock[i] = CreateLock();
    ticketClerkBreakCV[i] = CreateCV();
  }
	
	
  /* Initialize concessionClerk values */
  for(i=0; i<MAX_CC; i++) {
   	concessionClerkLock[i] = CreateLock();
    
    concessionClerkRegister[i] = 0;
  }
	
  /* Initialize manager values */
  totalRevenue = 0;
}


/* Test A to get manager to get funds in cash registers */
void TestAManagerOneCash()
{
	Print("\n\n\n", -1,-1,-1);
	/* Add some funds to ticket clerk registers */
	ticketClerkRegister[0] = 100;
	ticketClerkRegister[1] = 150;

	/* Make manager total equal to 0*/
	totalRevenue = 0;

	/*Pick a register to read from*/
	
	registerToCheck = Random(2);
	
	/* Copied code directly from manager*/

	/*Code modified to check proper acquiring*/
	Acquire(ticketClerkLock[registerToCheck]);
	totalRevenue += ticketClerkRegister[registerToCheck];
	Print("Manager collected [%i] from TicketClerk[%i].\n", ticketClerkRegister[registerToCheck], registerToCheck, -1); 
	ticketClerkRegister[registerToCheck] = 0;

	Release(ticketClerkLock[registerToCheck]);
	
	/*Check to see if register values match total revenue*/
	if(registerToCheck==0)
	{
		if(totalRevenue!=100)
			Print("ERROR: Manager read registers incorrectly.\n", -1, -1, -1);
	}
	else if(registerToCheck==1)
	{
		if(totalRevenue!=150)
			Print("ERROR: Manager read registers incorrectly.\n", -1, -1, -1);
	}
	
	/*Check to see if coffers were emptied*/
	if(ticketClerkRegister[registerToCheck] != 0)
		Print("ERROR: Manager emptied registers incorrectly.\n", -1, -1, -1);

	Acquire(mainLock);
	Signal(mainCV, mainLock);
	Release(mainLock);
	Exit(0);
}

/* Test B to have manager get clerk off break*/

int testBLock;
int testBCV;

void TestBManagerOffBreak()
{	
	/* Set TicketClerk line to 6 waiting in line*/
	ticketClerkLineCount[0] = 6;
	/* Set TicketClerk status to on break*/
	ticketClerkState[0] = 2;
	ticketClerkWorking = 1;
	/* Set flag to put TicketClerk back to work*/
	ticketClerkWorkNow = 0;
	
	
	Acquire(testBLock);
	Wait(testBCV, testBLock);
	Release(testBLock);
	
	/*Have manager signal ticketClerk*/
	/*Copied from code */
	Acquire(ticketClerkLineLock);
	if(ticketClerkLineCount[0]>5)
	{
		ticketClerkWorkNow = 1;					/*Set status to have clerk work*/
	}
	Release(ticketClerkLineLock);
	
	if(ticketClerkWorkNow && ticketClerkWorking<MAX_TT)
	{
		/*Get TC off break*/
		Acquire(ticketClerkBreakLock[0]);
		Signal(ticketClerkBreakCV[0],ticketClerkBreakLock[0]);
		Release(ticketClerkBreakLock[0]);
	}
	Print("Manager signaled Test B clerk to get off break\n", -1,-1,-1);
	Acquire(mainLock);
	Signal(mainCV, mainLock);
	Release(mainLock);
	Exit(0);
}

void TestBClerkOnBreak()
{
	Acquire(ticketClerkBreakLock[0]);
	Acquire(testBLock);
	Signal(testBCV, testBLock);
	Release(testBLock);
	ticketClerkWorking--;
  Wait(ticketClerkBreakCV[0],ticketClerkBreakLock[0]);
	
	Print("Test B Clerk got off break successfully\n", -1, -1, -1);
	Exit(0);
}

/* Test C*/
int testCLock;
int testCCV;
void TestCCustomer()
{
	int myTicketClerk = 0;
	Acquire(testCLock);
	Wait(testCCV, testCLock);
	Release(testCLock);
	Print("Customer 1 began\n", -1, -1, -1);
	/* Added to get code to work*/
  Acquire(ticketClerkLock[myTicketClerk]);
	/*Copied code*/
	Signal(ticketClerkCV[myTicketClerk],ticketClerkLock[myTicketClerk]);         /* You're free to carry on noble ticketClerk*/
  Release(ticketClerkLock[myTicketClerk]);                                    /* Fly free ticketClerk. You were a noble friend.*/
	Exit(0);
  
}

void TestCCustomer2()
{
	int myTicketClerk = 0;
	Acquire(testCLock);
	Wait(testCCV, testCLock);
	Release(testCLock);
	Print("Customer 2 began\n", -1, -1, -1);
	/* Code to test, copied over*/
	Acquire(ticketClerkLineLock);
	Print("Customer 2 acquired clerk line lock\n", -1, -1, -1);
	ticketClerkLineCount[myTicketClerk]++;
	Wait(ticketClerkLineCV[myTicketClerk],ticketClerkLineLock);
	Print("Clerk has grabbed me\n", -1, -1, -1);
	Release(ticketClerkLineLock);
	Exit(0);
}
void TestCClerk()

{
	ticketClerkLineCount[0] = 0;
	Acquire(testCLock);
	Broadcast(testCCV, testCLock);
	Release(testCLock);
	
	Acquire(ticketClerkLock[0]);
	Wait(ticketClerkCV[0],ticketClerkLock[0]);
	Release(ticketClerkLock[0]);
	Print("Clerk has been released!\n", -1,-1,-1);
	Acquire(ticketClerkLineLock);
    if(ticketClerkLineCount[0] > 0) {	/* There's a customer in my line*/
      ticketClerkState[0] = 1;        /* I must be busy, decrement line count*/
      ticketClerkLineCount[0]--;
      Print("TicketClerk %i has a line length %i and is signaling a customer.\n", 
          0, ticketClerkLineCount[0]+1, -1);
      Signal(ticketClerkLineCV[0],ticketClerkLineLock); 
			Release(ticketClerkLineLock);
		}
	Print("Clerk is done\n", -1, -1, -1);
	Acquire(mainLock);
	Signal(mainCV, mainLock);
	Release(mainLock);
	Exit(0);
}


/* Test D*/

int i;
int testDLock;
int testDCV;

void TestD_1()

{
	/* Set 2 ticket clerks to have open lines*/
	ticketClerkState[0] = 1;
	ticketClerkState[1] = 1;
	
	/*Randomly set their line lengths*/
	ticketClerkLineCount[0] = Random(5);
	ticketClerkLineCount[1] = Random(4);
	
	/*Print lengths*/
	Print("Line 0: %i | Line 1: %i\n", ticketClerkLineCount[0], ticketClerkLineCount[1],-1);
	Acquire(testDLock);
	Wait(testDCV, testDLock);
	Release(testDLock);
	/* Initialize Customer variables*/
	custIndex = 1;
	groupIndex = 1;
	
	/* Code to test*/
	Acquire(ticketClerkLineLock);
	
	myTicketClerk = 0; /*Default back-up value, in case all ticketClerks on break*/

	shortestTCLine = -1;     /* default the first line to current shortest */
	shortestTCLineLength = 100000;
	
	for(i=0; i<MAX_TC; i++) {
	  if((ticketClerkState[i] == 1) && (shortestTCLineLength > ticketClerkLineCount[i])) {
	    /*current line is shorter*/
	    shortestTCLine = i;
	    shortestTCLineLength = ticketClerkLineCount[i];
	  }
	}
	/*Found the TicketClerk with the shortest line*/
	myTicketClerk = shortestTCLine;   
	/* Get in the shortest line*/
	ticketClerkLineCount[myTicketClerk]++;
	Print("Customer %i is getting in line %i| Length : %i\n", custIndex, myTicketClerk, ticketClerkLineCount[myTicketClerk]);
	Release(ticketClerkLineLock);
	Acquire(mainLock);
	Signal(mainCV, mainLock);
	Release(mainLock);
	Exit(0);
}
int j;
void TestD_2()
{
	
	/* Initialize variables*/
	int custIndex = 2;
	int groupIndex = 2;
	int myTicketClerk = 0; /* Default back-up value, in case all ticketClerks on break*/
	int shortestTCLine = -1;     /* default the first line to current shortest*/
	int shortestTCLineLength = 100000;
	
	/* Code to test*/
	Acquire(ticketClerkLineLock);
	Acquire(testDLock);
	Signal(testDCV, testDLock);
	Release(testDLock);
	
	for(j=0; j<MAX_TC; j++) {
	  if((ticketClerkState[i] == 1) && (shortestTCLineLength > ticketClerkLineCount[i])) {
	    /*current line is shorter*/
	    shortestTCLine = j;
	    shortestTCLineLength = ticketClerkLineCount[j];
	  }
	}
	/* Found the TicketClerk with the shortest line*/
	myTicketClerk = shortestTCLine;
  
	/* Get in the shortest line*/
	ticketClerkLineCount[myTicketClerk]++;
	Print("Customer %i is getting in line %i| Length : %i\n", custIndex, myTicketClerk, ticketClerkLineCount[myTicketClerk]);
	Release(ticketClerkLineLock);
	Exit(0);
}

/* Test E*/
int k;
int testELock;
int testECV;
void TestE_1()
{
	totalRevenue = 0;
	Acquire(testELock);
	Wait(testECV, testELock);
	Release(testELock);
	/*Check clerk money levels*/
	for(k = 0; k<MAX_CC; k++)
	{
		Acquire(concessionClerkLock[k]);
		totalRevenue += concessionClerkRegister[k];

		Print("Manager collected %i from ConcessionClerk%i.\n", concessionClerkRegister[k], k, -1);
		concessionClerkRegister[k] = 0;
		Release(concessionClerkLock[k]);
	}
	for(k = 0; k<MAX_TC; k++)
	{
		Acquire(ticketClerkLock[k]);
		totalRevenue += ticketClerkRegister[k];
		Print("Manager collected %i from TicketClerk%i.\n", ticketClerkRegister[k], k, -1); 
		ticketClerkRegister[k] = 0;

		Release(ticketClerkLock[k]);
	}
	Print("Total money made by office = %i\n", totalRevenue, -1, -1);
	Acquire(mainLock);
	Signal(mainCV, mainLock);
	Release(mainLock);
	Exit(0);
}

int actualRevenue;
int l;
void TestE_2()
{
	/* Reset all register values*/
	for( l = 0; l<MAX_CC; l++)
		concessionClerkRegister[l] = 0;
	for( l = 0; l<MAX_TC; l++)
		ticketClerkRegister[l] = 0;
	actualRevenue = 0;
	for( l = 0; l<MAX_CC; l++){
		Acquire(concessionClerkLock[l]);
		concessionClerkRegister[l] = Random(20);
		Release(concessionClerkLock[l]);
	}
	for( l = 0; l<MAX_TC; l++){
		Acquire(ticketClerkLock[l]);
		ticketClerkRegister[l] = Random(15);
		Release(ticketClerkLock[l]);
	}
	for( l = 0; l<MAX_CC; l++)
		actualRevenue+=concessionClerkRegister[l];
	for( l = 0; l<MAX_TC; l++)
		actualRevenue+=ticketClerkRegister[l];
	Acquire(testELock);
	Signal(testECV, testELock);
	Release(testELock);
	Exit(0);
}

int m;
int main()
{
	Print("Starting tests.\n", -1,-1,-1);
	init_values();
	/* Test A*/
	/* Manager test to only read from one cashier at a time*/
	Fork((void *)TestAManagerOneCash);
	
	Acquire(mainLock);
	Wait(mainCV, mainLock);
	Release(mainLock);
	
	Print("---------------------------------------------------------------------------\n", -1, -1, -1);
	Print("Test A is Complete \n", -1, -1, -1);
	Print("---------------------------------------------------------------------------\n", -1, -1, -1);
	
	/* Test B */
	/* Manager test to get clerks off break */
	testBLock = CreateLock();
	testBCV = CreateCV();
	Fork((void*)TestBManagerOffBreak);

	Fork((void*)TestBClerkOnBreak);

	Acquire(mainLock);
	Wait(mainCV, mainLock);
	Release(mainLock);


	Print("---------------------------------------------------------------------------\n", -1, -1, -1);
	Print("Test B is Complete \n", -1, -1, -1);	
	Print("---------------------------------------------------------------------------\n", -1, -1, -1);
	
	/* Test C*/
	/* Clerks wait for customer to signal them to move on*/
	testCLock = CreateLock();
	testCCV = CreateCV();	
	Fork((void*)TestCCustomer);
	Fork((void*)TestCCustomer2);
	Fork((void*)TestCClerk);
	
	

	Acquire(mainLock);
	Wait(mainCV, mainLock);
	Release(mainLock);
	Print("---------------------------------------------------------------------------\n", -1, -1, -1);
	Print("Test C is Complete \n", -1, -1, -1);
	Print("---------------------------------------------------------------------------\n", -1, -1, -1);

	
	/* Test D*/
	/* Customers always choose shortest line*/
	testDLock = CreateLock();
	testDCV = CreateCV();
	Fork((void*)TestD_1);
	Fork((void*)TestD_2);

		

	Acquire(mainLock);
	Wait(mainCV, mainLock);
	Release(mainLock);
	Print("---------------------------------------------------------------------------\n", -1, -1, -1);
	Print("Test D is Complete \n", -1, -1, -1);
	Print("---------------------------------------------------------------------------\n", -1, -1, -1);
	
	/* Test E */
	/* Manager collecting cash nevers suffers a race condition */
		testELock = CreateLock();
		testECV = CreateCV();
		Fork((void*)TestE_1);
		Fork((void*)TestE_2);
	Acquire(mainLock);
	Wait(mainCV, mainLock);
	Release(mainLock);
		Print("Actual Revenue: %i\n", actualRevenue, -1, -1);
	

	
	Print("---------------------------------------------------------------------------\n", -1, -1, -1);
	Print("Test E is Complete \n", -1, -1, -1);
	Print("---------------------------------------------------------------------------\n", -1, -1, -1);

	Exit(0);
	return 0;
}
