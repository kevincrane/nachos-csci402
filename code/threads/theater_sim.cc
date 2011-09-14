// theater_sim.cc
// All relevant agent-like methods governing the interactions within the
// movie theater simulation.
//
// TODO: ADD DOCUMENTATION HERE

#include "copyright.h"
#include "system.h"
#include "thread.h"
#include "theater_sim.h"
#ifdef CHANGED
#include "synch.h"
#endif


// CONSTANTS
#define MAX_TC 5   			 // constant: defines maximum number of ticket clerks
#define MAX_CUST 50          // constant: maximum number of customers

// Customer Global variables
typedef struct {
  bool  isLeader;           // Is this customer the group leader?
  int   index;              // Index in customers[] array
  int   group;              // Which group am I?
  int   money;              // Amount of money carried
  int   numTickets;         // Number of tickets carried
  int   seatNumber;         // Seat number
  bool  hasSoda;            // Do I have soda?
  bool  hasPopcorn;         // Do I have popcorn?
} CustomerStruct;

CustomerStruct customers[MAX_CUST];   // List of all customers in theater

// Group Global variables
int groupHeads[MAX_CUST];   // List of indices pointing to the leaders of each group
int groupSize[MAX_CUST];    // The size of each group


// TicketClerk Global variables
int   ticketClerkState[MAX_TC];           // Current state of a ticketClerk
Lock*	ticketClerkLock[MAX_TC];            // array of Locks, each corresponding to one of the TicketClerks
Condition*	ticketClerkCV[MAX_TC];        // condition variable assigned to particular TicketClerk
int 	ticketClerkLineCount[MAX_TC];       // How many people in each ticketClerk's line?
int		numberOfTicketsNeeded[MAX_TC];      // How many tickets does the current customer need?

Lock*	ticketClerkLineLock;                // Lock for line of customers
Condition*	ticketClerkLineCV[MAX_TC];		// Condition variable assigned to particular line of customers


// CODE HERE BITCHES

// return the length of an int array
int len(int[] intArr) {
  return sizeof(intArr)/sizeof(int);
}


// Customer Code

