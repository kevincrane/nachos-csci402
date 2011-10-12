/* theater_sim.cc
   All relevant agent-like methods governing the interactions within the
   movie theater simulation.
*/

#include "syscall.h"

/* CONSTANTS */
#define MAX_CUST 35        /* constant: maximum number of customers                */
#define MAX_TC 5            /* constant: defines maximum number of ticketClerks     */
#define MAX_TT 3            /* constant: defines maximum number of ticketTakers     */
#define MAX_CC 5            /* constant: defines maximum number of concessionClerks */
#define MAX_SEATS 25        /* constant: max number of seats in the theater         */
#define NUM_ROWS 5          /* constant: number of rows in the theater              */
#define NUM_COLS 5          /* constant: number of seats/row                        */

#define TICKET_PRICE 12     /* constant: price of a movie ticket */
#define POPCORN_PRICE 5     /* constant: price of popcorn        */
#define SODA_PRICE 4        /* constant: price of soda           */

#define len(x) (sizeof (x) / sizeof *(x))   /* used for finding the length of arrays */

/* Customer Global variables */
typedef struct {
  int   isLeader;           /* Is this customer the group leader? */
  int   index;              /* Index in customers[] array         */
  int   group;              /* Which group am I?                  */
  int   money;              /* Amount of money carried            */
  int   numTickets;         /* Number of tickets carried          */
  int   seatRow;            /* Seat row                           */
  int   seatCol;            /* Seat column                        */
  int   hasSoda;            /* Do I have soda?                    */
  int   hasPopcorn;         /* Do I have popcorn?                 */
  int   wantsPopcorn;       /* Do I want popcorn?                 */
  int   wantsSoda;          /* Do I want soda?                    */
  int   needsRestroom;      /* Do I have to use the restroom?     */
  int   totalPopcorns;      /* Num popcorns the group wants       */
  int   totalSodas;         /* Num sodas the group wants          */
} CustomerStruct;

CustomerStruct customers[MAX_CUST];       /* List of all customers in theater                                */
int totalCustomers;                       /* total number of customers there actually are                    */
int totalCustomersServed;	       	  /* total number of customers that finished                         */
int totalGroups;                          /* total number of groups there actually are                       */
int freeSeatsInRow[NUM_ROWS];             /* seating arangements in theater                                  */
int customerLobbyLock;                    /* Lock for when cust in lobby                                     */
int customerLobbyCV;                      /* CV for when cust in lobby                                       */
int waitingOnGroupLock[MAX_CUST];         /* Lock for when cust is waiting for group members in the restroom */
int waitingOnGroupCV[MAX_CUST];          
int chooseSeatsLock;

/* Group Global variables */
int groupHeads[MAX_CUST];                 /* List of indices pointing to the leaders of each group */
int groupSize[MAX_CUST];                  /* The size of each group                                */


/* TicketClerk Global variables */
int   ticketClerkState[MAX_TC];           /* Current state of a ticketClerk                                */
int   ticketClerkLock[MAX_TC];            /* array of Locks, each corresponding to one of the TicketClerks */
int   ticketClerkCV[MAX_TC];              /* condition variable assigned to particular TicketClerk         */
int   ticketClerkLineCount[MAX_TC];       /* How many people in each ticketClerk's line?                   */
int   numberOfTicketsNeeded[MAX_TC];      /* How many tickets does the current customer need?              */
int   amountOwedTickets[MAX_TC];          /* How much money the customer owes for tickets                  */
int   ticketClerkRegister[MAX_TC];        /* amount of money the ticketClerk has collected                 */
int   ticketClerkWorking;                 /* Number of Ticket Clerks working                               */

int   ticketClerkLineLock;                /* Lock for line of customers                                    */
int   ticketClerkLineCV[MAX_TC];	  /* Condition variable assigned to particular line of customers   */
int   ticketClerkBreakLock[MAX_TC];	  /* Lock for when TC goes on break                                */
int   ticketClerkBreakCV[MAX_TC];	  /* Condition variable for when TC goes on break                  */
int   ticketClerkIsWorking[MAX_TC];

/* TicketTaker Global variables */
int   ticketTakerState[MAX_TT];           /* Current state of a ticketTaker                                */
int   ticketTakerLock[MAX_TT];            /* array of Locks, each corresponding to one of the ticketTakers */
int   ticketTakerCV[MAX_TT];              /* condition variable assigned to particular ticketTaker         */
int   ticketTakerLineCount[MAX_TT];       /* How many people in each ticketTaker's line?                   */

int   allowedIn[MAX_TT];                  /* Is this group allowed into the theater?                       */
int   numTicketsReceived[MAX_TT];         /* Number of tickets the customer gave                           */
int   totalTicketsTaken=0;                /* Total tickets taken for this movie                            */
int   theaterFull;                        /* Is the theater close enough to capacity?                      */
int   movieStarted;                       /* Has the movie started already?                                */

int ticketTakerWorking;                   /* Number of Ticket Takers currently working                     */
int ticketTakerLineLock;                  /* Lock for line of customers                                    */
int ticketTakerLineCV[MAX_TT];            /* Condition variable assigned to particular line of customers   */
int ticketTakerMovieLock;                 /* Lock for ticket takers to acquire for wait                    */
int ticketTakerMovieCV;                   /* Condition variable for wait                                   */
int ticketTakerBreakLock;                 /* Lock for when TT goes on break                                */
int ticketTakerBreakCV;                   /* Condition variable when TT goes on break                      */
int ticketTakerIsWorking[MAX_TT];
int ticketTakerCounterLock;

/* ConcessionClerk Global variables */
int concessionClerkLineLock;              /* lock for line of customers                                        */
int concessionClerkLock[MAX_CC];          /* array of locks, each corresponding to one of the concessionClerks */
int concessionClerkBreakLock[MAX_CC];     /* Lock for when CC goes on break                                    */
int concessionClerkLineCV[MAX_CC];        /* Condition variable assigned to each line                          */
int concessionClerkCV[MAX_CC];            /* Condition variable assigned to each concessionClerk               */
int concessionClerkBreakCV[MAX_CC];       /* Condition variable for when CC goes on break                      */

int concessionClerkLineCount[MAX_CC];     /* How many people are in each concessionClerk's line              */
int concessionClerkState[MAX_CC];         /* State of each concessionClerk, 0 = free, 1 = busy, 2 = on break */
int concessionClerkRegister[MAX_CC];      /* Cash register for each concessionClerk                          */
int concessionClerkWorking;
int amountOwed[MAX_CC];                   /* How much the customer being helped owes */
int numPopcornsOrdered[MAX_CC];           /* How many popcorns the customer wants    */
int numSodasOrdered[MAX_CC];              /* How many sodas the customer wants       */
int concessionClerkIsWorking[MAX_CC];     /* If the CC is not on break = true        */

/* MovieTechnician Global variables */
int numSeatsOccupied;
int movieStatusLock;                      /* Lock corresponding to the movieStatus variable shared between the movieTech and the manager */
int movieFinishedLock;                    /* Lock needed to signal customers when movie is finished */

int movieFinishedLockCV;                  /* Condition variable to signal customers when the movie is over */
int movieStatusLockCV;                    /* Condition variable to wait after the movie is over */

