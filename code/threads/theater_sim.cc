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


// CONSTANTS    (TAs: change the values of these to whatever you find pleasing)
#define MAX_CUST 100         // constant: maximum number of customers
#define MAX_TC 5            // constant: defines maximum number of ticketClerks
#define MAX_TT 3            // constant: defines maximum number of ticketTakers
#define MAX_CC 5            // constant: defines maximum number of concessionClerks

#define MAX_SEATS 25        // constant: max number of seats in the theater
#define NUM_ROWS 5          // constant: number of rows in the theater
#define NUM_COLS 5          // constant: number of seats/row

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
  int   seatRow;            // Seat row
  int   seatCol;            // Seat column
  bool  hasSoda;            // Do I have soda?
  bool  hasPopcorn;         // Do I have popcorn?
  bool  wantsPopcorn;       // Do I want popcorn?
  bool  wantsSoda;          // Do I want soda?
  bool  needsRestroom;      // Do I have to use the restroom?
  int   totalPopcorns;      // Num popcorns the group wants
  int   totalSodas;         // Num sodas the group wants
} CustomerStruct;

CustomerStruct customers[MAX_CUST];       // List of all customers in theater
int totalCustomers;                       // total number of customers there actually are
int totalCustomersServed;	       	  // total number of customers that finished
int totalGroups;                          // total number of groups there actually are
int freeSeatsInRow[NUM_ROWS];             // seating arangements in theater
Lock* customerLobbyLock;                  // Lock for when cust in lobby
Condition* customerLobbyCV;               // CV for when cust in lobby
Lock* waitingOnGroupLock[MAX_CUST];       // Lock for when cust is waiting for group members in the restroom
Condition* waitingOnGroupCV[MAX_CUST];          

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
Condition*  ticketClerkLineCV[MAX_TC];	  // Condition variable assigned to particular line of customers
Lock* ticketClerkBreakLock;		  // Lock for when TC goes on break
Condition* ticketClerkBreakCV;		  // Condition variable for when TC goes on break
bool ticketClerkIsWorking[MAX_TC];

// TicketTaker Global variables
int   ticketTakerState[MAX_TT];           // Current state of a ticketTaker
Lock* ticketTakerLock[MAX_TT];            // array of Locks, each corresponding to one of the ticketTakers
Condition*  ticketTakerCV[MAX_TT];        // condition variable assigned to particular ticketTaker
int   ticketTakerLineCount[MAX_TT];       // How many people in each ticketTaker's line?

bool allowedIn[MAX_TT];                   // Is this group allowed into the theater?
int   numTicketsReceived[MAX_TT];         // Number of tickets the customer gave
int   totalTicketsTaken=0;                // Total tickets taken for this movie
bool  movieStarted;                       // Has the movie started already?
int ticketTakerWorking;                   // Number of Ticket Takers currently working
Lock* ticketTakerLineLock;                // Lock for line of customers
Condition*  ticketTakerLineCV[MAX_TT];    // Condition variable assigned to particular line of customers
Lock* ticketTakerMovieLock;               // Lock for ticket takers to acquire for wait
Condition* ticketTakerMovieCV;            // Condition variable for wait
Lock* ticketTakerBreakLock;               // Lock for when TT goes on break
Condition* ticketTakerBreakCV;            // Condition variable when TT goes on break
bool ticketTakerIsWorking[MAX_TT];
Lock* ticketTakerCounterLock = new Lock("counter");

// ConcessionClerk Global variables
Lock* concessionClerkLineLock;            // lock for line of customers
Lock* concessionClerkLock[MAX_CC];        // array of locks, each corresponding to one of the concessionClerks
Lock* concessionClerkBreakLock;           // Lock for when CC goes on break

Condition* concessionClerkLineCV[MAX_CC]; // Condition variable assigned to each line
Condition* concessionClerkCV[MAX_CC];     // Condition variable assigned to each concessionClerk
Condition* concessionClerkBreakCV;        // Condition variable for when CC goes on break

int concessionClerkLineCount[MAX_CC];     // How many people are in each concessionClerk's line
int concessionClerkState[MAX_CC];         // State of each concessionClerk, 0 = free, 1 = busy, 2 = on break
int concessionClerkRegister[MAX_CC];      // Cash register for each concessionClerk
int concessionClerkWorking;
int amountOwed[MAX_CC];                   // How much the customer being helped owes
int numPopcornsOrdered[MAX_CC];           // How many popcorns the customer wants
int numSodasOrdered[MAX_CC];              // How many sodas the customer wants
bool concessionClerkIsWorking[MAX_CC];    // If the CC is not on break = true

// MovieTechnician Global variables
int numSeatsOccupied;
Lock* movieStatusLock;                    // Lock corresponding to the movieStatus variable shared between the movieTech and the manager
Lock* movieFinishedLock;                  // Lock needed to signal customers when movie is finished

Condition* movieFinishedLockCV;           // Condition variable to signal customers when the movie is over
Condition* movieStatusLockCV;             // Condition variable to wait after the movie is over

int movieStatus;                          // 0 = need to start, 1 = playing, 2 = finished
int movieLength;                          // "Length" of the movie - a number of yields between 200 and 300
bool theaterDone = false;									// Flag for when the theater is finished
bool theaterStarted = false;							// Flag for once manager has checked everything first

//Manager Globals
int totalRevenue;

// Customer Code

