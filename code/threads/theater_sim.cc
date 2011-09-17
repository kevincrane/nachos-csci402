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
#define MAX_SEATS 10        // constant: max number of seats in the theater

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
int   totalCustomers;                       // total number of customers there actually are
int   totalCustomersServed;									// total number of customers that finished
int   totalGroups;                          // total number of groups there actually are
Lock* customerLock;
Condition*  customerCV;
Lock* customerLobbyLock;        // Lock for when cust in lobby
Condition*  customerLobbyCV;     // CV for when cust in lobby

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
int	  ticketClerkWorking;                 // Number of Ticket Clerks working
Lock* ticketClerkLineLock;                // Lock for line of customers
Condition*  ticketClerkLineCV[MAX_TC];		// Condition variable assigned to particular line of customers
Lock* ticketClerkBreakLock;								// Lock for when TC goes on break
Condition* ticketClerkBreakCV;						// Condition variable for when TC goes on break

// TicketTaker Global variables
int   ticketTakerState[MAX_TT];           // Current state of a ticketTaker
Lock* ticketTakerLock[MAX_TT];            // array of Locks, each corresponding to one of the ticketTakers
Condition*  ticketTakerCV[MAX_TT];        // condition variable assigned to particular ticketTaker
int   ticketTakerLineCount[MAX_TT];       // How many people in each ticketTaker's line?
bool  allowedIn[MAX_TT];                  // Is this group allowed into the theater?
int   numTicketsReceived[MAX_TT];         // Number of tickets the customer gave
int   totalTicketsTaken=0;          // Total tickets taken for this movie
bool  movieStarted;                       // Has the movie started already?
int ticketTakerWorking;                   // Number of Ticket Takers currently working
Lock* ticketTakerLineLock;                // Lock for line of customers
Condition*  ticketTakerLineCV[MAX_TT];		// Condition variable assigned to particular line of customers
Lock* ticketTakerMovieLock;               // Lock for ticket takers to acquire for wait
Condition* ticketTakerMovieCV;            // Condition variable for wait
Lock* ticketTakerBreakLock;               // Lock for when TT goes on break
Condition* ticketTakerBreakCV;            // Condition variable when TT goes on break

Lock* ticketTakerCounterLock = new Lock("counter");

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
    
//    customerLobbyLock[i] = new Lock("C_LOBBY_LOCK");
//    customerLobbyCV[i] = new Condition("C_LOBBY_CV");
  }
customerLobbyLock = new Lock("C_LOBBY_LOCK");
    customerLobbyCV = new Condition("C_LOBBY_CV");
  DEBUG('p', "\n");
}


// Bring a group of Customers back from the lobby


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
    myTicketClerk = 0;      // Default back-up value, in case all ticketClerks on break
    
    DEBUG('p', "cust%i: all tc's occupied, looking for shortest line.\n", custIndex);
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
  printf("Customer %i in Group %i in TicketClerk line %i is paying %i for tickets\n",
      custIndex, groupIndex, myTicketClerk, amountOwedTickets[myTicketClerk]);
  ticketClerkCV[myTicketClerk]->Signal(ticketClerkLock[myTicketClerk]);
  
  // TicketClerk has given you n tickets. Goodies for you!
  ticketClerkCV[myTicketClerk]->Wait(ticketClerkLock[myTicketClerk]);
  customers[custIndex].numTickets += numberOfTicketsNeeded[myTicketClerk];      // Receive tickets!
  printf("Customer %i in Group %i is leaving TicketClerk%i\n",
      custIndex, groupIndex, myTicketClerk);
  DEBUG('p', "cust%i: I has %i tickets!\n", custIndex, customers[custIndex].numTickets);

  ticketClerkLock[myTicketClerk]->Release();                                    // Fly free ticketClerk. You were a noble friend.
  ticketClerkCV[myTicketClerk]->Signal(ticketClerkLock[myTicketClerk]);         // You're free to carry on noble ticketClerk
  
}

