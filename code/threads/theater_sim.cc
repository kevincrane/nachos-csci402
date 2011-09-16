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
#define MAX_CC 5            // constant: defines maximum number of concessionClerks

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

CustomerStruct customers[MAX_CUST];       // List of all customers in theater
int totalCustomers;                       // total number of customers there actually are
int totalCustomersServed;									// total number of customers that finished
int totalGroups;                          // total number of groups there actually are
Lock* customerLock;
Condition* customerCV;

// Group Global variables
int groupHeads[MAX_CUST];                 // List of indices pointing to the leaders of each group
int groupSize[MAX_CUST];                  // The size of each group


// TicketClerk Global variables
int   ticketClerkState[MAX_TC];           // Current state of a ticketClerk
Lock* ticketClerkLock[MAX_TC];            // array of Locks, each corresponding to one of the TicketClerks
Condition*  ticketClerkCV[MAX_TC];        // condition variable assigned to particular TicketClerk
int   ticketClerkLineCount[MAX_TC];       // How many people in each ticketClerk's line?
int   numberOfTicketsNeeded[MAX_TC];      // How many tickets does the current customer need?
int   amountOwedTickets[MAX_TC];          // How much money the customer owes for tickets
int   ticketClerkRegister[MAX_TC];        // amount of money the ticketClerk has collected
int   theaterOnTicket = 1;                // Theater Number (TODO: only 1 theater?)
int	  ticketClerkWorking;				  // Number of Ticket Clerks working
Lock* ticketClerkLineLock;                // Lock for line of customers
Condition*  ticketClerkLineCV[MAX_TC];		// Condition variable assigned to particular line of customers
Lock* ticketClerkBreakLock;								// Lock for when TC goes on break
Condition* ticketClerkBreakCV;						// Condition variable for when TC goes on break

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
Lock* ticketTakerBreakLock;								// Lock for when TT goes on break
Condition* ticketTakerBreakCV;						// Condition variable when TT goes on break

// ConcessionClerk Global variables
Lock* concessionClerkLineLock;
int concessionClerkLineCount[MAX_CC];
Lock* concessionClerkLock[MAX_CC];
int concessionClerkWorking;
int concessionClerkRegister[MAX_CC];
int concessionClerkState[MAX_CC];
Lock* concessionClerkBreakLock;					// Lock for when CC goes on break
Condition* concessionClerkBreakCV;			// Condition variable for when CC goes on break


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
      printf("Customer %i in Group %i has entered the movie theater\n", j, i);
  
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
      printf("Customer %i in Group %i is walking up to TicketClerk%i to buy %i tickets\n", 
          custIndex, groupIndex, myTicketClerk, groupSize[groupIndex]);
      DEBUG('p', "cust%i: talking to tc%i.\n", custIndex, myTicketClerk);
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
    printf("Customer %i in Group %i is getting in TicketClerk line %i\n", custIndex, groupIndex, myTicketClerk);
    DEBUG('p', "cust%i: waiting for tc%i.\n", custIndex, myTicketClerk);
    
    // Get in the shortest line
    ticketClerkLineCount[myTicketClerk]++;
    ticketClerkLineCV[myTicketClerk]->Wait(ticketClerkLineLock);
  }
  ticketClerkLineLock->Release();
  // Done with either going moving to TicketClerk or getting in line. Now sleep.
  
  // TicketClerk has acknowledged you. Time to wake up and talk to him.
  ticketClerkLock[myTicketClerk]->Acquire();
  numberOfTicketsNeeded[myTicketClerk] = groupSize[groupIndex];
  ticketClerkCV[myTicketClerk]->Signal(ticketClerkLock[myTicketClerk]);
  // Done asking for 'myGroupSize' tickets from TicketClerk. Now sleep.
  
  // TicketClerk has returned a price for you
  ticketClerkCV[myTicketClerk]->Wait(ticketClerkLock[myTicketClerk]);
  customers[custIndex].money -= amountOwedTickets[myTicketClerk];               // Pay the ticketClerk for the tickets
  printf("Customer%i: Charge my Visa-brand debit card for $%i, please!\n", custIndex, amountOwedTickets[myTicketClerk]);
  DEBUG('p', "cust%i: paying for tickets.\n", custIndex);
  ticketClerkCV[myTicketClerk]->Signal(ticketClerkLock[myTicketClerk]);
  
  // TicketClerk has given you n tickets. Goodies for you!
  ticketClerkCV[myTicketClerk]->Wait(ticketClerkLock[myTicketClerk]);
  customers[custIndex].numTickets += numberOfTicketsNeeded[myTicketClerk];      // Receive tickets!
  printf("Customer%i: Thanks for the tickets, now I have %i!\n", custIndex, customers[custIndex].numTickets);
  DEBUG('p', "cust%i: receiving tickets and (possibly) heading to concession stand.\n", custIndex);

  ticketClerkLock[myTicketClerk]->Release();                                    // Fly free ticketClerk. You were a noble friend.
  ticketClerkCV[myTicketClerk]->Signal(ticketClerkLock[myTicketClerk]);         // You're free to carry on noble ticketClerk
  
}