// init all values and assemble into groups
void customerInit(int groups[], int numGroups) 
{
  int currIndex = 0;
  totalCustomers = 0;
  totalGroups = numGroups;
  int randNum = 0;
  
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
      customers[j].seatRow = -1;
      customers[j].seatCol = -1;
      customers[j].hasPopcorn = false;
      customers[j].hasSoda = false;
      customers[j].needsRestroom = false;
      customers[j].totalPopcorns = 0;
      customers[j].totalSodas = 0;
      randNum = rand()%100;

      if(randNum < 75) {
        customers[j].wantsPopcorn = true;
      } else {
        customers[j].wantsPopcorn = false;
      }
      randNum = rand()%100;
      if(randNum < 75) {
        customers[j].wantsSoda = true;
      } else {
        customers[j].wantsSoda = false;
      }
    }
    currIndex += groups[i];
    totalCustomers += groups[i];
  }
  customerLobbyLock = new Lock("C_LOBBY_LOCK");
  customerLobbyCV = new Condition("C_LOBBY_CV");
  for(int i=0; i<totalGroups; i++) {
    waitingOnGroupLock[i] = new Lock("C_WAIT_LOCK");
    waitingOnGroupCV[i] = new Condition("WAIT_CV");
  }
  DEBUG('p', "\n");
}

// Bring a group of Customers back from the lobby