int movieStatus;                          /* 0 = need to start, 1 = playing, 2 = finished */
int movieLength;                          /* "Length" of the movie - a number of yields between 200 and 300 */
int theaterDone = 0;	  	          /* Flag for when the theater is finished */
int theaterStarted = 0;		          /* Flag for once manager has checked everything first */

int nextGroup = 0;
int nextTC = 0;
int nextCust = 0;
int nextCC = 0;
int nextTT = 0;

/* Manager Globals */
int totalRevenue;

/* MODIFIED FROM PROJECT 1 - might need to add a lock*/
int groups[MAX_CUST];


/* Customer Code */

/* init all values and assemble into groups */
void customerInit(int numGroups) 
{
  int currIndex = 0;
  int randNum = 0;
  int i;
  int j;
  int k;
  totalCustomers = 0;
  totalGroups = numGroups;
  
  /* Iterate through each of the groups passed in; each int is number of people in group */
  for(i=0; i<numGroups; i++) 
    {
      groupHeads[i] = currIndex;    /* Current index corresponds to a group leader */
      groupSize[i] = groups[i];     /* Number of people in current group*/
      for(j=currIndex; j<(currIndex+groups[i]); j++) 
	{
	  Print("Customer %i in Group %i has entered the movie theater\n", j, i, -1);
  
	  /* Initialize all values for current customer */
	  if(j == currIndex) {
	    customers[j].isLeader = 1;
	  } else {
	    customers[j].isLeader = 0;
	  }
	  customers[j].index = j;
	  customers[j].group = i;
	  customers[j].money = 120;
	  customers[j].numTickets = 0;
	  customers[j].seatRow = -1;
	  customers[j].seatCol = -1;
	  customers[j].hasPopcorn = 0;
	  customers[j].hasSoda = 0;
	  customers[j].needsRestroom = 0;
	  customers[j].totalPopcorns = 0;
	  customers[j].totalSodas = 0;
	  randNum = 100; /* ***************rand()%100;************* */
	  
	  if(randNum < 75) {
	    customers[j].wantsPopcorn = 1;
	  } else {
	    customers[j].wantsPopcorn = 0;
	  }
	  randNum = 100; /* **************rand()%100;********* */
	  if(randNum < 75) {
	    customers[j].wantsSoda = 1;
	  } else {
	    customers[j].wantsSoda = 0;
	  }
	}
      currIndex += groups[i];
      totalCustomers += groups[i];
      
    }
  customerLobbyLock = CreateLock();
  customerLobbyCV = CreateCV();
  for(k=0; k<totalGroups; k++) {
    waitingOnGroupLock[k] = CreateLock();
    waitingOnGroupCV[k] = CreateCV();
  }    
}

/* Bring a group of Customers back from the lobby */

void doBuyTickets(int custIndex, int groupIndex) 
{
  int i;
  int j;
  int k;
  int shortestTCLine;
  int shortestTCLineLength;
  int myTicketClerk = 200;
  int findNewLine = 0;

  Print("Customer %i entered doBuyTickets.\n", custIndex, -1, -1);

  /* Buy tickets from ticketClerk if you're a groupHead */
  
  /* Buy tickets from */
/*  do { */
    myTicketClerk = 200;
    findNewLine = 0;

    Print("Customer %i is aquiring lock %i.\n", custIndex, ticketClerkLineLock, -1);
    Acquire(ticketClerkLineLock);
    Print("My TicketClerk is %i.\n", myTicketClerk, -1, -1);
    /* See if any TicketClerk not busy */
    for(i=0; i<MAX_TC; i++) {
      if(ticketClerkState[i] == 0) {
        /* Found a clerk who's not busy */
        myTicketClerk = i;             /* and now you belong to me */
        ticketClerkState[i] = 1;
        Print("Customer %i in Group %i is walking up to TicketClerk %i ", custIndex, groupIndex, myTicketClerk);
        Print("to buy %i tickets.\n", groupSize[groupIndex], -1, -1);
        break;
      }
    }
  
    /* All ticketClerks were occupied, find the shortest line instead */
    Print("My TicketClerk is %i.\n", myTicketClerk, -1, -1);
    if(myTicketClerk == 200) {
      Print("Customer %i did not find a free ticket clerk.\n", custIndex, -1, -1);
      shortestTCLine = 0;     /* default the first line to current shortest */
      shortestTCLineLength = 100000;
      for(j=0; j<MAX_TC; j++) {
        if((ticketClerkState[j] == 1) && (shortestTCLineLength > ticketClerkLineCount[j])) {
          /* current line is shorter */
          shortestTCLine = j;
          shortestTCLineLength = ticketClerkLineCount[j];
        }
      }
    
      /* Found the TicketClerk with the shortest line */
      myTicketClerk = shortestTCLine;
      /*      Print("", -1, -1, -1);*/
      Print("Customer %i in Group %i is getting in TicketClerk line %i.\n", custIndex, groupIndex, myTicketClerk);
      
      /* Get in the shortest line */
      ticketClerkLineCount[myTicketClerk]++;
      Print("Customer %i is using CV %i to wait on lock %i.\n", custIndex, ticketClerkLineCV[myTicketClerk], ticketClerkLineLock);
      Wait(ticketClerkLineCV[myTicketClerk], ticketClerkLineLock);
      if((ticketClerkIsWorking[myTicketClerk] == 0) || (ticketClerkState[myTicketClerk] == 2)) {
        findNewLine = 1;
        Print("Customer %i in Group %i sees TicketClerk %i is on break.\n", custIndex, groupIndex, myTicketClerk);
        Release(ticketClerkLineLock);
      }
    }
    Print("abcdefghi=%i\n", findNewLine, -1, -1);
/*  } while(findNewLine == 1); */

  Print("Customer %i is releasing lock %i.\n", custIndex, ticketClerkLineLock, -1);
  Release(ticketClerkLineLock);
  /* Done with either going moving to TicketClerk or getting in line. Now sleep. */

  /* TicketClerk has acknowledged you. Time to wake up and talk to him.*/
  Print("Customer %i is acquiring lock %i.\n", custIndex, ticketClerkLock[myTicketClerk], -1);
  Acquire(ticketClerkLock[myTicketClerk]);

  numberOfTicketsNeeded[myTicketClerk] = groupSize[groupIndex];

  Print("Customer %i is using CV %i to signal lock %i.\n", custIndex, ticketClerkCV[myTicketClerk], ticketClerkLock[myTicketClerk]);
  Signal(ticketClerkCV[myTicketClerk], ticketClerkLock[myTicketClerk]);
  /* Done asking for 'myGroupSize' tickets from TicketClerk. Now sleep.*/

  /* TicketClerk has returned a price for you */
  Print("Customer %i is using CV %i to wait on lock %i.\n", custIndex, ticketClerkCV[myTicketClerk], ticketClerkLock[myTicketClerk]);
  Wait(ticketClerkCV[myTicketClerk], ticketClerkLock[myTicketClerk]);

  customers[custIndex].money -= amountOwedTickets[myTicketClerk];               /* Pay the ticketClerk for the tickets */
  Print("Customer %i in Group %i in TicketClerk line %i ", custIndex, groupIndex, myTicketClerk);
  Print("is paying %i for tickets.\n", amountOwedTickets[myTicketClerk], -1, -1);
  Signal(ticketClerkCV[myTicketClerk], ticketClerkLock[myTicketClerk]);
  
  /* TicketClerk has given you n tickets. Goodies for you! */
  Wait(ticketClerkCV[myTicketClerk], ticketClerkLock[myTicketClerk]);
  customers[custIndex].numTickets += numberOfTicketsNeeded[myTicketClerk];      /* Receive tickets! */
  
  Print("Customer %i in Group %i is leaving TicketClerk %i.\n", custIndex, groupIndex, myTicketClerk);

  Signal(ticketClerkCV[myTicketClerk], ticketClerkLock[myTicketClerk]);         /* You're free to carry on noble ticketClerk */
  Release(ticketClerkLock[myTicketClerk]);                                      /* Fly free ticketClerk. You were a noble friend.*/

  /*doBuyTickets(custIndex, groupIndex);*/
  Print("Customer %i just bought tickets.\n", custIndex, -1, -1);

}

