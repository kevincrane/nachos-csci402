// theater_sim.cc
// All relevant agent-like methods governing the interactions within the
// movie theater simulation.
//
// TODO: ADD DOCUMENTATION HERE

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "copyright.h"
#include "system.h"
#include "thread.h"
#include "theater_sim.h"

#ifdef CHANGED
#include "synch.h"
#endif


// CONSTANTS
#define MAX_CUST 50         // constant: maximum number of customers
#define MAX_TC 5            // constant: defines maximum number of ticketClerks
#define MAX_TT 3            // constant: defines maximum number of ticketTakers
#define MAX_CC 5

#define TICKET_PRICE 12     // constant: price of a movie ticket
#define POPCORN_PRICE 5     // constant: price of popcorn
#define SODA_PRICE 4        // constant: price of soda

#define len(x) (sizeof (x) / sizeof *(x))   // used for finding the length of arrays

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
int totalCustomers;         // total number of customers there actually are
int totalGroups;            // total number of groups there actually are
Lock* customerLock;
Condition* customerCV;

// Group Global variables
int groupHeads[MAX_CUST];   // List of indices pointing to the leaders of each group
int groupSize[MAX_CUST];    // The size of each group


// TicketClerk Global variables
int   ticketClerkState[MAX_TC];           // Current state of a ticketClerk
Lock* ticketClerkLock[MAX_TC];            // array of Locks, each corresponding to one of the TicketClerks
Condition*  ticketClerkCV[MAX_TC];        // condition variable assigned to particular TicketClerk
int   ticketClerkLineCount[MAX_TC];       // How many people in each ticketClerk's line?
int   numberOfTicketsNeeded[MAX_TC];      // How many tickets does the current customer need?
int   amountOwedTickets[MAX_TC];          // How much money the customer owes for tickets
int   theaterOnTicket = 1;                // Theater Number (TODO: only 1 theater?)
int	  ticketClerkWorking;				  // Number of Ticket Clerks working
Lock* ticketClerkLineLock;                // Lock for line of customers
Condition*  ticketClerkLineCV[MAX_TC];		// Condition variable assigned to particular line of customers
int   ticketClerkRegister[MAX_TC];		  // Money register for ticket clerk

// TicketTaker Global variables
int   ticketTakerState[MAX_TT];           // Current state of a ticketTaker
Lock* ticketTakerLock[MAX_TT];            // array of Locks, each corresponding to one of the ticketTakers
Condition*  ticketTakerCV[MAX_TT];        // condition variable assigned to particular ticketTaker
int   ticketTakerLineCount[MAX_TT];       // How many people in each ticketTaker's line?
int   numberOfTheater[MAX_TT];            // What theater are these customers going to?
int   numTicketsReceived[MAX_TT];         // Number of tickets the customer gave
int   totalTicketsTaken[MAX_TT];          // Total tickets taken for this movie
bool  movieStarted;                       // Has the movie started already?
int ticketTakerWorking;					  // Number of Ticket Takers currently working
Lock* ticketTakerLineLock;                // Lock for line of customers
Condition*  ticketTakerLineCV[MAX_TT];		// Condition variable assigned to particular line of customers
Lock* ticketTakerMovieLock;				  // Lock for ticket takers to acquire for wait
Condition* ticketTakerMovieCV;			  // Condition variable for wait

// ConcessionClerk Global variables
Lock* concessionClerkLineLock;
int concessionClerkLineCount[MAX_CC];
Lock* concessionClerkLock[MAX_CC];
int concessionClerkWorking;
int concessionClerkRegister[MAX_CC];
int concessionClerkState[MAX_CC];

// MovieTechnician Global variables
Lock* movieStatusLock;
int numSeatsOccupied;
int movieStatus;
Condition* movieStatusLockCV;


// CODE HERE BITCHES


// Customer Code