void doBuyTickets(int custIndex, int groupIndex) 
{
  int myTicketClerk;
  bool findNewLine;

  do {
    myTicketClerk = -1;
    findNewLine = false;
		DEBUG('p', "cust%i: Acquiring ticketClerkLineLock\n", custIndex);
    ticketClerkLineLock->Acquire();
    //See if any TicketClerk not busy
    for(int i=0; i<MAX_TC; i++) 
    {
      if(ticketClerkState[i] == 0) {   //TODO: should use enum BUSY={0,1, other states (e.g. on break?)}
        //Found a clerk who's not busy
        myTicketClerk = i;             // and now you belong to me
        ticketClerkState[i] = 1;
        printf("Customer %i in Group %i is walking up to TicketClerk%i to buy %i tickets.\n", 
        custIndex, groupIndex, myTicketClerk, groupSize[groupIndex]);
        DEBUG('p', "cust%i: talking to tc%i.\n", custIndex, myTicketClerk);
        break;
      }
    }
  
    // All ticketClerks were occupied, find the shortest line instead
    if(myTicketClerk == -1) 
      {
	myTicketClerk = 0; // Default back-up value, in case all ticketClerks on break

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
	if(ticketClerkIsWorking[myTicketClerk] == false) {
	  findNewLine = true;
	  printf("Customer %i in Group %i sees TicketClerk %i is on break.\n", custIndex, groupIndex, myTicketClerk);
	  ticketClerkLineLock->Release();
	}
      }
  } while(findNewLine);

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

// Give tickets to the ticketTaker,and either go into theater or lobby/repeat
void doGiveTickets(int custIndex, int groupIndex)
{
  int myTicketTaker;
  bool findNewLine;

  do {
    myTicketTaker = -1;
    findNewLine = false;
    ticketTakerLineLock->Acquire();
    //See if any ticketTaker not busy
    for(int i=0; i<MAX_TT; i++) {
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
    if(myTicketTaker == -1) {
      DEBUG('p', "cust%i: all tt's occupied, looking for shortest line.\n", custIndex);
      int shortestTTLine = 0;     // default the first line to current shortest
      int shortestTTLineLength = ticketTakerLineCount[0];
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
      if(ticketTakerIsWorking[myTicketTaker] == false) {
        findNewLine = true;
        printf("Customer %i in Group %i sees TicketTaker %i is on break.\n", custIndex, groupIndex, myTicketTaker);
        ticketTakerLineLock->Release();
      }
    }
  } while(findNewLine);

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
    
    ticketTakerCV[myTicketTaker]->Signal(ticketTakerLock[myTicketTaker]);       // You're free to carry on noble ticketTaker
    ticketTakerLock[myTicketTaker]->Release();                                  // Fly free ticketClerk. You were a noble friend.

    // Go chill in the lobby until customer's allowed in
    customerLobbyLock->Acquire();
    customerLobbyCV->Wait(customerLobbyLock);
    customerLobbyLock->Release();
    
    // Try this step again now that the group is free from the lobby's terrible grasp
    doGiveTickets(custIndex, groupIndex);
    return;
  } else {
    // Allowed into theater
    ticketTakerCV[myTicketTaker]->Signal(ticketTakerLock[myTicketTaker]);       // You're free to carry on noble ticketClerk
		ticketTakerLock[myTicketTaker]->Release();                                  // Fly free ticketClerk. You were a noble friend.
	}
}

// Customer sitting in position [row, col]
void choseSeat(int custIndex, int row, int col) {
  customers[custIndex].seatRow = row+1; // defaults rows from 1-5 as opposed to 0-4
  customers[custIndex].seatCol = col;

  printf("Customer %i in Group %i has found the following seat: row %i and seat %i\n",
	 custIndex, customers[custIndex].group, row+1, col);

  numSeatsOccupied++;
}

// Try to find ideal seats in the theater
void doChooseSeats(int custIndex, int groupIndex)
{
  int gSize = groupSize[groupIndex];
  DEBUG('p', "Customer %i, Group %i choosing seats.\n", custIndex, groupIndex);
  // Try to find a row with enough free seats
  for(int i=0; i<NUM_ROWS; i++) {
    if(freeSeatsInRow[i] >= gSize) {
      // enough consecutive seats in one row
      for(int j=custIndex; j<(custIndex+gSize); j++) {
        // Seating a customer
        choseSeat(j, i, freeSeatsInRow[i]);
        freeSeatsInRow[i]--;
      }
      return;
    }
  }
  
  // Find two consecutive rows to sit in?
  for(int i=1; i<NUM_ROWS; i++) {
    if((freeSeatsInRow[i-1] + freeSeatsInRow[i]) >= gSize) {
      int toBeSeated = gSize;
      int freeInFirstRow = freeSeatsInRow[i-1];
      for(int j=0; j<freeInFirstRow; j++) {
        // Seating a customer
        choseSeat(custIndex+j, i-1, freeSeatsInRow[i-1]);
        freeSeatsInRow[i-1]--;
        toBeSeated--;
      }
      for(int j=0; j<toBeSeated; j++) {
        // Seating remaining customers in group
        choseSeat((custIndex+freeInFirstRow+j), i, freeSeatsInRow[i]);
        freeSeatsInRow[i];
      }
      return;
    }
  }
  
  // Just find somewhere to sit before someone gets mad
  for(int i=0; i<NUM_ROWS; i++) {
    int toBeSeated = gSize;
    if(freeSeatsInRow[i] > 0) {
      for(int j=0; j<freeSeatsInRow[i]; j++) {
        // Sit customer down here
        choseSeat(custIndex+(gSize-toBeSeated), i, freeSeatsInRow[i]-j);
        toBeSeated--;
        if(toBeSeated == 0) {
          freeSeatsInRow[i] -= j+1;
          return;
        }
      }
      freeSeatsInRow[i] = 0;
    }
  }
}

void takeFoodOrders(int custIndex) 
{
  int popcorn = 0;
  int soda = 0;

  for(int i=0; i<totalCustomers; i++) {
    if(customers[custIndex].group == customers[i].group) {
	if(customers[i].wantsPopcorn == true) {
	  customers[custIndex].totalPopcorns++;
	  popcorn++;
	}
	if(customers[i].wantsSoda == true) {
	  customers[custIndex].totalSodas++;
	  soda++;
	}
    }
    if((popcorn==1) || (soda==1)) {
      printf("Customer %i in Group %i has %i popcorn and %i soda requests from a group member.\n", custIndex, customers[custIndex].group, popcorn, soda);
    }
    popcorn = 0;
    soda = 0;
  }
}

void doBuyFood(int custIndex, int groupIndex)
{
  int myConcessionClerk;
  bool findNewLine;

  do {
    myConcessionClerk = -1;
    findNewLine = false;
    concessionClerkLineLock->Acquire();
    // See if any concessionClerks are not busy
    for(int i = 0; i < MAX_CC; i++) {
      if(concessionClerkState[i] == 0) {
        // Found a clerk who's not busy
        myConcessionClerk = i;
        concessionClerkState[i] = 1;
        break;
      }
    }
    
    // All concessionClerks were occupied, find the shortest line instead
    if(myConcessionClerk == -1) {
      DEBUG('p', "cust%i: all cc's occupied, looking for shortest line.\n", custIndex);
      int shortestCCLine = 0; // default the first line to current shortest
      int shortestCCLineLength = concessionClerkLineCount[0];
      for(int i=1; i<MAX_CC; i++) {
        if((concessionClerkState[i] == 1) && (shortestCCLineLength > concessionClerkLineCount[i])) {
          // current line is shorter
          shortestCCLine = i;
          shortestCCLineLength = concessionClerkLineCount[i];
        }
      }

      // Found the ConcessionClerk with the shortest line
      myConcessionClerk = shortestCCLine;
      printf("Customer %i in Group %i is getting in ConcessionClerk line %i.\n", custIndex, customers[custIndex].group, myConcessionClerk);
      
      // Get in the shortest line
      concessionClerkLineCount[myConcessionClerk]++;
      concessionClerkLineCV[myConcessionClerk]->Wait(concessionClerkLineLock);
      if(concessionClerkIsWorking[myConcessionClerk] == false) {
        findNewLine = true;
        printf("Concession Clerk %i is on break. Finding a new line.\n", myConcessionClerk);
        concessionClerkLineLock->Release();
      }
    }
  } while(findNewLine);
  
  concessionClerkLineLock->Release();
  // Done with going to ConcessionClerk or getting in line. Now sleep.

  // ConcessionClerk has acknowledged you. TIme to wake up and talk to him.
  concessionClerkLock[myConcessionClerk]->Acquire();
  printf("Customer %i in Group %i is walking up to ConcessionClerk %i to buy %i popcorn and %i soda.\n", 
      custIndex, customers[custIndex].group, myConcessionClerk, customers[custIndex].totalPopcorns, customers[custIndex].totalSodas);
  numPopcornsOrdered[myConcessionClerk] = customers[custIndex].totalPopcorns;
  concessionClerkCV[myConcessionClerk]->Signal(concessionClerkLock[myConcessionClerk]);

  // Waiting for ConcessionClerk to acknowledge mypopocorn order
  concessionClerkCV[myConcessionClerk]->Wait(concessionClerkLock[myConcessionClerk]);

  // Telling ConcessionClerk my soda order
  DEBUG('p', "cust%i: cc has acknowledged my popcorn order, saying I need %i sodas.\n", custIndex, customers[custIndex].totalSodas);
  numSodasOrdered[myConcessionClerk] = customers[custIndex].totalSodas;
  concessionClerkCV[myConcessionClerk]->Signal(concessionClerkLock[myConcessionClerk]);

  // Waiting for ConcessionClerk to pass me my order
  concessionClerkCV[myConcessionClerk]->Wait(concessionClerkLock[myConcessionClerk]);

  // Take food
  for(int i=0; i<totalCustomers; i++) {
    if((customers[custIndex].group == customers[i].group) && (customers[i].wantsPopcorn == true)) {
      customers[i].hasPopcorn = true;
    }
    if((customers[custIndex].group == customers[i].group) && (customers[i].wantsSoda == true)) {
      customers[i].hasSoda = true;
    }
  }
  concessionClerkCV[myConcessionClerk]->Signal(concessionClerkLock[myConcessionClerk]);

  // Waiting for concessionClerk to tell me how much I owe
  concessionClerkCV[myConcessionClerk]->Wait(concessionClerkLock[myConcessionClerk]);
  
  // Hand concession clerk money
  customers[custIndex].money -= amountOwed[myConcessionClerk];
  concessionClerkCV[myConcessionClerk]->Signal(concessionClerkLock[myConcessionClerk]);
  printf("Customer %i in Group %i in ConcessionClerk line %i is paying %i for food.\n",
      custIndex, customers[custIndex].group, myConcessionClerk, amountOwed[myConcessionClerk]);

  // Wait for concessionClerk to take money
  concessionClerkCV[myConcessionClerk]->Wait(concessionClerkLock[myConcessionClerk]);

  // Leave the counter
  printf("Customer %i in Group %i is leaving ConcessionClerk %i.\n", custIndex, customers[custIndex].group, myConcessionClerk);
  concessionClerkCV[myConcessionClerk]->Signal(concessionClerkLock[myConcessionClerk]);
  concessionClerkLock[myConcessionClerk]->Release();
}

// Customers returning from bathroom break
void doReturnFromRestroom(int myIndex) {
  int groupIndex = customers[myIndex].group;

  waitingOnGroupLock[groupIndex]->Acquire();
  printf("Customer %i in Group %i is leaving the bathroom.\n", myIndex, groupIndex);
  DEBUG('p', "Customer %i is telling Group %i that I'm back from the restroom. \n", myIndex, groupIndex);
  customers[myIndex].needsRestroom = false;
  waitingOnGroupCV[groupIndex]->Signal(waitingOnGroupLock[groupIndex]);
  waitingOnGroupLock[groupIndex]->Release();
} 

// Leaving theater, going to bathroom
void doLeaveTheaterAndUseRestroom(int custIndex, int groupIndex) {
  
  int randNum;
  Thread *t;
  int numInRestroom = 0;
  bool forked = false;

  for(int i=custIndex; i<custIndex+groupSize[groupIndex]; i++) {
    randNum = rand()%100; 
    if(randNum < 25) {
      customers[i].needsRestroom = true;
    }
  }

  for(int i=custIndex; i<custIndex+groupSize[groupIndex]; i++) {
    if(customers[i].needsRestroom == true) {
      printf("Customer %i in Group %i is going to the bathroom.\n", i, groupIndex);
      numSeatsOccupied--;
      numInRestroom++;
    } else {
      printf("Customer %i in Group %i is in the lobby.\n", i, groupIndex);
      numSeatsOccupied--;
    }
  }
    
  waitingOnGroupLock[groupIndex]->Acquire();

  for(int i=custIndex; i<custIndex+groupSize[groupIndex]; i++) {
    if(customers[i].needsRestroom == true) {
      DEBUG('p', "Forking new thread for Customer %i in Group %i.\n", i, groupIndex);
      t = new Thread("c");
      t->Fork((VoidFunctionPtr)doReturnFromRestroom, i);
    }
  }
  
  while(numInRestroom > 0) {
    waitingOnGroupCV[groupIndex]->Wait(waitingOnGroupLock[groupIndex]);
    numInRestroom = 0;
    for(int i=custIndex; i<custIndex+groupSize[groupIndex]; i++) {
      if(customers[i].needsRestroom == true) {
        numInRestroom++;
      }
    }
    DEBUG('p', "%i members in Group %i left in the restroom.\n", numInRestroom, groupIndex);
  }
  waitingOnGroupLock[groupIndex]->Release();
}

// Defines behavior routine for all groups of customers
void groupHead(int custIndex) 
{
  int groupIndex = customers[custIndex].group;
  
  // Buy tickets from ticketClerk if you're a groupHead
  
  DEBUG('p', "cust%i: Begin to buy movie tickets.\n", custIndex);
  // Buy tickets from ticketClerk
  doBuyTickets(custIndex, groupIndex);

  takeFoodOrders(custIndex);
  if((customers[custIndex].totalSodas > 0) || (customers[custIndex].totalPopcorns > 0)) {
    doBuyFood(custIndex, groupIndex);
  }

  // Give tickets to ticketTaker
  doGiveTickets(custIndex, groupIndex);
 
  // Go find seats in the theater
  doChooseSeats(custIndex, groupIndex);

  // Wait for MovieTechnician to tell you movie is over
  DEBUG('p',"Customer %i is acquiring movieFinishedLock.\n", custIndex);
  movieFinishedLock->Acquire();
  DEBUG('p',"Customer %i is waiting for the movie to finish.\n", custIndex);
  movieFinishedLockCV->Wait(movieFinishedLock);
  DEBUG('p',"Customer %i knows the movie is over.\n", custIndex);
  movieFinishedLock->Release();
 
  // Leave theater and use restroom
  doLeaveTheaterAndUseRestroom(custIndex, groupIndex);

  // Counter to see how many customers have left movies
  totalCustomersServed += groupSize[groupIndex];
	DEBUG('p', "Customers served: %i\n", totalCustomersServed);
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
    // TicketClerk is on break, go away.
    if(ticketClerkState[myIndex] == 2) {
      printf("TicketClerk %i is going on break.\n", myIndex);

      ticketClerkLineLock->Acquire();
      ticketClerkIsWorking[myIndex] = false;
      ticketClerkLineCV[myIndex]->Broadcast(ticketClerkLineLock);
      ticketClerkLineLock->Release();

      ticketClerkBreakLock->Acquire();
      ticketClerkBreakCV->Wait(ticketClerkBreakLock);

      ticketClerkLineLock->Acquire();
      ticketClerkState[myIndex]=0;
      ticketClerkIsWorking[myIndex] = true;
      ticketClerkLineLock->Release();

      ticketClerkBreakLock->Release();
      printf("TicketClerk %i is coming off break.\n", myIndex);
    }

    //Is there a customer in my line?
    DEBUG('p', "tc%i: Acquiring ticketClerkLineLock.\n", myIndex);
    ticketClerkLineLock->Acquire();
    if(ticketClerkLineCount[myIndex] > 0) {	// There's a customer in my line
      ticketClerkState[myIndex] = 1;        // I must be busy, decrement line count
      ticketClerkLineCount[myIndex]--;
      printf("TicketClerk %i has a line length %i and is signaling a customer.\n", 
          myIndex, ticketClerkLineCount[myIndex]+1);
      ticketClerkLineCV[myIndex]->Signal(ticketClerkLineLock); // Wake up 1 customer
    } else {
      //No one in my line
      printf("TicketClerk %i has no one in line. I am available for a customer.\n", myIndex);
      ticketClerkState[myIndex] = 0;
    }
    
    DEBUG('p', "tc%i: Acquiring lock on self.\n", myIndex);
    ticketClerkLineLock->Release();
		ticketClerkLock[myIndex]->Acquire();                            // Call dibs on this current ticketClerk for a while
    
    
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
    
  }
}