// init all values and assemble into groups
void customerInit(int[] groups) {
  int currIndex = 0;
  // Iterate through each of the groups passed in; each int is number of people in group
  for(int i=0; i<len(groups); i++) {
    groupHeads[i] = currIndex;    // Current index corresponds to a group leader
    groupSize[i] = groups[i];     // Number of people in current group
    for(int j=currIndex; j<(currIndex+groups[i]; j++) {
      // Initialize all values for current customer
      if(j == currIndex) customers[j].isLeader = true;
        else customers[j].isLeader = false;
      customers[j].index = j;
      customers[j].group = i;
      customers[j].money = 120;
      customers[j].numTickets = 0;
      customers[j].seatNumber = -1;
      customers[j].hasPopcorn = false;
      customers[j].hasSoda = false;
    }
    currIndex += groups[i];
  }
}


void doBuyTicket() {

}


// Customer
void Customer(int localGroupSize)
{
  int myTicketClerk = -1;
  int myGroupSize = localGroupSize;
  //TODO: how separate group leader from regular bitch customer
  
  ticketClerkLineLock->Acquire();
  //See if any TicketClerk not busy
  for(int i=0; i<MAX_TC; i++) {
    if(ticketClerkState[i] == 0) {   //TODO: should use enum BUSY={0,1, other states (e.g. on break?)}
      //Found a clerk who's not busy
      myTicketClerk = i;             // and now you belong to me
      ticketClerkState[i] = 1;
      printf("Customer NAME: Talking to TicketClerk %i.\n", myTicketClerk);
      break;
    }
  }
  
  // All ticketClerks were occupied, find the shortest line instead
  if(myTicketClerk == -1) {
    int shortestTCLine = 0;     // default the first line to current shortest
    int shortestTCLineLength = ticketClerkLineCount[0];
    for(int i=1; i<MAX_TC; i++) {
      if((ticketClerkState[i] == 1) && (shortestTCLineLength > ticketClerkLineCount[i])) {
        //current line is shorter
        shortestTCLine = i;
        shortestTCLineLength = ticketClerkLineCount[i];
      }
    }
    
    // Found the TicketClerk with the shortest line
    myTicketClerk = shortestTCLine;
    printf("Customer NAME: Waiting in line for TicketClerk %i.\n", myTicketClerk);
    
    // Get in the shortest line
    ticketClerkLineCount[myTicketClerk]++;
    ticketClerkLineCV[myTicketClerk]->Wait(ticketClerkLineLock);
  }
  ticketClerkLineLock->Release();
  // Done with either going moving to TicketClerk or getting in line. Now sleep.
  
  // TicketClerk has acknowledged you. Time to wake up and talk to him.
  ticketClerkLock[myTicketClerk]->Acquire();
  numberOfTicketsNeeded[myTicketClerk] = myGroupSize;
  ticketClerkCV[myTicketClerk]->Signal(ticketClerkLock[myTicketClerk]);
  // Done asking for 'myGroupSize' tickets from TicketClerk. Now sleep.
  
  // TicketClerk has returned a price for you
  ticketClerkCV[myTicketClerk]->Wait(ticketClerkLock[myTicketClerk]);
  printf("Customer NAME: Here's my Visa-brand debit card!\n");
  ticketClerkCV[myTicketClerk]->Signal(ticketClerkLock[myTicketClerk]);
  
  // TicketClerk has given you tickets
  // TODO: either buy food or go to the correct theatre!
  
}

// TicketClerk
// -Thread that is run when Customer is interacting with TicketClerk to buy movie tickets
void ticketClerk(int myIndex) {
  int numberOfTicketsHeld = 0;
  while(true) {
    //Is there a customer in my line?
    ticketClerkLineLock->Acquire();
    if(ticketClerkLineCount[myIndex] > 0) {	// There's a customer in my line
      ticketClerkState[myIndex] = 1;        // I must be busy, decrement line count
      ticketClerkLineCount[myIndex]--;
      ticketClerkLineCV[myIndex]->Signal(ticketClerkLineLock); // Wake up 1 customer
    } else {
      //No one in my line
      ticketClerkState[myIndex] = 0;
    }
    
    ticketClerkLock[myIndex]->Acquire();
    ticketClerkLineLock->Release();
    
    //Wait for Customer to come to my counter and ask for tickets; tell him what he owes.
    ticketClerkCV[myIndex]->Wait(ticketClerkLock[myIndex]);
    printf("TicketClerk %i: \"%i tickets will cost $%i.\"\n", myIndex, numberOfTicketsNeeded[myIndex], numberOfTicketsNeeded[myIndex]*12);
    ticketClerkCV[myIndex]->Signal(ticketClerkLock[myIndex]);
    
    // Customer has given you money, give him the correct number of tickets, send that fool on his way
    ticketClerkCV[myIndex]->Wait(ticketClerkLock[myIndex]);
    numberOfTicketsHeld = numberOfTicketsNeeded[myIndex];
    printf("TicketClerk %i: \"Here are your %i tickets. Enjoy the show!\"\n", myIndex, numberOfTicketsNeeded[myIndex]);
    ticketClerkCV[myIndex]->Signal(ticketClerkLock[myIndex]);
    
    // TODO: Wait for next customer? Don't think it's necessary
    // TODO: ability to go on break; give manager the burden of calling for break
    //  trust manager to not call for break unless ticketClerkLineSize[myIndex] == 0
  } 
}


// Initialize values and players in this theater
void init() {

  int aGroups[10] = {3, 1, 4, 5, 3, 4, 1, 1, 5, 2};
  
  // Initialize customers and groups
  customerInit(aGroups);
  
	Thread *t;
	for(int i=0; i<MAX_TC; i++) {
		ticketClerkLineCV[i] = new Condition("TC_CV");		// instantiate line condition variables
		t = new Thread("TicketClerk");
		t->Fork((VoidFunctionPtr)ticketClerk,0);
	}
}



// MANAGER



// CONCESSION CLERK




// TICKET TAKER





// MOVIE TECHNICIAN




//Temporary to check if makefile works
void Theater_Sim() {

	printf("Works bitches\n");
	init();
	return;
}