void doGiveTickets(int custIndex, int groupIndex)
{
  int myTicketTaker = -1;
  
  ticketTakerLineLock->Acquire();
  //See if any ticketTaker not busy
  for(int i=0; i<MAX_TT; i++) 
  {
    if(ticketTakerState[i] == 0) {
      //Found a clerk who's not busy
      myTicketTaker = i;             // and now you belong to me
      ticketTakerState[i] = 1;
      printf("Customer %i in Group %i is getting in TicketTaker line %i\n",
          custIndex, groupIndex, myTicketTaker);
      DEBUG('p', "cust%i: talking to tt%i.\n", custIndex, myTicketTaker);
      break;
    }
  }
  
  // All ticketTakers were occupied, find the shortest line instead
  if(myTicketTaker == -1) 
  {
    DEBUG('p', "cust%i: all tt's occupied, looking for shortest line.\n", custIndex);
    int shortestTTLine = -1;     // default the first line to current shortest
    int shortestTTLineLength = 10000;
    for(int i=0; i<MAX_TT; i++) {
      if((ticketTakerState[i] == 1) && (shortestTTLineLength > ticketTakerLineCount[i])) {
        //current line is shorter
        shortestTTLine = i;
        shortestTTLineLength = ticketTakerLineCount[i];
      }
    }
    
    // Found the TicketClerk with the shortest line
    myTicketTaker = shortestTTLine;
    printf("Customer %i in Group %i is getting in TicketTaker line %i\n", 
        custIndex, groupIndex, myTicketTaker);
    DEBUG('p', "cust%i: waiting for tt%i.\n", custIndex, myTicketTaker);
    
    // Get in the shortest line
    ticketTakerLineCount[myTicketTaker]++;
    ticketTakerLineCV[myTicketTaker]->Wait(ticketTakerLineLock);
  }
  ticketTakerLineLock->Release();
  // Done with either going moving to TicketTaker or getting in line. Now sleep.
  
  // TicketTaker has acknowledged you. Tell him how many tickets you have
  ticketTakerLock[myTicketTaker]->Acquire();
  numTicketsReceived[myTicketTaker] = groupSize[groupIndex];
  printf("Customer %i in Group %i is walking up to TicketTaker %i to give %i tickets.\n",
      custIndex, groupIndex, myTicketTaker, numTicketsReceived[myTicketTaker]);
  ticketTakerCV[myTicketTaker]->Signal(ticketTakerLock[myTicketTaker]);
  
  // Did the TicketTaker let you in?
  ticketTakerCV[myTicketTaker]->Wait(ticketTakerLock[myTicketTaker]);
  if(!allowedIn[myTicketTaker]) {
    // Theater was either full or the movie had started
    for(int i=custIndex; i<custIndex+groupSize[groupIndex]; i++)
      printf("Customer %i in Group %i is in the lobby.\n", i, groupIndex);
    
    ticketTakerLock[myTicketTaker]->Release();                                  // Fly free ticketTaker. You were a true friend.
    ticketTakerCV[myTicketTaker]->Signal(ticketTakerLock[myTicketTaker]);       // You're free to carry on noble ticketTaker
    
    customerLobbyLock->Acquire();
    customerLobbyCV->Wait(customerLobbyLock);
    customerLobbyLock->Release();
    
    // Try this step again now that the group is free from the lobby's terrible grasp
    doGiveTickets(custIndex, groupIndex);
    return;
  } else {
    // Allowed into theater
    printf("Allowed into theater slut.\n");
      
    ticketTakerLock[myTicketTaker]->Release();                                  // Fly free ticketClerk. You were a noble friend.
    ticketTakerCV[myTicketTaker]->Signal(ticketTakerLock[myTicketTaker]);       // You're free to carry on noble ticketClerk
  
  }
}