// CONCESSION CLERK
void concessionClerk(int myIndex) {
  
  while(true) {

    if(concessionClerkState[myIndex] == 2) {
      printf("ConcessionClerk %i is going on break.\n", myIndex);
      concessionClerkLineLock->Acquire();
      concessionClerkIsWorking[myIndex] = false;
      concessionClerkLineCV[myIndex]->Broadcast(concessionClerkLineLock);
      concessionClerkLineLock->Release();

      concessionClerkBreakLock->Acquire();
      concessionClerkBreakCV->Wait(concessionClerkBreakLock);

      concessionClerkLineLock->Acquire();
      concessionClerkState[myIndex]=0;
      concessionClerkIsWorking[myIndex] = true;
      concessionClerkLineLock->Release();

      concessionClerkBreakLock->Release();
      printf("ConcessionClerk %i is coming off break.\n", myIndex);
    }
    
    // Is there a customer in my line?
    concessionClerkLineLock->Acquire();

    if(concessionClerkLineCount[myIndex] > 0) {   // There's a customer in my line
      concessionClerkState[myIndex] = 1;          // I must be busy   
      
      printf("ConcessionClerk %i has a line length %i and is signaling a customer.\n", myIndex, concessionClerkLineCount[myIndex]);
      concessionClerkLineCount[myIndex]--;
      concessionClerkLineCV[myIndex]->Signal(concessionClerkLineLock); // Wake up 1 customer
    } else {
      //No one in my line
      concessionClerkState[myIndex] = 0;
    }
    
    concessionClerkLock[myIndex]->Acquire();
    concessionClerkLineLock->Release();
    
    //Wait for Customer to come to my counter and tell me their popcorn order
    concessionClerkCV[myIndex]->Wait(concessionClerkLock[myIndex]);

    concessionClerkCV[myIndex]->Signal(concessionClerkLock[myIndex]);
    
    //Wait for customer to tell me their soda order
    concessionClerkCV[myIndex]->Wait(concessionClerkLock[myIndex]);

    DEBUG('p',"cc%i: Got customer's soda order of %i sodas.\n", myIndex, numSodasOrdered[myIndex]);
    DEBUG('p',"cc%i: Passing customer %i popcorns.\n", myIndex, numPopcornsOrdered[myIndex]);
    DEBUG('p',"cc%i: Passing customer %i sodas.\n", myIndex, numSodasOrdered[myIndex]);

    // Wake up customer so they can take food
    concessionClerkCV[myIndex]->Signal(concessionClerkLock[myIndex]);

    // Wait for customer to take food
    concessionClerkCV[myIndex]->Wait(concessionClerkLock[myIndex]);

    // Calculate price
    amountOwed[myIndex] = numPopcornsOrdered[myIndex]*POPCORN_PRICE + numSodasOrdered[myIndex]*SODA_PRICE;
    printf("ConcessionClerk %i has an order of %i popcorn and %i soda. The cost is %i.\n",
        myIndex, numPopcornsOrdered[myIndex], numSodasOrdered[myIndex], amountOwed[myIndex]);

    // Tell customer how much they owe me
    concessionClerkCV[myIndex]->Signal(concessionClerkLock[myIndex]);

    // Wait for customer to give me money
    concessionClerkCV[myIndex]->Wait(concessionClerkLock[myIndex]);

    // Take Money
    concessionClerkRegister[myIndex] += amountOwed[myIndex];
    printf("ConcessionClerk %i has been paid for the order.\n", myIndex);

    // Wait for customer to leave counter
    concessionClerkCV[myIndex]->Signal(concessionClerkLock[myIndex]);
    concessionClerkCV[myIndex]->Wait(concessionClerkLock[myIndex]);
    concessionClerkLock[myIndex]->Release();
  }
}