void groupHead(int custIndex) 
{
  int groupIndex = customers[custIndex].group;
  
  // Buy tickets from ticketClerk if you're a groupHead
  
  DEBUG('p', "cust%i: Begin to buy movie tickets.\n", custIndex);
  doBuyTickets(custIndex, groupIndex);
  
  // doBuySnacks(custIndex, groupIndex);
  // doTurnInTickets(custIndex, groupIndex);  //TODO: loop back to doTurnInTickets if forced into lobby like stupid sheep
  // doEnterTheater   //TODO: who's doing and how? concerns everyone in customers[] btwn (currIndex) and (currIndex+groupSize[groupIndex])
  // doLeaveTheaterAndTakeAShit
  
  //Counter to see how many customers have left movies
  totalCustomersServed += groupSize[groupIndex];
  for(int i=custIndex; i<custIndex+groupSize[groupIndex]; i++)
  {
    printf("Customer %i in Group %i has left the movie theater\n", i, customers[i].group);
  }
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
      DEBUG('p', "tc%i: No one wants to buy tickets right now :(\n", myIndex);  // :(
      ticketClerkState[myIndex] = 0;
    }
    
    DEBUG('p', "tc%i: Acquiring lock on self.\n", myIndex);
    ticketClerkLock[myIndex]->Acquire();                            // Call dibs on this current ticketClerk for a while
    ticketClerkLineLock->Release();
    
    //Wait for Customer to come to my counter and ask for tickets; tell him what he owes.
    ticketClerkCV[myIndex]->Wait(ticketClerkLock[myIndex]);
    amountOwedTickets[myIndex] = numberOfTicketsNeeded[myIndex] * TICKET_PRICE;   // Tell customer how money he owes
    printf("TicketClerk%i: \"%i tickets will cost $%i.\"\n", myIndex, numberOfTicketsNeeded[myIndex], amountOwedTickets[myIndex]);
    DEBUG('p', "tc%i: Telling customer what he owes.\n", myIndex);
    ticketClerkCV[myIndex]->Signal(ticketClerkLock[myIndex]);
    
    // Customer has given you money, give him the correct number of tickets, send that fool on his way
    ticketClerkCV[myIndex]->Wait(ticketClerkLock[myIndex]);
    ticketClerkRegister[myIndex] += amountOwedTickets[myIndex];     // Add money to cash register
    numberOfTicketsHeld = numberOfTicketsNeeded[myIndex];           // Give customer his tickets
    printf("TicketClerk%i: \"Here are your %i tickets. Enjoy the show!\"\n", myIndex, numberOfTicketsHeld);
    DEBUG('p', "tc%i: Giving %i tickets to customer and sending him on his way.\n", myIndex, numberOfTicketsHeld);
    ticketClerkCV[myIndex]->Signal(ticketClerkLock[myIndex]);
    
    DEBUG('p', "tc%i: Finished handling customer.\n", myIndex);
    ticketClerkCV[myIndex]->Wait(ticketClerkLock[myIndex]);         // Wait until customer out of the way before beginning next one
    
    // TODO: Wait for next customer? Don't think it's necessary
    // TODO: ability to go on break; give manager the burden of calling for break
    //  trust manager to not call for break unless ticketClerkLineSize[myIndex] == 0
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