void groupHead(int custIndex) 
{
  int groupIndex = customers[custIndex].group;
  
  // Buy tickets from ticketClerk if you're a groupHead
  
  DEBUG('p', "cust%i: Begin to buy movie tickets.\n", custIndex);
  // Buy tickets from ticketClerk
  doBuyTickets(custIndex, groupIndex);
  
  // doBuySnacks(custIndex, groupIndex);
  
  // Give tickets to ticketTaker
  doGiveTickets(custIndex, groupIndex);  
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
  if(myIndex==2 || myIndex==3)
    ticketClerkState[myIndex] = 2;
    
  int numberOfTicketsHeld;
  while(true) 
  {
    //Is there a customer in my line?
    DEBUG('p', "tc%i: Acquiring ticketClerkLineLock.\n", myIndex);
    ticketClerkLineLock->Acquire();
    if(ticketClerkLineCount[myIndex] > 0) {	// There's a customer in my line
      ticketClerkState[myIndex] = 1;        // I must be busy, decrement line count
      ticketClerkLineCount[myIndex]--;
      printf("TicketClerk %i has a line length %i and is signaling a customer.\n", 
          myIndex, ticketClerkLineCount[myIndex]+1);
      ticketClerkLineCV[myIndex]->Signal(ticketClerkLineLock); // Wake up 1 customer
    } else if(ticketClerkState[myIndex] == 2) {
      // ticketClerk is on break, don't disturb him or he'll get cranky
    } else {
      //No one in my line
      printf("TicketClerk %i has no one in line. I am available for a customer.\n", myIndex);
      ticketClerkState[myIndex] = 0;
    }
    
    DEBUG('p', "tc%i: Acquiring lock on self.\n", myIndex);
    ticketClerkLock[myIndex]->Acquire();                            // Call dibs on this current ticketClerk for a while
    ticketClerkLineLock->Release();
    
    //Wait for Customer to come to my counter and ask for tickets; tell him what he owes.
    ticketClerkCV[myIndex]->Wait(ticketClerkLock[myIndex]);
    amountOwedTickets[myIndex] = numberOfTicketsNeeded[myIndex] * TICKET_PRICE;   // Tell customer how much money he owes
    printf("TicketClerk %i has an order for %i and the cost is %i.\n",
        myIndex, numberOfTicketsNeeded[myIndex], amountOwedTickets[myIndex]);
    ticketClerkCV[myIndex]->Signal(ticketClerkLock[myIndex]);
    
    // Customer has given you money, give him the correct number of tickets, send that fool on his way
    ticketClerkCV[myIndex]->Wait(ticketClerkLock[myIndex]);
    ticketClerkRegister[myIndex] += amountOwedTickets[myIndex];     // Add money to cash register
    numberOfTicketsHeld = numberOfTicketsNeeded[myIndex];           // Give customer his tickets
    DEBUG('p', "tc%i: Giving %i tickets to customer and sending him on his way.\n", myIndex, numberOfTicketsHeld);
    ticketClerkCV[myIndex]->Signal(ticketClerkLock[myIndex]);
    
    DEBUG('p', "tc%i: Finished handling customer.\n", myIndex);
    ticketClerkCV[myIndex]->Wait(ticketClerkLock[myIndex]);         // Wait until customer is out of the way before beginning next one
    
    // TODO: ability to go on break; give manager the burden of calling for break
  }
}



// CONCESSION CLERK




// TICKET TAKER
void ticketTaker(int myIndex)
{
  if(myIndex==2 || myIndex==0)
    ticketTakerState[myIndex] = 2;

  while(true)
  {
    //Is there a customer in my line?
    ticketTakerLineLock->Acquire();
    if(ticketTakerLineCount[myIndex] > 0) {	// There's a customer in my line
      ticketTakerState[myIndex] = 1;        // I must be busy, decrement line count
      ticketTakerLineCount[myIndex]--;
      printf("TicketTaker %i has a line length %i and is signaling a customer.\n", 
          myIndex, ticketTakerLineCount[myIndex]+1);
      ticketTakerLineCV[myIndex]->Signal(ticketTakerLineLock); // Wake up 1 customer
    } else if(ticketTakerState[myIndex] == 2) {
      // ticketTaker is on break, don't disturb him or face his wrath
    } else {
      //No one in my line
      printf("TicketTaker %i has no one in line. I am available for a customer.\n", myIndex);
      ticketTakerState[myIndex] = 0;
    }
    ticketTakerLock[myIndex]->Acquire();
    ticketTakerLineLock->Release();
    
    //Wait for Customer to tell me how many tickets he has
    ticketTakerCV[myIndex]->Wait(ticketTakerLock[myIndex]);
       
    if(numTicketsReceived[myIndex] > (MAX_SEATS-totalTicketsTaken) || movieStarted)
    {
      // Movie has either started or is too full
      if(movieStarted)
        printf("TicketTaker %i has stopped taking tickets.\n", myIndex);
      else
        printf("TicketTaker %i is not allowing the group into the theater. The number of taken tickets is %i and the group size is %i.\n", 
            myIndex, totalTicketsTaken, numTicketsReceived[myIndex]);
        
      allowedIn[myIndex] = false;
      ticketTakerCV[myIndex]->Signal(ticketTakerLock[myIndex]);
      ticketTakerCV[myIndex]->Wait(ticketTakerLock[myIndex]);
      continue;
    } else {
      allowedIn[myIndex] = true;
    }
    ticketTakerCV[myIndex]->Signal(ticketTakerLock[myIndex]);

    //Wait for Customer to give me their tickets
    printf("TicketTaker %i has received %i tickets.\n", myIndex, numTicketsReceived[myIndex]);
    // TODO: update number of people in theater, make manager deal with this?
    ticketTakerCounterLock->Acquire();
    totalTicketsTaken += numTicketsReceived[myIndex];
    ticketTakerCounterLock->Release();
    ticketTakerCV[myIndex]->Wait(ticketTakerLock[myIndex]);
    // TicketTaker is done with customer, allows them to move into the theater
  }
}