// TICKET TAKER
void ticketTaker(int myIndex)
{

  while(true)
  {
    // TicketTaker is on break, go away.
    if(ticketTakerState[myIndex] == 2) {
      printf("TicketTaker %i is going on break.\n", myIndex);
      ticketTakerLineLock->Acquire();
      ticketTakerIsWorking[myIndex] = false;
      ticketTakerLineCV[myIndex]->Broadcast(ticketTakerLineLock);
      ticketTakerLineLock->Release();

      ticketTakerBreakLock->Acquire();
      ticketTakerBreakCV->Wait(ticketTakerBreakLock);

      ticketTakerLineLock->Acquire();
      ticketTakerState[myIndex] = 0;
      ticketTakerIsWorking[myIndex] = true;
      ticketTakerLineLock->Release();
      
      ticketTakerBreakLock->Release();
      printf("TicketTaker %i is coming off break.\n", myIndex);
    }

    //Is there a customer in my line?
    ticketTakerLineLock->Acquire();
   
    if(ticketTakerLineCount[myIndex] > 0) {	// There's a customer in my line
      ticketTakerState[myIndex] = 1;        // I must be busy, decrement line count
      ticketTakerLineCount[myIndex]--;
      printf("TicketTaker %i has a line length %i and is signaling a customer.\n", 
          myIndex, ticketTakerLineCount[myIndex]+1);
      ticketTakerLineCV[myIndex]->Signal(ticketTakerLineLock); // Wake up 1 customer
    } else {
      //No one in my line
      printf("TicketTaker %i has no one in line. I am available for a customer.\n", myIndex);
      ticketTakerState[myIndex] = 0;
    }
    ticketTakerLock[myIndex]->Acquire();
    ticketTakerLineLock->Release();
    
    //Wait for Customer to tell me how many tickets he has
    ticketTakerCV[myIndex]->Wait(ticketTakerLock[myIndex]);
       
    if((numTicketsReceived[myIndex] > (MAX_SEATS-totalTicketsTaken)) || movieStarted)
    {
      // Movie has either started or is too full
      if(movieStarted) 
        printf("TicketTaker %i has stopped taking tickets.\n", myIndex);
      else {
        printf("TicketTaker %i is not allowing the group into the theater. The number of taken tickets is %i and the group size is %i.\n", 
            myIndex, totalTicketsTaken, numTicketsReceived[myIndex]);
      }
        
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
		ticketTakerLock[myIndex]->Release();
    // TicketTaker is done with customer, allows them to move into the theater
  }
}



// MANAGER
void manager(int myIndex)
{
  int pastTotal = 0;
  totalRevenue = 0;
  bool ticketTakerWorkNow = false;
  bool ticketClerkWorkNow = false;
  bool concessionClerkWorkNow = false;

  while(true)
  {
    //Put Employee on Break
    DEBUG('p', "Manager: Checking for employees to go on break. \n");
  	
    //TicketTaker Break
    DEBUG('p', "Entered TicketTaker Break Section\n");
    for(int i = 0; i < MAX_TT; i++)		//Put TicketTaker on break 
    {
      ticketTakerLineLock->Acquire();
      DEBUG('p', "Manager: Acquiring ticketTakerLineLock %i to check line length of 0. \n", i);
      if(ticketTakerLineCount[i]==0 && ticketTakerState[i]!=2)
  	  {
        DEBUG('p', "Manager: TicketTaker%i has no one in line. \n", i);
        if(rand() % 5 == 0)
    		{
     		  ticketTakerLock[i]->Acquire();
     		  ticketTakerState[i] = 2;
  				printf("Manager has told TicketTaker %i to go on break.\n", i);
     		  ticketTakerLock[i]->Release();
    		}
				ticketTakerLineLock->Release();
				break;
  	  }
  	  else if(ticketTakerWorking > 0)
  	  {
  	  	ticketTakerLock[i]->Acquire();
	    	if(rand() % 5 == 0)
	    	{
	    	  DEBUG('p', "Manager: TicketTaker%i is going to take a break since another employee is working. \n", i);
					printf("Manager has told TicketTaker %i to go on break.\n", i);
	    	  ticketTakerState[i] = 2;
	    	}
 		    ticketTakerLock[i]->Release();
				ticketTakerLineLock->Release();
  	  }
  	  else ticketTakerLineLock->Release();
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
					printf("Manager has told ConcessionClerk %i to go on break.\n", i);
    		  concessionClerkLock[i]->Release();
    		}
				concessionClerkLineLock->Release();
				break;
  	  }
  	  else
  	  {
  	  	concessionClerkLock[i]->Acquire();
  	    if(concessionClerkWorking > 0)
	      {
  	    	if(rand() % 5 == 0)
  	    	{
  	    	  DEBUG('p', "Manager: concessionClerk%i is going to take a break since another employee is working. \n", i);
						printf("Manager has told ConcessionClerk %i to go on break.\n", i);
  	    	  concessionClerkState[i] = 2;
  	    	}
  	    concessionClerkLock[i]->Release();
  	  	}
				concessionClerkLineLock->Release();
  	  }
	  }	
  		  	
  	//TicketClerk Break
  	for(int i = 0; i < MAX_TC; i++)		//Put TicketClerk on break 
  	{
  	  ticketClerkLineLock->Acquire();
  	  DEBUG('p', "Manager: Acquiring ticketClerkLineLock %i to check line empty: line length is %i. \n", i, ticketClerkLineCount[i]);
  	  if(ticketClerkLineCount[i]==0&&ticketClerkState[i]!=2)
  	  {
  	    DEBUG('p', "Manager: ticketClerk%i has no one in line. \n", i);
  	    if(rand() % 5 == 0)
  			{
					DEBUG('p', "Manager: ticketClerk%i is going to take a break. \n", i);
					ticketClerkLock[i]->Acquire();
					printf("Manager has told TicketClerk %i to go on break. \n",i);
					ticketClerkState[i] = 2;			//State as 2 means to take a break
					ticketClerkLock[i]->Release();
  			}
				ticketClerkLineLock->Release();
				break;
  	  } else 
			{
        ticketClerkLock[i]->Acquire();
   	    if(ticketClerkWorking > 0)
        {
   	    	if(rand() % 5 == 0)
   	    	{
   	    	  DEBUG('p', "Manager: ticketClerk%i is going to take a break since another employee is working. \n", i);
   	    	  ticketClerkState[i] = 2;
  					printf("Manager has told TicketClerk %i to go on break. \n",i);
   	    	}
   	    ticketClerkLock[i]->Release();
   	  	}
				DEBUG('p', "Manager: Releasing TicketClerkLineLock.\n");
				ticketClerkLineLock->Release();
   	  }
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
				break;
			}
			else if(ticketTakerWorking == 0 && ticketTakerLineCount[0] > 0){
				DEBUG('p', "Manager: Found a line without an employee, grabbing an employee to come off break. \n",i);
				break;
			}
			ticketTakerLineLock->Release();
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
				break;
			}
			ticketClerkLineLock->Release();
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
				break;
			}
			concessionClerkLineLock->Release();
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
    DEBUG('p', "MovieStatus = %i.\n", movieStatus);
    if(movieStatus == 2 && theaterStarted)		//Stopped
    {
			DEBUG('p', "TotalTicketsTaken = %i\n", totalTicketsTaken);
			DEBUG('p', "NumSeatsOccupied = %i\n", numSeatsOccupied);
    	if(totalTicketsTaken == 0 && numSeatsOccupied == 0) {
    		// Tell TicketTakers to take tickets
    		movieStarted = false;
    		ticketTakerMovieLock->Acquire();
    		DEBUG('p', "Manager: Waking up all ticketTakers!\n");
    		ticketTakerMovieCV->Broadcast(ticketTakerMovieLock);
    		ticketTakerMovieLock->Release();
    			
    		// Tell Customers to go to theater
    		customerLobbyLock->Acquire();
    		DEBUG('p', "Manager: Waking up all customers!\n");
    		customerLobbyCV->Broadcast(customerLobbyLock);
    		customerLobbyLock->Release();
    
       	// TODO: Pause for random movie starting
       	for(int i=0; i<30; i++) {
       		currentThread->Yield();
       	}

       	// Set movie ready to start
       	movieStarted = true;
       	movieStatusLock->Acquire();
       	movieStatus = 0;
       	movieStatusLockCV->Signal(movieStatusLock);
       	movieStatusLock->Release(); 
     	}
    }
			
		//Check clerk money levels
		pastTotal = totalRevenue;
		for(int i = 0; i<MAX_CC; i++)
		{
			concessionClerkLock[i]->Acquire();
			totalRevenue += concessionClerkRegister[i];

			printf("Manager collected %i from ConcessionClerk%i.\n", concessionClerkRegister[i], i);
			concessionClerkRegister[i] = 0;
			concessionClerkLock[i]->Release();
		}
		for(int i = 0; i<MAX_TC; i++)
		{
			ticketClerkLock[i]->Acquire();
			totalRevenue += ticketClerkRegister[i];
			printf("Manager collected %i from TicketClerk%i.\n", ticketClerkRegister[i], i); 
			ticketClerkRegister[i] = 0;

			ticketClerkLock[i]->Release();
		}
		DEBUG('p', "Manager: Total amount of money in theater is %i \n", totalRevenue);
		printf("Total money made by office = %i\n", totalRevenue);
		//TODO: Check theater sim complete
		if(totalCustomersServed == totalCustomers){
			DEBUG('p', "\n\nManager: Everyone is happy and has left. Closing the theater.\n\n\n");
			theaterDone = true;
			break;
		}
		
		//Pause  ---   NOT SURE HOW LONG TO MAKE MANAGER YIELD - ryan
		for(int i=0; i<10; i++)
		{
			theaterStarted = true;
			currentThread->Yield();
		}
	}	
}	  