// MANAGER  (TODO: ADD MOAR COMMENTS)
void manager(int myIndex)
{
  int pastTotal = 0;
  int totalRevenue = 0;
	bool ticketTakerWorkNow = false;
	bool ticketClerkWorkNow = false;
	bool concessionClerkWorkNow = false;
  while(true)
  {
  	//Put Employee on Break
  	DEBUG('p', "Manager: Checking for employees to go on break. \n");
  	
  	//TicketTaker Break
  	for(int i = 0; i < MAX_TT; i++)		//Put TicketTaker on break 
  	{
  	  ticketTakerLineLock->Acquire();
  	  DEBUG('p', "Manager: Acquiring ticketTakerLineLock %i to check line length of 0. \n", i);
  	  if(ticketTakerLineCount[i]==0 && ticketTakerState[i]!=2)
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
  	  if(concessionClerkLineCount[i]==0&&concessionClerkState[i]!=2)
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
  	  if(ticketClerkLineCount[i]==0&&ticketClerkState[i]!=2)
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
  	
  	// Take Employee Off Break
		// Ticket Taker
		for(int i = 0; i < MAX_TT; i++)
		{
			DEBUG('p', "Manager: Acquiring ticketTakerLineLock %i to check line length > 5.\n", i);
			ticketTakerLineLock->Acquire();
			if(ticketTakerLineCount[i]>5)
			{
				DEBUG('p', "Manager: Found line longer than 5. Flagging ticketTaker%i to come off break.\n",i);
				ticketTakerWorkNow = true;						//Set status to having the employee work
			}
			ticketTakerLineLock->Release();
			break;
		}
		if(ticketTakerWorkNow && ticketTakerWorking<MAX_TT)
		{
			DEBUG('p', "Manager: Signalling a ticketTaker to come off break.\n");
			//Get TT off break
			ticketTakerBreakLock->Acquire();
			ticketTakerBreakCV->Signal(ticketTakerBreakLock);
			ticketTakerBreakLock->Release();
		}
		
		// Ticket Clerk
		for(int i = 0; i < MAX_TC; i++)
		{
			DEBUG('p', "Manager: Acquiring ticketClerkLineLock %i to check line length > 5.\n", i);
			ticketClerkLineLock->Acquire();
			if(ticketClerkLineCount[i]>5)
			{
				DEBUG('p', "Manager: Found line longer than 5. Flagging ticketClerk%i to come off break.\n", i);
				ticketClerkWorkNow = true;					//Set status to have clerk work
			}
			ticketClerkLineLock->Release();
			break;
		}
		if(ticketClerkWorkNow && ticketClerkWorking<MAX_TT)
		{
			DEBUG('p', "Manager: Signalling a ticketClerk to come off break.\n");
			//Get TC off break
			ticketClerkBreakLock->Acquire();
			ticketClerkBreakCV->Signal(ticketClerkBreakLock);
			ticketClerkBreakLock->Release();
		}
		// Concession Clerk
		for(int i = 0; i < MAX_CC; i++)
		{
			DEBUG('p', "Manager: Acquiring concessionClerkLineLock %i to check line length > 5.\n", i);
			concessionClerkLineLock->Acquire();
			if(concessionClerkLineCount[i]>5)
			{
				DEBUG('p', "Manager: Found line longer than 5. Flagging concessionClerk%i to come off break.\n", i);
				concessionClerkWorkNow = true;
			}
			concessionClerkLineLock->Release();
			break;
		}
		if(concessionClerkWorkNow && concessionClerkWorking<MAX_CC)
		{
			DEBUG('p', "Manager: Signalling a concessionClerk to come off break.\n");
			//Get CC off break
			concessionClerkBreakLock->Acquire();
			concessionClerkBreakCV->Signal(concessionClerkBreakLock);
			concessionClerkBreakLock->Release();
		}
		
	
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
  	pastTotal = totalRevenue;
  	totalRevenue = 0;
  	for(int i = 0; i<MAX_CC; i++)
  	{
  	  concessionClerkLock[i]->Acquire();
  	  totalRevenue += concessionClerkRegister[i];
  	  concessionClerkLock[i]->Release();
  	}
  	for(int i = 0; i<MAX_TC; i++)
  	{
  	  ticketClerkLock[i]->Acquire();
  	  totalRevenue += ticketClerkRegister[i];
  	  ticketClerkLock[i]->Release();
  	}
  	DEBUG('p', "Manager: Total amount of money in theater is $%i \n", totalRevenue);
  	
  	//TODO: Check theater sim complete
  	if(totalCustomersServed == totalCustomers){
  		DEBUG('p', "\n\nManager: Everyone is happy and has left. Closing the theater.\n\n\n");
  		break;
  	}
  	
  	
  	//Pause  ---   NOT SURE HOW LONG TO MAKE MANAGER YIELD - ryan
  	for(int i=0; i<10; i++)
  	{
  	  currentThread->Yield();
  	}
  }	
  	
}	  

// MOVIE TECHNICIAN



// Initialize values and players in this theater
void init() {
  DEBUG('p', "Initializing values and players in the movie theater.\n");
  int aGroups[] = {3, 1, 4, 5, 3, 4, 1, 1, 5, 2, 3};
  int aNumGroups = len(aGroups);
  totalCustomersServed = 0;
  // Initialize ticketClerk values
	Thread *t;
	ticketClerkLineLock = new Lock("TC_LINELOCK");
	ticketClerkBreakLock = new Lock("TC_BREAKLOCK");
	ticketClerkBreakCV = new Condition("TC_BREAKCV");
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
	
	// Initialize customers
  customerInit(aGroups, len(aGroups));
	for(int i=0; i<aNumGroups; i++) 
	{
	  // Fork off a new thread for a customer
	  DEBUG('p', "Forking new thread: customer%i\n", i);
	  t = new Thread("cust");
	  t->Fork((VoidFunctionPtr)groupHead,groupHeads[i]);
	}
	
	// Initialize ticketTaker values
	ticketTakerLineLock = new Lock("TT_LINELOCK");
	ticketTakerBreakLock = new Lock("TT_BREAKLOCK");
	ticketTakerBreakCV = new Condition("TT_BREAKCV");
	for(int i=0; i<MAX_TT; i++)
	{
	  ticketTakerLock[i] = new Lock("TT_LOCK");
	}
	
	// Initialize concessionClerk values
	concessionClerkLineLock = new Lock("CC_LINELOCK");
	concessionClerkBreakLock = new Lock("CC_BREAKLOCK");
	concessionClerkBreakCV = new Condition("CC_BREAKCV");
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
	
	DEBUG('p', "We have this many customers: %i\n", totalCustomers);
	t = new Thread("man");
	t->Fork((VoidFunctionPtr)manager,0);  //TODO: make this run without being an infinite loop - fixed by adding some code to the customer (ryan)
}

//Temporary to check if makefile works
void Theater_Sim() {
  DEBUG('p', "Beginning movie theater simulation.\n");
  
	init();
	return;
}