// MANAGER  (TODO: ADD MOAR COMMENTS)

/*	To have a employee go on break
		The manager will check if the line length is 0 first to see if no one is in a line
		or will check if less than 3 in the line and another employee is working
		If either are true there is a 20% chance the manager will tell them to go on break
		The manager will set their state to 2 meaning they should go on break
		In that employee's code there should be an if statement checking for this state
		
		In order to make your employee go on break you need to acquire the appropriate lock
		Then use the condition variable to wait
		ie
			ticketTakerBreakLock->Acquire()
			ticketTakerBreakCV->Wait(TicketTakerBreakLock)
		By doing this the employee will go on break and enter a queue for when they will be called of break
		
		If a line gets greater than 5, the manager will signal this condition variable which will get
		the employee of break
		
		Make sure you have a release for the break lock after the wait!
*/
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
    		  printf("TicketTaker %i is going on break.\n", i);
    		  ticketTakerLock[i]->Acquire();
    		  ticketTakerState[i] = 2;
    		  ticketTakerLock[i]->Release();
    		  //break;
    		}
  	  }
  	  else if(ticketTakerWorking > 0)
  	  {
  	  	ticketTakerLock[i]->Acquire();
	    	if(rand() % 5 == 0)
	    	{
	    	  DEBUG('p', "Manager: TicketTaker%i is going to take a break since another employee is working. \n", i);
	    	  ticketTakerState[i] = 2;
	    	}
 		    ticketTakerLock[i]->Release();
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
  	if(movieStatus == 2)		//Stopped
  	{
  	  if(totalTicketsTaken == 0)
  	  {
  	    //Tell TicketTakers to take tickets
  	    ticketTakerMovieLock->Acquire();
  	    ticketTakerMovieCV->Broadcast(ticketTakerMovieLock);
  	    ticketTakerMovieLock->Release();
  	    
  	    //Tell Customers to go to theater
  	    customerLobbyLock->Acquire();
  	    customerLobbyCV->Broadcast(customerLobbyLock);
  	    customerLobbyLock->Release();
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
	
  // Initialize concessionClerk values
  concessionClerkLineLock = new Lock("CC_LINELOCK");
  concessionClerkBreakLock = new Lock("CC_BREAKLOCK");
  concessionClerkBreakCV = new Condition("CC_BREAKCV");
  for(int i=0; i<MAX_CC; i++)
  {
    concessionClerkLock[i] = new Lock("CC_LOCK");
  }
	
	// Initialize ticketTaker values
  ticketTakerLineLock = new Lock("TT_LINELOCK");
  ticketTakerBreakLock = new Lock("TT_BREAKLOCK");
  ticketTakerBreakCV = new Condition("TT_BREAKCV");
  for(int i=0; i<MAX_TT; i++) 
	{
    ticketTakerLineCV[i] = new Condition("TT_lineCV");		// instantiate line condition variables
    ticketTakerLock[i] = new Lock("TT_LOCK");
    ticketTakerCV[i] = new Condition("TT_CV");
    
    // Fork off a new thread for a ticketTaker
    DEBUG('p', "Forking new thread: ticketTaker%i\n", i);
    t = new Thread("tt");
    t->Fork((VoidFunctionPtr)ticketTaker,i);
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
	
	// Initialize movieTechnician values
	movieStatusLock = new Lock("MS_LOCK");
	DEBUG ('p', "Forking new thread: manager\n");
	DEBUG('p', "We have this many customers: %i\n", totalCustomers);
	t = new Thread("man");
//	t->Fork((VoidFunctionPtr)manager,0);
	
}

//Temporary to check if makefile works
void Theater_Sim() {
  DEBUG('p', "Beginning movie theater simulation.\n");
  
	init();
	return;
}