// MOVIE TECHNICIAN
void movieTech(int myIndex) {
  while(!theaterDone) {
    DEBUG('p',"Total Tickets Taken: %i. Num Seats Occupied: %i.\n", totalTicketsTaken, numSeatsOccupied);
    if(movieStatus == 0 && (totalTicketsTaken == numSeatsOccupied) && numSeatsOccupied!=0) {
      DEBUG('p', "MOVIE TECH ATTEMPTING TO START MOVIE\n");
      movieStatusLock->Acquire();
      movieStatus = 1;
      totalTicketsTaken = 0;
      movieStatusLock->Release();
      printf("The MovieTechnician has started the movie.\n");

      movieLength = rand()%100 + 200; // Random number between 200 and 300
      while(movieLength > 0) {
        currentThread->Yield();
        DEBUG('p', "MOVIE IS PLAYING,%i\n", movieLength);
	      movieLength--;
      }

      movieStatusLock->Acquire();
      movieStatus = 2;
      movieStatusLock->Release();

      printf("The MovieTechnician has ended the movie.\n");

      DEBUG('p', "MT is acquiring movieFInishedLock\n");
      movieFinishedLock->Acquire();
      DEBUG('p', "MT is broadcasing to customers\n");
      movieFinishedLockCV->Broadcast(movieFinishedLock);
      movieFinishedLock->Release();
      
      // Free all seats for next movie
      for(int i=0; i<NUM_ROWS; i++)
        freeSeatsInRow[i] = NUM_COLS;
        
      printf("The MovieTechnician has told all customers to leave the theater room.\n");
    }

    if(movieStatus == 0) {
      currentThread->Yield();
    } else {
      DEBUG('p',"MovieTech is acquiring movieStatusLock.\n");
      movieStatusLock->Acquire();
      DEBUG('p', "MovieTech is waiting for Manager to say start movie.\n");
      movieStatusLockCV->Wait(movieStatusLock);  // Wait until manager says it's time to start the movie
      DEBUG('p', "MovieTech has been told by Manager to start movie.\n");
      movieStatusLock->Release();
    } 
  }
}