/* Give tickets to the ticketTaker,and either go into theater or lobby/repeat */
void doGiveTickets(int custIndex, int groupIndex)
{
  int myTicketTaker;
  int findNewLine;
  int i;
  int j;
  int k;

  do {
    myTicketTaker = -1;
    findNewLine = 0;
    Acquire(ticketTakerLineLock);
    /* See if any ticketTaker not busy */
    for(i=0; i<MAX_TT; i++) {
      if(ticketTakerState[i] == 0) {
        /* Found a clerk who's not busy */
        myTicketTaker = i;             /* and now you belong to me */
        ticketTakerState[i] = 1;
        Print("Customer %i in Group %i is getting in TicketTaker Line %i.\n", custIndex, groupIndex, myTicketTaker);
        break;
      }
    }
  
    /* All ticketTakers were occupied, find the shortest line instead */
    if(myTicketTaker == -1) {
      
      int shortestTTLine = 0;     /* default the first line to current shortest */
      int shortestTTLineLength = 1000000;
      for(j=0; j<MAX_TT; j++) {
        if((ticketTakerState[j] == 1) && (shortestTTLineLength > ticketTakerLineCount[j])) {
          /* current line is shorter */
          shortestTTLine = j;
          shortestTTLineLength = ticketTakerLineCount[j];
        }
      }

      /* Found the TicketClerk with the shortest line */
      myTicketTaker = shortestTTLine;
      Print("Customer %i in Group %i is getting in TicketTaker line %i.\n", custIndex, groupIndex, myTicketTaker);
      
      /* Get in the shortest line */
      ticketTakerLineCount[myTicketTaker]++;
      Wait(ticketTakerLineCV[myTicketTaker], ticketTakerLineLock);
      if(ticketTakerState[myTicketTaker] == 2) { /*ticketTakerIsWorking[myTicketTaker] == 0)) {*/
        findNewLine = 1;
        ticketTakerLineCount[myTicketTaker]--;
        Print("Customer %i in Group %i sees TicketTaker %i ", custIndex, groupIndex, myTicketTaker);
        Print("is on break (state=%i).\n", ticketTakerState[myTicketTaker], -1, -1);
        Release(ticketTakerLineLock);
      }
    }
  } while(findNewLine);
  
  Release(ticketTakerLineLock);
  /* Done with either going moving to TicketTaker or getting in line. Now sleep. */
  
  /* TicketTaker has acknowledged you. Tell him how many tickets you have */
  Acquire(ticketTakerLock[myTicketTaker]);
  numTicketsReceived[myTicketTaker] = groupSize[groupIndex];
  
  Print("Customer %i in Group %i is walking up to TicketTaker %i ", custIndex, groupIndex, myTicketTaker);
  Print("to give %i tickets.\n", numTicketsReceived[myTicketTaker], -1, -1);
 
  Signal(ticketTakerCV[myTicketTaker], ticketTakerLock[myTicketTaker]);
  
  /* Did the TicketTaker let you in? */
  Wait(ticketTakerCV[myTicketTaker], ticketTakerLock[myTicketTaker]);
  if(!allowedIn[myTicketTaker]) {
    /* Theater was either full or the movie had started */
    for(k=custIndex; k<custIndex+groupSize[groupIndex]; k++) {
      Print("Customer %i in Group %i is in the lobby.\n", k, groupIndex, -1);
	}
    Signal(ticketTakerCV[myTicketTaker], ticketTakerLock[myTicketTaker]);       /* You're free to carry on noble ticketTaker */
    Release(ticketTakerLock[myTicketTaker]);                                    /* Fly free ticketClerk. You were a noble friend. */

    /* Go chill in the lobby until customer's allowed in */
    Acquire(customerLobbyLock);
    Wait(customerLobbyCV, customerLobbyLock);
    Release(customerLobbyLock);
    
    /* Try this step again now that the group is free from the lobby's terrible grasp */
    doGiveTickets(custIndex, groupIndex);
    return;
  } else {
    /* Allowed into theater */
    Signal(ticketTakerCV[myTicketTaker], ticketTakerLock[myTicketTaker]);       /* You're free to carry on noble ticketTaker */
    Release(ticketTakerLock[myTicketTaker]);                                    /* Fly free ticketTaker. You were a noble friend. */
  }
}

/* Customer sitting in position [row, col] */
void choseSeat(int custIndex, int row, int col) {
  customers[custIndex].seatRow = row+1; /* defaults rows from 1-5 as opposed to 0-4 */
  customers[custIndex].seatCol = col;
 
  Print("Customer %i in Group %i has found the following seat: ", custIndex, customers[custIndex].group, -1);
  Print("row %i and seat %i.\n", customers[custIndex].seatRow, customers[custIndex].seatCol, -1);
 
  numSeatsOccupied++;
}

