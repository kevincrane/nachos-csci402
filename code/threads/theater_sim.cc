// theater_sim.cc
// All relevant agent-like methods governing the interactions within the
// movie theater simulation.
//
// TODO: ADD DOCUMENTATION HERE

#include <stdio.h>

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
int   ticketClerkRegister[MAX_TC];        // amount of money the ticketClerk has collected
int   theaterOnTicket = 1;                // Theater Number (TODO: only 1 theater?)

Lock* ticketClerkLineLock;                // Lock for line of customers
Condition*  ticketClerkLineCV[MAX_TC];		// Condition variable assigned to particular line of customers

// TicketTaker Global variables
int   ticketTakerState[MAX_TT];           // Current state of a ticketTaker
Lock* ticketTakerLock[MAX_TT];            // array of Locks, each corresponding to one of the ticketTakers
Condition*  ticketTakerCV[MAX_TT];        // condition variable assigned to particular ticketTaker
int   ticketTakerLineCount[MAX_TT];       // How many people in each ticketTaker's line?
int   numberOfTheater[MAX_TT];            // What theater are these customers going to?
int   numTicketsReceived[MAX_TT];         // Number of tickets the customer gave
int   totalTicketsTaken[MAX_TT];          // Total tickets taken for this movie
bool  movieStarted;                       // Has the movie started already?

Lock* ticketTakerLineLock;                // Lock for line of customers
Condition*  ticketTakerLineCV[MAX_TT];		// Condition variable assigned to particular line of customers



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
      printf("Customer %i: Talking to TicketClerk %i.\n", custIndex, myTicketClerk);
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
    printf("Customer%i: Waiting in line for TicketClerk %i.\n", custIndex, myTicketClerk);
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
  
//  ticketClerkCV[myTicketClerk]->Wait(ticketClerkLock[myTicketClerk]);
  
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
  
  DEBUG('p', "cust%i: Finished my evening at the movies.\n\n", custIndex);
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



// MANAGER



// MOVIE TECHNICIAN




// Initialize values and players in this theater
void init() {
  DEBUG('p', "Initializing values and players in the movie theater.\n");
  int aGroups[] = {3, 1, 4, 5, 3, 4, 1, 1, 5, 2, 3};
  int aNumGroups = len(aGroups);
  
  // Initialize customers and groups
  customerInit(aGroups, aNumGroups);
  
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
	
  for(int i=0; i<aNumGroups; i++) 
  {
    // Fork off a new thread for a customer
    DEBUG('p', "Forking new groupHead thread: customer%i\n", groupHeads[i]);
    t = new Thread("cust");
    t->Fork((VoidFunctionPtr)groupHead,groupHeads[i]);
  }
}

//Temporary to check if makefile works
void Theater_Sim() {
  DEBUG('p', "Beginning movie theater simulation.\n");
  
  init();
  return;
}