// Initialize values and players in this theater
void init_values() {
  DEBUG('p', "Initializing values and players in the movie theater.\n");
  
  totalCustomersServed = 0;

  for(int i=0; i<NUM_ROWS; i++)
  {
    freeSeatsInRow[i] = NUM_COLS;
  }
	
  for(int i=0; i<NUM_ROWS; i++)
  {
    freeSeatsInRow[i] = NUM_COLS;
  }
	
	// Initialize ticketClerk values
	ticketClerkLineLock = new Lock("TC_LINELOCK");
	ticketClerkBreakLock = new Lock("TC_BREAKLOCK");
	ticketClerkBreakCV = new Condition("TC_BREAKCV");
		for(int i=0; i<MAX_TC; i++) 
	{
	  ticketClerkLineCV[i] = new Condition("TC_lineCV");		// instantiate line condition variables
	  ticketClerkLock[i] = new Lock("TC_LOCK");
	  ticketClerkCV[i] = new Condition("TC_CV");
	  ticketClerkIsWorking[i] = true;
	}
	
	// Initialize ticketTaker values
	ticketTakerLineLock = new Lock("TT_LINELOCK");
	ticketTakerBreakLock = new Lock("TT_BREAKLOCK");
	ticketTakerBreakCV = new Condition("TT_BREAKCV");
	ticketTakerMovieLock = new Lock("TT_MOVIELOCK");
	ticketTakerMovieCV = new Condition("TT_MOVIECV");
	movieStarted = false;

	for(int i=0; i<MAX_TT; i++)
	{
	  ticketTakerLineCV[i] = new Condition("TT_lineCV");
	  ticketTakerLock[i] = new Lock("TT_LOCK");
	  ticketTakerCV[i] = new Condition("TT_CV");
	  ticketTakerIsWorking[i] = true;
	}
	
	// Initialize concessionClerk values
	concessionClerkLineLock = new Lock("CC_LINELOCK");
	concessionClerkBreakLock = new Lock("CC_BREAKLOCK");
	concessionClerkBreakCV = new Condition("CC_BREAKCV");
	concessionClerkWorking = MAX_CC;
	for(int i=0; i<MAX_CC; i++) {
	  concessionClerkLineCV[i] = new Condition("CC_LINECV"); // instantiate line condition variables
	  concessionClerkLock[i] = new Lock("CC_LOCK");
	  concessionClerkCV[i] = new Condition("CC_CV");

	  concessionClerkLineCount[i] = 0;
	  concessionClerkState[i] = 0;
	  concessionClerkRegister[i] = 0;
	  amountOwed[i] = 0;
	  numPopcornsOrdered[i] = 0;
	  numSodasOrdered[i] = 0;
	  concessionClerkIsWorking[i] = true;
	}
	
	// Initialize movieTechnician values
	movieStatus = 2; // Movie starts in the "finished" state
	movieLength = 0;
	movieFinishedLockCV = new Condition("MFL_CV");
	movieStatusLockCV = new Condition("MSL_CV");
	movieFinishedLock = new Lock("MFL");
	movieStatusLock = new Lock("MS_LOCK");
	numSeatsOccupied = 0; 
	
	// Initialize manager values
	totalRevenue = 0;
}