// init all values and assemble into groups
void customerInit(int groups[], int numGroups) 
{
  int currIndex = 0;
  totalCustomers = 0;
  totalGroups = numGroups;
  
  // Iterate through each of the groups passed in; each int is number of people in group
  for(int i=0; i<numGroups; i++) 
  {
    groupHeads[i] = currIndex;    // Current index corresponds to a group leader
    groupSize[i] = groups[i];     // Number of people in current group
    DEBUG('p', "cust_init: Initializing a group of %i customers, starting at customer[%i].\n", groupSize[i], currIndex);
    for(int j=currIndex; j<(currIndex+groups[i]); j++) 
    {
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
    totalCustomers += groups[i];
  }
  
  DEBUG('p', "\n");
}

void doBuyTickets(int custIndex, int groupIndex) 
{
  int myTicketClerk = -1;
  
  ticketClerkLineLock->Acquire();
  //See if any TicketClerk not busy
  for(int i=0; i<MAX_TC; i++) 
  {
    if(ticketClerkState[i] == 0) {   //TODO: should use enum BUSY={0,1, other states (e.g. on break?)}
      //Found a clerk who's not busy
      myTicketClerk = i;             // and now you belong to me
      ticketClerkState[i] = 1;
      DEBUG('p', "cust%i: talking to tc%i.\n", custIndex, myTicketClerk);
      printf("Customer %i: Talking to TicketClerk %i.\n", custIndex, myTicketClerk);
      break;
    }
  }
  
  // All ticketClerks were occupied, find the shortest line instead
  if(myTicketClerk == -1) 
  {
    DEBUG('p', "cust%i: all tc's occupied, looking for shortest line.\n", custIndex);
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
    DEBUG('p', "cust%i: waiting for tc%i.\n", custIndex, myTicketClerk);
    printf("Customer%i: Waiting in line for TicketClerk %i.\n", custIndex, myTicketClerk);
    
    // Get in the shortest line
    ticketClerkLineCount[myTicketClerk]++;
    ticketClerkLineCV[myTicketClerk]->Wait(ticketClerkLineLock);
  }
  ticketClerkLineLock->Release();
  // Done with either going moving to TicketClerk or getting in line. Now sleep.
  
  // TicketClerk has acknowledged you. Time to wake up and talk to him.
  ticketClerkLock[myTicketClerk]->Acquire();
  DEBUG('p', "cust%i: tc has acknowledged me, saying I need %i tickets.\n", custIndex, groupSize[groupIndex]);
  numberOfTicketsNeeded[myTicketClerk] = groupSize[groupIndex];
  ticketClerkCV[myTicketClerk]->Signal(ticketClerkLock[myTicketClerk]);
  // Done asking for 'myGroupSize' tickets from TicketClerk. Now sleep.
  
  // TicketClerk has returned a price for you
  ticketClerkCV[myTicketClerk]->Wait(ticketClerkLock[myTicketClerk]);
  DEBUG('p', "cust%i: paying for tickets and (possibly) heading to concession stand.\n", custIndex);
  printf("Customer%i: Charge my Visa-brand debit card for $%i, please!\n", custIndex, amountOwedTickets[myTicketClerk]);
  ticketClerkCV[myTicketClerk]->Signal(ticketClerkLock[myTicketClerk]);
  
  DEBUG('p', "cust%i: Finished buying tickets.\n\n", custIndex);
}


void groupHead(int custIndex) 
{
  int groupIndex = customers[custIndex].group;
  
  // Buy tickets from ticketClerk if you're a groupHead
  
  DEBUG('p', "cust: Begin to buy movie tickets.\n");
  doBuyTickets(custIndex, groupIndex);
  
  // doBuySnacks(custIndex, groupIndex);
  // doTurnInTickets(custIndex, groupIndex);  //TODO: loop back to doTurnInTickets if forced into lobby like stupid sheep
  // doEnterTheater   //TODO: who's doing and how? concerns everyone in customers[] btwn (currIndex) and (currIndex+groupSize[groupIndex])
  // doLeaveTheaterAndShit
  
  DEBUG('p', "cust: Finished my evening at the movies.\n\n");
}




// TicketClerk
// -Thread that is run when Customer is interacting with TicketClerk to buy movie tickets
void ticketClerk(int myIndex) 
{
  int numberOfTicketsHeld;
  while(true) 
  {
    //Is there a customer in my line?
    DEBUG('p', "tc%i: Acquiring ticketClerkLineLock.\n", myIndex);
    ticketClerkLineLock->Acquire();
    if(ticketClerkLineCount[myIndex] > 0) {	// There's a customer in my line
      ticketClerkState[myIndex] = 1;        // I must be busy, decrement line count
      ticketClerkLineCount[myIndex]--;
      DEBUG('p', "tc%i: Removing customer from line, signalling him to wake up.\n", myIndex);
      ticketClerkLineCV[myIndex]->Signal(ticketClerkLineLock); // Wake up 1 customer
    } else {
      //No one in my line
      DEBUG('p', "tc%i: No one wants to buy tickets right now :(\n", myIndex);
      ticketClerkState[myIndex] = 0;
    }
    
    DEBUG('p', "tc%i: Acquiring lock on self.\n", myIndex);
    ticketClerkLock[myIndex]->Acquire();
    ticketClerkLineLock->Release();
        
    //Wait for Customer to come to my counter and ask for tickets; tell him what he owes.
    ticketClerkCV[myIndex]->Wait(ticketClerkLock[myIndex]);
    DEBUG('p', "tc%i: Telling customer what he owes.\n", myIndex);
    amountOwedTickets[myIndex] = numberOfTicketsNeeded[myIndex] * TICKET_PRICE;
    printf("TicketClerk%i: \"%i tickets will cost $%i.\"\n", myIndex, numberOfTicketsNeeded[myIndex], amountOwedTickets[myIndex]);
    ticketClerkCV[myIndex]->Signal(ticketClerkLock[myIndex]);
    
    // Customer has given you money, give him the correct number of tickets, send that fool on his way
    ticketClerkCV[myIndex]->Wait(ticketClerkLock[myIndex]);
    numberOfTicketsHeld = numberOfTicketsNeeded[myIndex];
    DEBUG('p', "tc, myIndex: Giving %i tickets to customer and sending him on his way.\n\n", myIndex, numberOfTicketsHeld);
    printf("TicketClerk%i: \"Here are your %i tickets. Enjoy the show!\"\n", myIndex, numberOfTicketsHeld);
    ticketClerkCV[myIndex]->Signal(ticketClerkLock[myIndex]);
    
    // TODO: Wait for next customer? Don't think it's necessary
    // TODO: ability to go on break; give manager the burden of calling for break
    //  trust manager to not call for break unless ticketClerkLineSize[myIndex] == 0
    DEBUG('p', "tc%i: Finished handling customer.\n", myIndex);
  }
}




// CONCESSION CLERK




// TICKET TAKER
void ticketTaker(int myIndex)
{
  while(true)
  {
    //Is there a customer in my line?
    ticketTakerLineLock->Acquire();
    if(movieStarted) {                      // Movie has begun, send customers to lobby to twiddle their thumbs
      DEBUG('p', "tt%i: movie already started, customers should wait in lobby.\n", myIndex);
      for(int i=0; i<ticketTakerLineCount[myIndex]; i++) {
        // TODO: tell customer to go wait in the lobby
      }
    } else {
      if(ticketTakerLineCount[myIndex] > 0) {	// There's a customer in my line
        ticketTakerState[myIndex] = 1;        // I must be busy, decrement line count
        ticketTakerLineCount[myIndex]--;
        DEBUG('p', "tt%i: found some customers, waiting for them to give me their tickets.\n", myIndex);
        ticketTakerLineCV[myIndex]->Signal(ticketTakerLineLock); // Wake up 1 customer
      } else {
        //No one in my line
        DEBUG('p', "tt%i: no one wants to give me their tickets right now :(\n", myIndex);
        ticketTakerState[myIndex] = 0;
      }
      
      ticketTakerLock[myIndex]->Acquire();
      ticketTakerLineLock->Release();
      
      //Wait for Customer to give me their tickets
      ticketTakerCV[myIndex]->Wait(ticketTakerLock[myIndex]);
      numberOfTheater[myIndex] = theaterOnTicket;
      DEBUG('p', "tt%i: customers gave me their tickets, pointed them to theater %i.\n", myIndex, theaterOnTicket);
      printf("TicketClerk%i: \"Go to theater %i for your movie, fine sirs.\"\n", myIndex, numberOfTheater[myIndex]);
      ticketTakerCV[myIndex]->Signal(ticketTakerLock[myIndex]);
      
      // TicketTaker is done with customer, allows them to move into the theater
    }
  }
}



// MANAGER     -------     ADD MOAR COMMENTS
// TODO     more comments
void manager(int myIndex)
{
  int pastTotal = 0;
  int total = 0;
  while(true)
  {
  	//Put Employee on Break
  	DEBUG('p', "Manager: Checking for employees to go on break. \n");
  	
  	//TicketTaker Break
  	for(int i = 0; i < MAX_TT; i++)		//Put TicketTaker on break 
  	{
  	  ticketTakerLineLock->Acquire();
  	  DEBUG('p', "Manager: Acquiring ticketTakerLineLock %i to check line length of 0. \n", i);
  	  if(ticketTakerLineCount[i]==0)
  	  {
  	    DEBUG('p', "Manager: TicketTaker%i has no one in line. \n", i);
  	    if(rand() % 5 == 0)
  		{
  		  DEBUG('p', "Manager: TicketTaker%i is going to take a break. \n", i);
  		  ticketTakerLock[i]->Acquire();
  		  ticketTakerState[i] = 2;
  		  ticketTakerLock[i]->Release();
  		  //break;
  		}
  	  }
  	  else
  	  {
  	  	ticketTakerLock[i]->Acquire();
  	    if(ticketTakerWorking > 0)
  	    {
  	    	if(rand() % 5 == 0)
  	    	{
  	    	  DEBUG('p', "Manager: TicketTaker%i is going to take a break since another employee is working. \n", i);
  	    	  ticketTakerState[i] = 2;
  	    	}
  	    ticketTakerLock[i]->Release();
  	  	}
  	  }
  	  ticketTakerLineLock->Release();
	}	  

  	//ConcessionClerk Break
  	for(int i = 0; i < MAX_CC; i++)		//Put ConcessionClerk on break 
  	{
  	  concessionClerkLineLock->Acquire();
  	  DEBUG('p', "Manager: Acquiring concessionClerkLineLock %i to check line length of 0. \n", i);
  	  if(concessionClerkLineCount[i]==0)
  	  {
  	    DEBUG('p', "Manager: concessionClerk%i has no one in line. \n", i);
  	    if(rand() % 5 == 0)
  		{
  		  DEBUG('p', "Manager: concessionClerk%i is going to take a break. \n", i);
  		  concessionClerkLock[i]->Acquire();
  		  concessionClerkState[i] = 2;			//State as 2 means to take a break
  		  concessionClerkLock[i]->Release();
  		}
  	  }
  	  else
  	  {
  	  	concessionClerkLock[i]->Acquire();
  	    if(concessionClerkWorking > 0)
  	    {
  	    	if(rand() % 5 == 0)
  	    	{
  	    	  DEBUG('p', "Manager: concessionClerk%i is going to take a break since another employee is working. \n", i);
  	    	  concessionClerkState[i] = 2;
  	    	}
  	    concessionClerkLock[i]->Release();
  	  	}
  	  }
  	  concessionClerkLineLock->Release();
	}	
  		  	
  	//TicketClerk Break
  	for(int i = 0; i < MAX_TC; i++)		//Put TicketClerk on break 
  	{
  	  ticketClerkLineLock->Acquire();
  	  DEBUG('p', "Manager: Acquiring ticketClerkLineLock %i to check line length of 0. \n", i);
  	  if(ticketClerkLineCount[i]==0)
  	  {
  	    DEBUG('p', "Manager: ticketClerk%i has no one in line. \n", i);
  	    if(rand() % 5 == 0)
  		{
  		  DEBUG('p', "Manager: ticketClerk%i is going to take a break. \n", i);
  		  ticketClerkLock[i]->Acquire();
  		  ticketClerkState[i] = 2;			//State as 2 means to take a break
  		  ticketClerkLock[i]->Release();
  		}
  	  }
  	  else
  	  {
  	  	ticketClerkLock[i]->Acquire();
  	    if(ticketClerkWorking > 0)
  	    {
  	    	if(rand() % 5 == 0)
  	    	{
  	    	  DEBUG('p', "Manager: ticketClerk%i is going to take a break since another employee is working. \n", i);
  	    	  ticketClerkState[i] = 2;
  	    	}
  	    ticketClerkLock[i]->Release();
  	  	}
  	  }
  	  ticketClerkLineLock->Release();
	}	
  	
  	//Take Employee Off Break
  	
  	DEBUG('p', "Manager: Checking movie to see if it needs to be restarted.\n");
  	//Check to start movie
  	movieStatusLock->Acquire();
  	if(movieStatus == 2)		//Stopped
  	{
  	  movieStatusLock->Release();
  	  if(numSeatsOccupied == 0)
  	  {
  	    //Tell TicketTakers to take tickets
  	    ticketTakerMovieLock->Acquire();
  	    ticketTakerMovieCV->Broadcast(ticketTakerMovieLock);
  	    ticketTakerMovieLock->Release();
  	    
  	    //Tell Customers to go to theater
  	    customerLock->Acquire();
  	    customerCV->Broadcast(customerLock);
  	    customerLock->Release();
  	  }
  	  //TODO: Pause for random movie starting
  	  //Set movie ready to start
  	  
  	  movieStatusLock->Acquire();
  	  movieStatus = 0;
  	  movieStatusLockCV->Signal(movieStatusLock);
  	  movieStatusLock->Release();
  	  
  	}
  	
  	//Check clerk money levels
  	pastTotal = total;
  	total = 0;
  	for(int i = 0; i<MAX_CC; i++)
  	{
  	  concessionClerkLock[i]->Acquire();
  	  total = total + concessionClerkRegister[i];
  	  concessionClerkLock[i]->Release();
  	}
  	for(int i = 0; i<MAX_TC; i++)
  	{
  	  ticketClerkLock[i]->Acquire();
  	  total = total + ticketClerkRegister[i];
  	  ticketClerkLock[i]->Release();
  	}
  	DEBUG('p', "Manager: Total amount of money in theater is $%i \n", total);
  	
  	//TODO: Check theater sim complete
  	
  	//Pause
  	for(int i=0; i<50; i++)
  	{
  	  currentThread->Yield();
  	}
  }	
  	
}	  

// MOVIE TECHNICIAN




// Initialize values and players in this theater
void init() {
  DEBUG('p', "Initializing values and players in the movie theater.\n");
  int aGroups[10] = {3, 1, 4}; //, 5, 3, 4, 1, 1, 5, 2};
  
  // Initialize customers and groups
  customerInit(aGroups, len(aGroups));
  
  // Initialize ticketClerk values
	Thread *t;
	ticketClerkLineLock = new Lock("TC_LINELOCK");
	for(int i=0; i<MAX_TC; i++) 
	{
    ticketClerkLineCV[i] = new Condition("TC_lineCV");		// instantiate line condition variables
    ticketClerkLock[i] = new Lock("TC_LOCK");
    ticketClerkCV[i] = new Condition("TC_CV");
    
    
    // Fork off a new thread for a ticketClerk
    DEBUG('p', "Forking new thread: ticketClerk%i\n", i);
    t = new Thread("tc");
    t->Fork((VoidFunctionPtr)ticketClerk,i);
	}
	
	for(int i=0; i<totalCustomers; i++) 
	{
	  // Fork off a new thread for a customer
	  DEBUG('p', "Forking new thread: customer%i\n", i);
	  t = new Thread("cust");
	  t->Fork((VoidFunctionPtr)groupHead,i);
	}
	
	
	
	// Initialize ticketTaker values
	ticketTakerLineLock = new Lock("TT_LINELOCK");
	for(int i=0; i<MAX_TT; i++)
	{
	  ticketTakerLock[i] = new Lock("TT_LOCK");
	}
	
	// Initialize concessionClerk values
	concessionClerkLineLock = new Lock("CC_LINELOCK");
	for(int i=0; i<MAX_CC; i++)
	{
	  concessionClerkLock[i] = new Lock("CC_LOCK");
	}
	
	// Initialize movieTechnician values
	movieStatusLock = new Lock("MS_LOCK");
	/*MANAGER GOES LAST....MMM K
		
	//Fork off new thread for a manager
	
	//Commented out for now because it creates a seg fault due to lack of initialization*/
	
	DEBUG ('p', "Forking new thread: manager\n");
	t = new Thread("man");
	t->Fork((VoidFunctionPtr)manager,0);
}

//Temporary to check if makefile works
void Theater_Sim() {
  DEBUG('p', "Beginning movie theater simulation.\n");
  
	init();
	return;
}