/* Try to find ideal seats in the theater */
void doChooseSeats(int custIndex, int groupIndex)
{
  int gSize = groupSize[groupIndex];
  int i;
  int j;

  /* Try to find a row with enough free seats */
  for(i=0; i<NUM_ROWS; i++) {
    if(freeSeatsInRow[i] >= gSize) {
      /* enough consecutive seats in one row */
      for(j=custIndex; j<(custIndex+gSize); j++) {
        /* Seating a customer */
        choseSeat(j, i, freeSeatsInRow[i]);
        freeSeatsInRow[i]--;
      }
      return;
    }
  }
  
  /* Find two consecutive rows to sit in? */
  for(i=1; i<NUM_ROWS; i++) {
    if((freeSeatsInRow[i-1] + freeSeatsInRow[i]) >= gSize) {
      int toBeSeated = gSize;
      int freeInFirstRow = freeSeatsInRow[i-1];
      for(j=0; j<freeInFirstRow; j++) {
        /* Seating a customer */
        choseSeat(custIndex+j, i-1, freeSeatsInRow[i-1]);
        freeSeatsInRow[i-1]--;
        toBeSeated--;
      }
      for(j=0; j<toBeSeated; j++) {
        /* Seating remaining customers in group */
        choseSeat((custIndex+freeInFirstRow+j), i, freeSeatsInRow[i]);
        freeSeatsInRow[i];
      }
      return;
    }
  }
  
  /* Just find somewhere to sit before someone gets mad */
  for(i=0; i<NUM_ROWS; i++) {
    int toBeSeated = gSize;
    if(freeSeatsInRow[i] > 0) {
      for(j=0; j<freeSeatsInRow[i]; j++) {
        /* Sit customer down here */
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
  
  /* You fool, you didn't sit down in time. Go wait in the lobby, idiot.*/
  Acquire(customerLobbyLock);
  Wait(customerLobbyCV, customerLobbyLock);
  Release(customerLobbyLock);
  /*  doChooseSeats(custIndex, groupIndex);*/
  doGiveTickets(custIndex, groupIndex);
}

void takeFoodOrders(int custIndex) 
{
  int popcorn = 0;
  int soda = 0;
  int i;

  for(i=0; i<totalCustomers; i++) {
    if(customers[custIndex].group == customers[i].group) {
      if(customers[i].wantsPopcorn == 1) {
        customers[custIndex].totalPopcorns++;
        popcorn++;
      }
      if(customers[i].wantsSoda == 1) {
        customers[custIndex].totalSodas++;
        soda++;
      }
    }
    if((popcorn==1) || (soda==1)) {
      Print("Customer %i in Group %i has %i popcorn", custIndex, customers[custIndex].group, popcorn);
      Print("and %i soda requests from a group member.\n", soda, -1, -1);
    }
    popcorn = 0;
    soda = 0;
  }
}

void doBuyFood(int custIndex, int groupIndex)
{
  int myConcessionClerk;
  int findNewLine;
  int i;
  
  do {
    myConcessionClerk = -1;
    findNewLine = 0;
    Acquire(concessionClerkLineLock);
    /* See if any concessionClerks are not busy */
    for(i = 0; i < MAX_CC; i++) {
      if(concessionClerkState[i] == 0) {
        /* Found a clerk who's not busy */
        myConcessionClerk = i;
        concessionClerkState[i] = 1;
        break;
      }
    }
    
    /* All concessionClerks were occupied, find the shortest line instead */
    if(myConcessionClerk == -1) {
      
      int shortestCCLine = 0; /* default the first line to current shortest */
      int shortestCCLineLength = concessionClerkLineCount[0];
      for(i=1; i<MAX_CC; i++) {
        if((concessionClerkState[i] == 1) && (shortestCCLineLength > concessionClerkLineCount[i])) {
          /* current line is shorter */
          shortestCCLine = i;
          shortestCCLineLength = concessionClerkLineCount[i];
        }
      }
      
      /* Found the ConcessionClerk with the shortest line */
      myConcessionClerk = shortestCCLine;
      Print("Customer %i in Group %i is getting in ConcessionClerk line %i.\n", custIndex, customers[custIndex].group, myConcessionClerk);
      
      /* Get in the shortest line */
      concessionClerkLineCount[myConcessionClerk]++;
      Wait(concessionClerkLineCV[myConcessionClerk], concessionClerkLineLock);
      if(concessionClerkIsWorking[myConcessionClerk] == 0 || concessionClerkState[myConcessionClerk] == 2) {
        findNewLine = 1;
        Print("Customer %i in Group %i sees ConcessionClerk %i is on break.\n", custIndex, groupIndex, myConcessionClerk);
        Release(concessionClerkLineLock);
      }
    }
  } while(findNewLine);
  
  Release(concessionClerkLineLock);
  /* Done with going to ConcessionClerk or getting in line. Now sleep.*/
  
  /* ConcessionClerk has acknowledged you. TIme to wake up and talk to him. */
  Acquire(concessionClerkLock[myConcessionClerk]);
  Print("Customer %i in Group %i is walking up to ConcessionClerk %i ", custIndex, customers[custIndex].group, myConcessionClerk);
  Print("to buy %i popcorn and %i soda.\n", customers[custIndex].totalPopcorns, customers[custIndex].totalSodas, -1);

  numPopcornsOrdered[myConcessionClerk] = customers[custIndex].totalPopcorns;
  Signal(concessionClerkCV[myConcessionClerk], concessionClerkLock[myConcessionClerk]);
  
  /* Waiting for ConcessionClerk to acknowledge mypopocorn order */
  Wait(concessionClerkCV[myConcessionClerk], concessionClerkLock[myConcessionClerk]);
  
  /* Telling ConcessionClerk my soda order */
  numSodasOrdered[myConcessionClerk] = customers[custIndex].totalSodas;
  Signal(concessionClerkCV[myConcessionClerk], concessionClerkLock[myConcessionClerk]);

  /* Waiting for ConcessionClerk to pass me my order */
  Wait(concessionClerkCV[myConcessionClerk], concessionClerkLock[myConcessionClerk]);

  /* Take food */
  for(i=0; i<totalCustomers; i++) {
    if((customers[custIndex].group == customers[i].group) && (customers[i].wantsPopcorn == 1)) {
      customers[i].hasPopcorn = 1;
    }
    if((customers[custIndex].group == customers[i].group) && (customers[i].wantsSoda == 1)) {
      customers[i].hasSoda = 1;
    }
  }
  Signal(concessionClerkCV[myConcessionClerk], concessionClerkLock[myConcessionClerk]);

  /* Waiting for concessionClerk to tell me how much I owe */
  Wait(concessionClerkCV[myConcessionClerk], concessionClerkLock[myConcessionClerk]);
  
  /* Hand concession clerk money */
  customers[custIndex].money -= amountOwed[myConcessionClerk];
  Signal(concessionClerkCV[myConcessionClerk], concessionClerkLock[myConcessionClerk]);

  Print("Customer %i in Group %i in ConcessionClerk line %i", custIndex, customers[custIndex].group, myConcessionClerk);
  Print(" is paying %i for food.\n", amountOwed[myConcessionClerk], -1, -1);

  /* Wait for concessionClerk to take money */
  Wait(concessionClerkCV[myConcessionClerk], concessionClerkLock[myConcessionClerk]);

  /* Leave the counter */
  Print("Customer %i in Group %i is leaving ConcessionClerk %i.\n", custIndex, customers[custIndex].group, myConcessionClerk);

  Signal(concessionClerkCV[myConcessionClerk], concessionClerkLock[myConcessionClerk]);
  Release(concessionClerkLock[myConcessionClerk]);
}

/* Customers returning from bathroom break */
void doReturnFromRestroom(int myIndex) {
  int groupIndex = customers[myIndex].group;

  Acquire(waitingOnGroupLock[groupIndex]);
  Print("Customer %i in Group %i is leaving the bathroom.\n", myIndex, groupIndex, -1);

  customers[myIndex].needsRestroom = 0;
  Signal(waitingOnGroupCV[groupIndex], waitingOnGroupLock[groupIndex]);
  Release(waitingOnGroupLock[groupIndex]);
  Exit(0);
} 

/* Leaving theater, going to bathroom */
void doLeaveTheaterAndUseRestroom(int custIndex, int groupIndex) {
  
  int randNum;
  int numInRestroom = 0;
  int forked = 0;
  int i;
  
  for(i=custIndex; i<custIndex+groupSize[groupIndex]; i++) {
    randNum = 30; /* ************rand()%100;********** */ 
    if(randNum < 25) {
      customers[i].needsRestroom = 1;
    }
  }

  for(i=custIndex; i<custIndex+groupSize[groupIndex]; i++) {
    if(customers[i].needsRestroom == 1) {
      Print("Customer %i in Group %i is going to the bathroom and has left the theater.\n", i, groupIndex, -1);
      numInRestroom++;
    } else {
      Print("Customer %i in Group %i is in the lobby and has left the theater.\n", i, groupIndex, -1);
    }
  }
    
  Acquire(waitingOnGroupLock[groupIndex]);
  
  for(i=custIndex; i<custIndex+groupSize[groupIndex]; i++) {
    if(customers[i].needsRestroom == 1) {
      Fork((void*)doReturnFromRestroom);
    }
  }
  
  while(numInRestroom > 0) {
    Wait(waitingOnGroupCV[groupIndex], waitingOnGroupLock[groupIndex]);
    numInRestroom = 0;
    for(i=custIndex; i<custIndex+groupSize[groupIndex]; i++) {
      if(customers[i].needsRestroom == 1) {
        numInRestroom++;
      }
    }
  }
}

/* Defines behavior routine for all groups of customers */
void groupHead(int custIndex) 
{
  int i;
  int groupIndex = nextGroup++;
  custIndex = groupHeads[nextGroup];

  /*int groupIndex = customers[custIndex].group;*/
  
  doBuyTickets(custIndex, groupIndex);
  
  Print("Customer %i entering takeFoodOrders.\n", custIndex, -1, -1);
  takeFoodOrders(custIndex);
  
  Print("Customer %i just took food orders.\n", custIndex, -1, -1);
  if((customers[custIndex].totalSodas > 0)||(customers[custIndex].totalPopcorns > 0)) {
    doBuyFood(custIndex, groupIndex);
  }
  
  /* Give tickets to ticketTaker */
  doGiveTickets(custIndex, groupIndex);
  
  /* Go find seats in the theater */
  Acquire(chooseSeatsLock);
  doChooseSeats(custIndex, groupIndex);
  Release(chooseSeatsLock);
  
  /* Wait for MovieTechnician to tell you movie is over */
  Acquire(movieFinishedLock);
  Wait(movieFinishedLockCV, movieFinishedLock);
  Release(movieFinishedLock);
  
  /* Leave theater and use restroom */
  doLeaveTheaterAndUseRestroom(custIndex, groupIndex);
  
  /* Counter to see how many customers have left movies */
  totalCustomersServed = totalCustomersServed + groupSize[groupIndex];
  
  for(i=custIndex; i<custIndex+groupSize[groupIndex]; i++)
    {
      Print("Customer %i in Group %i has left the movie theater.\n", i, customers[i].group, -1);
    }
  Exit(0);
}


/* TicketClerk */
/* -Thread that is run when Customer is interacting with TicketClerk to buy movie tickets */
void ticketClerk(int myIndex) 
{    
  int numberOfTicketsHeld;
  myIndex = nextTC++;

  while(1) 
    {
      /* TicketClerk is on break, go away. */
      if(ticketClerkState[myIndex] == 2) {

	Print("TicketClerk %i is going on break.\n", myIndex, -1, -1);
      
	Acquire(ticketClerkLineLock);
	Broadcast(ticketClerkLineCV[myIndex], ticketClerkLineLock);
	Release(ticketClerkLineLock);
      
	Acquire(ticketClerkBreakLock[myIndex]);
	Wait(ticketClerkBreakCV[myIndex], ticketClerkBreakLock[myIndex]);
	Release(ticketClerkBreakLock[myIndex]);

	Print("TicketClerk %i is coming off break.\n", myIndex, -1, -1);

	Acquire(ticketClerkLineLock);
	ticketClerkIsWorking[myIndex] = 1;
	ticketClerkState[myIndex] = 0;
	Release(ticketClerkLineLock);
      }

      /* Is there a customer in my line? */
      Acquire(ticketClerkLineLock);
      if(ticketClerkLineCount[myIndex] > 0) {	/* There's a customer in my line */

	ticketClerkState[myIndex] = 1;        /* I must be busy, decrement line count */
	ticketClerkLineCount[myIndex]--;

	Print("TicketClerk %i has a line length %i and is signaling a customer.\n", myIndex, ticketClerkLineCount[myIndex]+1, -1);

	Signal(ticketClerkLineCV[myIndex], ticketClerkLineLock); /* Wake up 1 customer */
      } else {
	/* No one in my line */
	Print("TicketClerk %i has no one in line. I am available for a customer.\n", myIndex, -1, -1);
	
	ticketClerkState[myIndex] = 0;
      }
      
      Release(ticketClerkLineLock);
      Acquire(ticketClerkLock[myIndex]);         /* Call dibs on this current ticketClerk for a while */
    
      /* If put on break, bail and wait */
      if((!ticketClerkIsWorking[myIndex]) && (ticketClerkState[myIndex] == 2)) {
        Release(ticketClerkLock[myIndex]);
        continue;
      }
      /* Wait for Customer to come to my counter and ask for tickets; tell him what he owes. */
      Wait(ticketClerkCV[myIndex], ticketClerkLock[myIndex]);
    
      amountOwedTickets[myIndex] = numberOfTicketsNeeded[myIndex] * TICKET_PRICE;   /* Tell customer how much money he owes */

      Print("TicketClerk %i has an order for %i and the cost is %i.\n", myIndex, numberOfTicketsNeeded[myIndex], amountOwedTickets[myIndex]);
      
      Signal(ticketClerkCV[myIndex], ticketClerkLock[myIndex]);
    
      /* Customer has given you money, give him the correct number of tickets, send that fool on his way */
      Wait(ticketClerkCV[myIndex], ticketClerkLock[myIndex]);
      ticketClerkRegister[myIndex] += amountOwedTickets[myIndex];     /* Add money to cash register */
      numberOfTicketsHeld = numberOfTicketsNeeded[myIndex];           /* Give customer his tickets */

      Signal(ticketClerkCV[myIndex], ticketClerkLock[myIndex]);
    
      Wait(ticketClerkCV[myIndex], ticketClerkLock[myIndex]);         /* Wait until customer is out of the way before beginning next one */
      Release(ticketClerkLock[myIndex]); /* Temp? */
    }
  Exit(0);
}

/* CONCESSION CLERK */
void concessionClerk() {
  int myIndex = nextCC++;
  Print("STARTED CONCESSIONSLUT %i.\n", myIndex, -1, -1);
  while(1) {

    if(concessionClerkState[myIndex] == 2) {
      Print("ConcessionClerk %i is going on break.\n", myIndex, -1, -1);
      
      Acquire(concessionClerkLineLock);
      Broadcast(concessionClerkLineCV[myIndex], concessionClerkLineLock);
      Release(concessionClerkLineLock);
      
      Acquire(concessionClerkBreakLock[myIndex]);
      Wait(concessionClerkBreakCV[myIndex], concessionClerkBreakLock[myIndex]);
      Release(concessionClerkBreakLock[myIndex]);
      
      Print("ConcessionClerk %i is coming off break.\n", myIndex, -1, -1);

      Acquire(concessionClerkLineLock);
      concessionClerkIsWorking[myIndex] = 1;
      concessionClerkState[myIndex] = 0;
      Release(concessionClerkLineLock);
    }
    
    /* Is there a customer in my line? */
    Acquire(concessionClerkLineLock);

    if(concessionClerkLineCount[myIndex] > 0) {   /* There's a customer in my line */
      concessionClerkState[myIndex] = 1;          /* I must be busy */  
      
      Print("ConcessionClerk %i has a line length %i and is signaling a customer.\n", myIndex, concessionClerkLineCount[myIndex], -1);

      concessionClerkLineCount[myIndex]--;
      Signal(concessionClerkLineCV[myIndex], concessionClerkLineLock); /* Wake up 1 customer */
    } else {
      /* No one in my line */
      Print("ConcessionClerk %i has no one in line. I am available for a customer.\n", myIndex, -1, -1);
      concessionClerkState[myIndex] = 0;
    }
    
    Acquire(concessionClerkLock[myIndex]);
    Release(concessionClerkLineLock);
    
    /* If put on break instead, bail and wait */
    if((!concessionClerkIsWorking[myIndex]) && (concessionClerkState[myIndex] == 2)) {
      Release(concessionClerkLock[myIndex]);
      continue;
    }
    
    /* Wait for Customer to come to my counter and tell me their popcorn order */
    Wait(concessionClerkCV[myIndex], concessionClerkLock[myIndex]);

    Signal(concessionClerkCV[myIndex], concessionClerkLock[myIndex]);
    
    /* Wait for customer to tell me their soda order */
    Wait(concessionClerkCV[myIndex], concessionClerkLock[myIndex]);
    
    /* Wake up customer so they can take food */
    Signal(concessionClerkCV[myIndex], concessionClerkLock[myIndex]);

    /* Wait for customer to take food */
    Wait(concessionClerkCV[myIndex], concessionClerkLock[myIndex]);

    /* Calculate price */
    amountOwed[myIndex] = numPopcornsOrdered[myIndex]*POPCORN_PRICE + numSodasOrdered[myIndex]*SODA_PRICE;
    Print("ConcessionClerk %i has an order of %i popcorn and ", myIndex, numPopcornsOrdered[myIndex], -1);
    Print("%i soda. The cost is %i.\n", numSodasOrdered[myIndex], amountOwed[myIndex], -1);

    /* Tell customer how much they owe me */
    Signal(concessionClerkCV[myIndex], concessionClerkLock[myIndex]);

    /* Wait for customer to give me money */
    Wait(concessionClerkCV[myIndex], concessionClerkLock[myIndex]);

    /* Take Money */
    concessionClerkRegister[myIndex] += amountOwed[myIndex];
    Print("ConcessionClerk %i has been paid for the order.\n", myIndex, -1, -1);
    
    /* Wait for customer to leave counter */
    Signal(concessionClerkCV[myIndex], concessionClerkLock[myIndex]);
    Wait(concessionClerkCV[myIndex], concessionClerkLock[myIndex]);
    Release(concessionClerkLock[myIndex]);
  }
  Exit(0);
}

/* TICKET TAKER */
void ticketTaker()
{
  int myIndex = nextTT++;
  while(1)
    {
      /* TicketTaker is on break, go away. */
      if(ticketTakerState[myIndex] == 2) {
        Print("TicketTaker %i is going on break.\n", myIndex, -1, -1);
            
        Acquire(ticketTakerLineLock);
        Broadcast(ticketTakerLineCV[myIndex], ticketTakerLineLock);
        Release(ticketTakerLineLock);
            
        Acquire(ticketTakerBreakLock);
        Wait(ticketTakerBreakCV, ticketTakerBreakLock);
        Release(ticketTakerBreakLock);

        Print("TicketTaker %i is coming off break.\n", myIndex, -1, -1);
        Acquire(ticketTakerLineLock);
        ticketTakerIsWorking[myIndex] = 1;
        ticketTakerState[myIndex] = 0;
        Release(ticketTakerLineLock);
      }

      /* Is there a customer in my line? */
      Acquire(ticketTakerLineLock);
      if(ticketTakerLineCount[myIndex] > 0) {	/* There's a customer in my line */
        ticketTakerState[myIndex] = 1;        /* I must be busy, decrement line count */
        ticketTakerLineCount[myIndex]--;
        Print("TicketTaker %i has a line length %i and is signaling a customer.\n", myIndex,ticketTakerLineCount[myIndex]+1, -1);

        Signal(ticketTakerLineCV[myIndex], ticketTakerLineLock); /* Wake up 1 customer */
      } else {
        /* No one in my line */
        Print("TicketTaker %i has no one in line. I am available for a customer.\n", myIndex, -1, -1);
        ticketTakerState[myIndex] = 0;
      }
      Acquire(ticketTakerLock[myIndex]);
      Release(ticketTakerLineLock);
    
      /* If put on break instead, bail and wait */
      if((!ticketTakerIsWorking[myIndex]) && (ticketTakerState[myIndex] == 2)) { /* numTicketsReceived[myIndex] < 0) { */
        Release(ticketTakerLock[myIndex]);
        continue;
      }
    
      /* Wait for Customer to tell me how many tickets he has */
      Wait(ticketTakerCV[myIndex], ticketTakerLock[myIndex]);
       
      Acquire(ticketTakerLineLock);
      Acquire(ticketTakerCounterLock);
      if((numTicketsReceived[myIndex] > (MAX_SEATS-totalTicketsTaken)) || theaterFull || movieStarted)
	{
	  /* Movie has either started or is too full */
	  if(theaterFull || movieStarted) {
	    Print("TicketTaker %i has stopped taking tickets.\n", myIndex, -1, -1);
	  }
	  
	  else {
	    theaterFull = 1;
      
      Print("TicketTaker %i is not allowing the group into the theater. ", myIndex, -1, -1);
      Print("The number of taken tickets is %i and the group size is %i.\n", totalTicketsTaken, numTicketsReceived[myIndex], -1);
	  }
    
	  allowedIn[myIndex] = 0;
	  Release(ticketTakerLineLock);
	  Release(ticketTakerCounterLock);
	  Signal(ticketTakerCV[myIndex], ticketTakerLock[myIndex]);
	  Wait(ticketTakerCV[myIndex], ticketTakerLock[myIndex]);
	  Release(ticketTakerLock[myIndex]);
	  continue;
	} else {
	  allowedIn[myIndex] = 1;
	  Release(ticketTakerLineLock);
	}
      Signal(ticketTakerCV[myIndex], ticketTakerLock[myIndex]);

      /*Wait for Customer to give me their tickets */
      Acquire(ticketTakerCounterLock); /* ************************************************** */
      Print("TicketTaker %i has received %i tickets.\n", myIndex, numTicketsReceived[myIndex], -1);

      totalTicketsTaken += numTicketsReceived[myIndex];
      
      if(totalCustomers-totalCustomersServed <= totalTicketsTaken) theaterFull = 1;
    
      Release(ticketTakerCounterLock);
      Wait(ticketTakerCV[myIndex], ticketTakerLock[myIndex]);
      Release(ticketTakerLock[myIndex]);
      /* TicketTaker is done with customer, allows them to move into the theater */
    }
  Exit(0);
}


/* MANAGER (SO NEW AND IMPROVED) */
void manager(int myIndex)
{
  int numTConBreak = 0;
  int numCConBreak = 0;
  int numTTonBreak = 0;
  int i;
  int j;
  int k;
  int offBreakCC;
  int offBreakTT;

  theaterFull = 0;
  totalRevenue = 0;
  
  /* Check for Employees to put  break */  
  while(1) {
    /* Checking TicketClerks to pull off break */
    int offBreakTC = 0;

    for(i=0; i<MAX_TC; i++) {
      /* Pull TC off break if line's long */
      Acquire(ticketClerkLineLock);
      if((ticketClerkLineCount[i] >= 5) && (numTConBreak >= 1) && (ticketClerkIsWorking[i])) {
	      for(j=0; j<MAX_TC; j++) {
          if((j == i) || (ticketClerkIsWorking[j])) continue;
          Acquire(ticketClerkLock[j]);
          ticketClerkState[j] = 0;          /* TicketClerk is off Break */

          numTConBreak--;
          
          Acquire(ticketClerkBreakLock[j]);
          Signal(ticketClerkBreakCV[j], ticketClerkBreakLock[j]);
          Release(ticketClerkBreakLock[j]);
          
          Release(ticketClerkLock[j]);
          offBreakTC = 1;
          break;
        }
      }
      Release(ticketClerkLineLock);
      /*     if(offBreakTC) break;       // Found someone to pull off their break */
    }
    
    /* Checking TicketClerks to put on break */
    Acquire(ticketClerkLineLock);
    for(i=0; i<MAX_TC; i++) {
      /* Potentially put TC on break if line is small and someone else is working */
      if(!ticketClerkIsWorking[i] && ticketClerkLineCount[i] > 0) {
        Broadcast(ticketClerkLineCV[myIndex], ticketClerkLineLock);
      }
      if((ticketClerkLineCount[i] < 3) && (ticketClerkState[i] != 2) && (MAX_TC-numTConBreak > 1)) {
        if(1 == 0) {
	/* if(rand()%5 == 0) { *************************** */
          Acquire(ticketClerkLock[i]);
          ticketClerkState[i] = 2;          /* TicketClerk is on Break */
          Print("Manager has told TicketClerk %i to go on break.\n", i, -1, -1);
          numTConBreak++;
          
          ticketClerkIsWorking[i] = 0;
          
          Signal(ticketClerkCV[i], ticketClerkLock[i]);
          Release(ticketClerkLock[i]);
          break;
        }
      }
    }
    Release(ticketClerkLineLock);
    
    /* Checking ConcessionClerks to pull off break */
    offBreakCC = 0;

    Acquire(concessionClerkLineLock);
    for(i=0; i<MAX_CC; i++) {
      /* Pull CC off break if line's long */
      if((concessionClerkLineCount[i] >= 5) && (numCConBreak >= 1) && (concessionClerkIsWorking[i])) {
        for(j=0; j<MAX_CC; j++) {
          if((j == i) || (concessionClerkIsWorking[j])) continue;
          Acquire(concessionClerkLock[j]);
          concessionClerkState[j] = 0;          /* ConcessionClerk is off Break */
          numCConBreak--;
          
          Acquire(concessionClerkBreakLock[j]);
          Signal(concessionClerkBreakCV[j], concessionClerkBreakLock[j]);
          Release(concessionClerkBreakLock[j]);
          
          Release(concessionClerkLock[j]);
          offBreakCC = 1;
          break;
        }
      }
      if(offBreakCC) break;       /* Found someone to pull off their break */
    }
    Release(concessionClerkLineLock);
    
    /* Checking ConcessionClerks to put on break */
    Acquire(concessionClerkLineLock);
    for(i=0; i<MAX_CC; i++) {
      if(!concessionClerkIsWorking[i] && concessionClerkLineCount[i] > 0) {
        Broadcast(concessionClerkLineCV[myIndex], concessionClerkLineLock);
      }
      /* Potentially put CC on break if line is small and someone else is working */
      if((concessionClerkLineCount[i] < 3) && (concessionClerkState[i] != 2) && (MAX_CC-numCConBreak > 1)) {
        if(1 == 0) {
	  /* if(rand()%5 == 0) { ******************** */
          Acquire(concessionClerkLock[i]);
          concessionClerkState[i] = 2;          /* ConcessionClerk is on Brea */
          Print("Manager has told ConcessionClerk %i to go on break.\n", i, -1, -1);
          numCConBreak++;
          
          concessionClerkIsWorking[i] = 0;
          
          Signal(concessionClerkCV[i], concessionClerkLock[i]);
          Release(concessionClerkLock[i]);
          break;
        }
      }
    }
    Release(concessionClerkLineLock);
    
    
    /* Checking TicketTakers to pull off break */
    offBreakTT = 0; /* ********************* added 0 ********************* */
    
    Acquire(ticketTakerLineLock);
    for(i=0; i<MAX_TT; i++) {
      /* Pull TT off break if line's long */
      if((ticketTakerLineCount[i] >= 5) && (numTTonBreak >= 1) && (ticketTakerIsWorking[i])) {
        for(j=0; j<MAX_TT; j++) {
          if((j == i) || (ticketTakerIsWorking[j])) continue;
          Acquire(ticketTakerLock[j]);
          ticketTakerState[j] = 0;          /* TicketTaker is off Break */
          ticketTakerIsWorking[j] = 1;

          numTTonBreak--;
          
          Acquire(ticketTakerBreakLock);
          Signal(ticketTakerBreakCV, ticketTakerBreakLock);
          Release(ticketTakerBreakLock);
          
          Release(ticketTakerLock[j]);
          offBreakTT = 1;
          break;
        }
      }
      /*      if(offBreakTT) break;       // Found someone to pull off their break */
    }
    Release(ticketTakerLineLock);
    
    /* Checking TicketTakers to put on break */
    Acquire(ticketTakerLineLock);
    for(i=0; i<MAX_TT; i++) {
      if(!ticketTakerIsWorking[i] && ticketTakerLineCount[i] > 0) {
        Broadcast(ticketTakerLineCV[myIndex], ticketTakerLineLock);
      }
      /* Potentially put TT on break if line is small and someone else is working */
      if((ticketTakerLineCount[i] < 3) && (ticketTakerState[i] != 2) && (MAX_TT-numTTonBreak > 1)) {
        if(1 == 0) {
	  /* if(rand()%5 == 0) { ********************************* */
          Acquire(ticketTakerLock[i]);
          ticketTakerState[i] = 2;          /* TicketTaker is on Break */
          Print("Manager has told TicketTaker %i to go on break.\n", i, -1, -1);

          numTTonBreak++;
          
          ticketTakerIsWorking[i] = 0;
          numTicketsReceived[i] = 0;
          
          Signal(ticketTakerCV[i], ticketTakerLock[i]);
          Release(ticketTakerLock[i]);
          break;
        }
      }
    }
    Release(ticketTakerLineLock);
		
    /* Check to start movie */

    if(movieStatus == 2)		/* Stopped */
    {
      if((theaterFull || (totalCustomers-(totalCustomersServed+numSeatsOccupied)) <= (MAX_SEATS-numSeatsOccupied))  && totalTicketsTaken != 0) { /*!theaterFull || (totalTicketsTaken == 0 && numSeatsOccupied == 0)) { */
	  
	  /* Set movie ready to start */
	  movieStarted = 1;
	  Acquire(movieStatusLock);
	  movieStatus = 0;
	  Signal(movieStatusLockCV, movieStatusLock);
	  Release(movieStatusLock); 
       	
	  /* TODO: Pause for random movie starting */
	  for(k=0; k<30; k++) {
	    Yield();
	  }
	  movieStarted = 0;
       	
     	} else Write("OH NOESSS\n", sizeof("OH NOESSS\n"), ConsoleOutput);
    }
    
    
    /* Check clerk money levels */
    for(i = 0; i<MAX_CC; i++)
    {
      Acquire(concessionClerkLock[i]);
      totalRevenue += concessionClerkRegister[i];

      Print("Manager collected %i from ConcessionClerk %i.\n", concessionClerkRegister[i], i, -1);
      concessionClerkRegister[i] = 0;
      Release(concessionClerkLock[i]);
    }

    for(i = 0; i<MAX_TC; i++)
    {
      Acquire(ticketClerkLock[i]);
      totalRevenue += ticketClerkRegister[i];
      Print("Manager collected %i from TicketClerk %i.\n", ticketClerkRegister[i], i, -1);

      ticketClerkRegister[i] = 0;

      Release(ticketClerkLock[i]);
    }
    Print("Total money made by office = %i.\n", totalRevenue, -1, -1);
		
    if(totalCustomersServed == totalCustomers){
      Print("\n\nManager: Everyone is happy and has left. Closing the theater.\n\n\n", -1, -1, -1);
      theaterDone = 1;
      break;
    }
		
    for(i=0; i<10; i++) {
      Yield();
    }
  }
  Exit(0);
}


/* MOVIE TECHNICIAN */
void movieTech(int myIndex) {
  int i;

  while(theaterDone == 0) {	
    if(theaterFull || (totalCustomers-(totalCustomersServed+numSeatsOccupied)) <= (MAX_SEATS-numSeatsOccupied) || 1) {
      Acquire(movieStatusLock);
      movieStatus = 1;
      Release(movieStatusLock);
      Print("The MovieTechnician has started the movie.\n", -1, -1, -1);

      movieLength = 50;
      while(movieLength > 0) {
        Yield();
	movieLength--;
      }

      Acquire(movieStatusLock);
      movieStatus = 2;
      Release(movieStatusLock);

      Print("The MovieTechnician has ended the movie.\n", -1, -1, -1);

      Acquire(movieFinishedLock);
      Broadcast(movieFinishedLockCV, movieFinishedLock);
      Release(movieFinishedLock);
      
      /* Free all seats for next movie */
      for(i=0; i<NUM_ROWS; i++)
        freeSeatsInRow[i] = NUM_COLS;
      
      theaterFull = 0;
      movieStarted = 0;
      numSeatsOccupied = 0;
      totalTicketsTaken = 0;
      
      /* Tell Customers to go to theater */
      if(totalCustomers != totalCustomersServed) {
        Acquire(customerLobbyLock);
        Broadcast(customerLobbyCV, customerLobbyLock);
        Release(customerLobbyLock);
      }
        
      Print("The MovieTechnician has told all customers to leave the theater room.\n", -1, -1, -1);
    }

    if(movieStatus == 0) {
      Yield();
    } else {
      Acquire(movieStatusLock);
      Wait(movieStatusLockCV, movieStatusLock);  /* Wait until manager says it's time to start the movie */
      Release(movieStatusLock);
    }
  }
  Exit(0);
}

/* Initialize variables in theater */
void init_values(){
  int i;
  totalCustomersServed = 0;

  ticketTakerWorking = MAX_TT;
  ticketClerkWorking = MAX_TC;
  concessionClerkWorking = MAX_CC;
	
  for(i=0; i<NUM_ROWS; i++)
  {
    freeSeatsInRow[i] = NUM_COLS;
  }
	
  for(i=0; i<NUM_ROWS; i++)
  {
    freeSeatsInRow[i] = NUM_COLS;
  }
	
  /* Initialize ticketClerk values */
  ticketClerkLineLock = CreateLock();
  for(i=0; i<MAX_TC; i++) 
  {
    ticketClerkLineCV[i] = CreateCV();		/* instantiate line condition variables */
    ticketClerkLock[i] = CreateLock();
    ticketClerkCV[i] = CreateCV();
    ticketClerkBreakLock[i] = CreateLock();
    ticketClerkBreakCV[i] = CreateCV();
    ticketClerkIsWorking[i] = 1;
  }
	
  /* Initialize ticketTaker values */
  ticketTakerLineLock = CreateLock();
  ticketTakerMovieLock = CreateLock();
  ticketTakerMovieCV = CreateCV();
  ticketTakerBreakLock = CreateLock();
  ticketTakerBreakCV = CreateCV();
  movieStarted = 0;
  for(i=0; i<MAX_TT; i++)
  {
    ticketTakerLineCV[i] = CreateCV();
    ticketTakerLock[i] = CreateLock();
    ticketTakerCV[i] = CreateCV();
    ticketTakerIsWorking[i] = 1;
  }
	
  /* Initialize concessionClerk values */
  concessionClerkLineLock = CreateLock();
  concessionClerkWorking = MAX_CC;
  for(i=0; i<MAX_CC; i++) {
    concessionClerkLineCV[i] = CreateCV(); /* instantiate line condition variables */
    concessionClerkLock[i] = CreateLock();
    concessionClerkCV[i] = CreateCV();
    concessionClerkBreakLock[i] = CreateLock();
    concessionClerkBreakCV[i] = CreateCV();
    
    concessionClerkLineCount[i] = 0;
    concessionClerkState[i] = 0;
    concessionClerkRegister[i] = 0;
    amountOwed[i] = 0;
    numPopcornsOrdered[i] = 0;
    numSodasOrdered[i] = 0;
    concessionClerkIsWorking[i] = 1;
  }
	
  /* Initialize movieTechnician values */
  movieStatus = 2; /* Movie starts in the "finished" state */
  movieLength = 0;
  movieFinishedLockCV = CreateCV();
  movieStatusLockCV = CreateCV();
  movieFinishedLock = CreateLock();
  movieStatusLock = CreateLock();
  numSeatsOccupied = 0; 
	
  /* Initialize manager values */
  totalRevenue = 0;
}

/* Initialize players in this theater */
void init() {
  int i;

  int chooseSeatsLock = CreateLock();
  int ticketTakerCounterLock = CreateLock();
  int custsLeftToAssign = MAX_CUST;
  int aNumGroups = 0;

  init_values();

  while(custsLeftToAssign > 0) {
    int addNum = 3;
    /* int addNum = Rand()%5+1;  Random number of customers from 1-5  ******************** */
    if(addNum > custsLeftToAssign) {
      addNum = custsLeftToAssign;
    }
    groups[aNumGroups] = addNum;
    aNumGroups++;
    custsLeftToAssign -= addNum;
  }
  
  /* Display for user how many of each theater player there are */
  Print("Number of Customers =        %i.\n", MAX_CUST, -1, -1);
  Print("Number of Groups =           %i.\n", aNumGroups, -1, -1);
  Print("Number of TicketClerks =     %i.\n", MAX_TC, -1, -1);
  Print("Number of ConcessionClerks = %i.\n", MAX_CC, -1, -1);
  Print("Number of TicketTakers =     %i.\n", MAX_TT, -1, -1);
  
  for(i=0; i<MAX_TC; i++) 
  { 
    /* Fork off a new thread for a ticketClerk */
    Fork((void*)ticketClerk);
  }
	
  /* Initialize customers */
  customerInit(aNumGroups);
  for(i=0; i<aNumGroups; i++) 
  {
    /* Fork off a new thread for a customer */
    Fork((void*)groupHead);
  }

  for(i=0; i<MAX_TT; i++)
  {
    /* Fork off a new thread for a ticketTaker */
    Fork((void*)ticketTaker);
  }
	
/*  for(i=0; i<MAX_CC; i++) {
    /* Fork off a new thread for a concessionClerk */
/*    Fork((void*)concessionClerk); 
  }*/
  
  Fork((void*)movieTech);
/*  Fork((void*)manager);*/

  Exit(0);
}

/* Temporary to check if makefile works */
int main() {
  Print("CSCI-402: Assignment 2\n", -1, -1, -1);
  
  init();
  return 0;
}