// Initialize players in this theater
void init() {
  DEBUG('p', "Initializing values and players in the movie theater.\n");
	init_values();
  
	Thread *t;
	
  /*
  int custsLeftToAssign = MAX_CUST;
  int aGroups[MAX_CUST];
  int aNumGroups = 0;
  while(custsLeftToAssign > 0) {
    int addNum = rand()%5+1;      // Random number of customers from 1-5
    if(addNum > custsLeftToAssign)
      addNum = custsLeftToAssign;
    aGroups[aNumGroups] = addNum;
    aNumGroups++;
    custsLeftToAssign -= addNum;
  }
  */
  
	int aGroups[] = {3, 1, 4, 5, 3, 4, 2, 1, 5, 2, 3};
  int aNumGroups = len(aGroups);
	for(int i=0; i<MAX_TC; i++) 
	{ 
	  // Fork off a new thread for a ticketClerk
	  DEBUG('p', "Forking new thread: ticketClerk%i\n", i);
	  t = new Thread("tc");
	  t->Fork((VoidFunctionPtr)ticketClerk,i);
	}
	
  // Initialize customers
  customerInit(aGroups, aNumGroups);
	for(int i=0; i<aNumGroups; i++) 
	{
	  // Fork off a new thread for a customer
	  DEBUG('p', "Forking new thread: customerGroup%i\n", groupHeads[i]);
	  t = new Thread("cust");
	  t->Fork((VoidFunctionPtr)groupHead,groupHeads[i]);
	}

	for(int i=0; i<MAX_TT; i++)
	{
	  // Fork off a new thread for a ticketTaker
	  DEBUG('p', "Forking new thread: ticketTaker%i\n", i);
	  t = new Thread("tt");
	  t->Fork((VoidFunctionPtr)ticketTaker, i);
	}
	
	for(int i=0; i<MAX_CC; i++) {
	  // Fork off a new thread for a concessionClerk
	  DEBUG('p', "Forking new thread: concessionClerk%i\n", i);
	  t = new Thread("cc");
	  t->Fork((VoidFunctionPtr)concessionClerk, i);
	}
  
	DEBUG('p', "Forking new thread: movieTech.\n");
	t = new Thread("mt");
	t->Fork((VoidFunctionPtr)movieTech, 1);
         
	DEBUG ('p', "Forking new thread: manager\n");
	DEBUG('p', "We have this many customers: %i\n", totalCustomers);
	t = new Thread("man");
	t->Fork((VoidFunctionPtr)manager,0);  
}

//Temporary to check if makefile works
void Theater_Sim() {
  DEBUG('p', "Beginning movie theater simulation.\n");
  
	init();
	return;
}

