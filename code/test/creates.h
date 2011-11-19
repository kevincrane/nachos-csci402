/* Keeps all the global values I need for the theater and does some initialization bullshit */

#ifndef CREATES_H
#define CREATES_H

#include "syscall.h"

/* CONSTANTS */
#define MAX_CUST 30        /* constant: maximum number of customers                */
#define MAX_TC 5            /* constant: defines maximum number of ticketClerks     */
#define MAX_TT 3            /* constant: defines maximum number of ticketTakers     */
#define MAX_CC 2            /* constant: defines maximum number of concessionClerks */
#define MAX_SEATS 25        /* constant: max number of seats in the theater         */
#define NUM_ROWS 5          /* constant: number of rows in the theater              */
#define NUM_COLS 5          /* constant: number of seats/row                        */

#define TICKET_PRICE 12     /* constant: price of a movie ticket */
#define POPCORN_PRICE 5     /* constant: price of popcorn        */
#define SODA_PRICE 4        /* constant: price of soda           */

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
int freeSeatsInRow;             /* seating arangements in theater                                  */
int customerLobbyLock;                    /* Lock for when cust in lobby                                     */
int customerLobbyCV;                      /* CV for when cust in lobby                                       */
int waitingOnGroupLock[MAX_CUST];         /* Lock for when cust is waiting for group members in the restroom */
int waitingOnGroupCV[MAX_CUST];          
int chooseSeatsLock;

/* Group Global variables */
int groupHeads;                 /* List of indices pointing to the leaders of each group */
int groupSize;                  /* The size of each group                                */


/* TicketClerk Global variables */
int   ticketClerkState;           /* Current state of a ticketClerk                                */
int   ticketClerkLock[MAX_TC];            /* array of Locks, each corresponding to one of the TicketClerks */
int   ticketClerkCV[MAX_TC];              /* condition variable assigned to particular TicketClerk         */
int   ticketClerkLineCount;       /* How many people in each ticketClerk's line?                   */
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
int   ticketTakerState;           /* Current state of a ticketTaker                                */
int   ticketTakerLock[MAX_TT];            /* array of Locks, each corresponding to one of the ticketTakers */
int   ticketTakerCV[MAX_TT];              /* condition variable assigned to particular ticketTaker         */
int   ticketTakerLineCount;       /* How many people in each ticketTaker's line?                   */

int   allowedIn;                  /* Is this group allowed into the theater?                       */
int   numTicketsReceived;         /* Number of tickets the customer gave                           */
int   totalTicketsTaken;                /* Total tickets taken for this movie                            */
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

int concessionClerkLineCount;     /* How many people are in each concessionClerk's line              */
int concessionClerkState;         /* State of each concessionClerk, 0 = free, 1 = busy, 2 = on break */
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

int nextGroup;
int nextTC;
int nextCust;
int nextCC;
int nextTT;

/* Manager Globals */
int totalRevenue;

/* MODIFIED FROM PROJECT 1 - might need to add a lock*/
int groups;


char* concat_str(char* str, int num) {
	char cnum = (char)(((int)'0')+num);
	int len = sizeof(str);
	str[len] = cnum;
	str[len+1] = '\0';
	return str;
}


/* init all values and assemble into groups */
void customerInit(int numGroups) 
{
  int currIndex = GetMV(totalCustomers, 0);
  int randNum = 0;
  int i;
  int j;
  int k;
  
  totalGroups = numGroups;
  
  /* Iterate through each of the groups passed in; each int is number of people in group */
/*  for(i=0; i<numGroups; i++) 
    {*/
    i = numGroups-1;
      SetMV(groupHeads, currIndex, i);    /* Current index corresponds to a group leader */
      SetMV(groupSize, GetMV(groups, i), i);     /* Number of people in current group*/
      for(j=currIndex; j<(currIndex+GetMV(groups, i)); j++) 
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
      currIndex += GetMV(groups, i);
      SetMV(totalCustomers, currIndex, 0); /*+ GetMV(groups, i);*/
      
/*    }*/
  customerLobbyLock = CreateLock("cll", 3); /* TODO Check if this is in for loop */
  customerLobbyCV = CreateCV("clcv", 4);
  for(k=0; k<totalGroups; k++) {
    waitingOnGroupLock[k] = CreateLock(concat_str("wogl",k), 5);
    waitingOnGroupCV[k] = CreateCV(concat_str("wogcv", k), 6);
  }
}

/* Initialize variables in theater */
void init_locks_and_stuff() {
  int i;
  SetMV(totalCustomersServed, 0, 0);

  ticketTakerWorking = MAX_TT;
  ticketClerkWorking = MAX_TC;
  concessionClerkWorking = MAX_CC;
	
  freeSeatsInRow = CreateMV("freeSeats", 9, NUM_ROWS);
  for(i=0; i<NUM_ROWS; i++)
    SetMV(freeSeatsInRow, NUM_COLS, i);
    
  /* Initialize ticketClerk values */
  ticketClerkState = CreateMV("tcState", 7, MAX_TC);
  ticketClerkLineCount = CreateMV("tcLineCount", 11, MAX_TC);
  nextTC = CreateMV("nextTC", 6, 1);
  SetMV(nextTC, 0, 0);
  ticketClerkLineLock = CreateLock("tcll", 4);
  for(i=0; i<MAX_TC; i++) 
  {
    ticketClerkLineCV[i] = CreateCV(concat_str("tclcv", i), 6);		/* instantiate line condition variables */
    ticketClerkLock[i] = CreateLock(concat_str("tcl", i), 4); /*********************CHANGE THIS******************************/
    ticketClerkCV[i] = CreateCV(concat_str("tccv", i), 5);
    ticketClerkBreakLock[i] = CreateLock(concat_str("tcbl", i), 5); /*****************************CHANGE THIST*********************/
    ticketClerkBreakCV[i] = CreateCV(concat_str("tcbcv", i), 6);
    ticketClerkIsWorking[i] = 1;
/*    SetMV(ticketClerkState, 0, i);*/
  }
	
  /* Initialize ticketTaker values */
  ticketTakerState = CreateMV("ttState", 7, MAX_TT);
  ticketTakerLineCount = CreateMV("ttLineCount", 11, MAX_TT);
  numTicketsReceived = CreateMV("ticketsReceived", 15, MAX_TT);
  allowedIn = CreateMV("allowedIn", 9, MAX_TT);
  nextTT = CreateMV("nextTT", 6, 1);
  SetMV(nextTT, 0, 0);
  ticketTakerLineLock = CreateLock("ttll", 4);
  ticketTakerMovieLock = CreateLock("ttml", 4);
  ticketTakerMovieCV = CreateCV("ttmcv", 5);
  ticketTakerBreakLock = CreateLock("ttbl", 4);
  ticketTakerBreakCV = CreateCV("ttbcv", 5);
  movieStarted = 0;
  for(i=0; i<MAX_TT; i++)
  {
    ticketTakerLineCV[i] = CreateCV(concat_str("ttlcv", i), 6);
    ticketTakerLock[i] = CreateLock(concat_str("ttl", i), 4); /****************************CHANGE THIS*****************************/
    ticketTakerCV[i] = CreateCV(concat_str("ttcv", i), 5);
    ticketTakerIsWorking[i] = 1;
/*    SetMV(ticketTakerState, 0, i);*/
  }
	
  /* Initialize concessionClerk values */
  concessionClerkState = CreateMV("ccState", 7, MAX_CC);
  concessionClerkLineCount = CreateMV("ccLineCount", 11, MAX_CC);
  nextCC = CreateMV("nextCC", 6, 1);
  SetMV(nextCC, 0, 0);
  concessionClerkLineLock = CreateLock("ccll", 4);
  concessionClerkWorking = MAX_CC;
  for(i=0; i<MAX_CC; i++) {
    concessionClerkLineCV[i] = CreateCV(concat_str("cclcv", i), 6); /* instantiate line condition variables */
    concessionClerkLock[i] = CreateLock(concat_str("ccl", i), 4); /***************************CHANGE THIS**************************/
    concessionClerkCV[i] = CreateCV(concat_str("cccv", i), 5);
    concessionClerkBreakLock[i] = CreateLock(concat_str("ccbl", i), 5); /**************************CHANGE THIS********************/
    concessionClerkBreakCV[i] = CreateCV(concat_str("ccbcv", i), 6);
    
/*    concessionClerkLineCount[i] = 0; */
/*    SetMV(concessionClerkState, 0, i);*/
    concessionClerkRegister[i] = 0;
    amountOwed[i] = 0;
    numPopcornsOrdered[i] = 0;
    numSodasOrdered[i] = 0;
    concessionClerkIsWorking[i] = 1;
  }
	
  /* Initialize movieTechnician values */
  movieStatus = 2; /* Movie starts in the "finished" state */
  movieLength = 0;
  movieFinishedLockCV = CreateCV("mflcv", 5);
  movieStatusLockCV = CreateCV("mslcv", 5);
  movieFinishedLock = CreateLock("mfl", 3);
  movieStatusLock = CreateLock("msl", 3);
  numSeatsOccupied = 0; 
	
  /* Initialize manager values */
  totalRevenue = 0;
}


/* Initialize players in this theater */
void initTheater() {

  int chooseSeatsLock = CreateLock("csl", 3);
  int ticketTakerCounterLock = CreateLock("ttcl", 4);
  int custsLeftToAssign = 3; /*MAX_CUST;*/
  int addNum = 3;
  
  int aNumGroups = CreateMV("numGroups", 9, 1);
  groups = CreateMV("groups", 6, MAX_CUST);
  totalCustomers = CreateMV("totalCustomers", 14, 1);
  totalCustomersServed = CreateMV("totalCustomersServed", 20, 1);
  groupHeads = CreateMV("groupHeads", 10, MAX_CUST);
  groupSize = CreateMV("groupSize", 9, MAX_CUST);
  nextGroup = CreateMV("nextGroup", 9, 1);
  numSeatsOccupied = CreateMV("numSeats", 8, 1);
  totalTicketsTaken = CreateMV("totalTicketsTaken", 17, 1);
  
  SetMV(totalCustomers, 0, 0);  
  SetMV(aNumGroups, 0, 0);
  SetMV(nextGroup, 0, 0);
  SetMV(numSeatsOccupied, 0, 0);
  SetMV(totalTicketsTaken, 0, 0);
  
  init_locks_and_stuff();

  while(custsLeftToAssign > 0) {
    /* int addNum = Rand()%5+1;  Random number of customers from 1-5  ******************** */
    if(addNum > custsLeftToAssign) {
      addNum = custsLeftToAssign;
    }
    
    Acquire(customerLobbyLock);
    SetMV(groups, addNum, GetMV(aNumGroups,0));
    SetMV(aNumGroups, GetMV(aNumGroups,0)+1, 0);
    custsLeftToAssign -= addNum;
/*    Release(customerLobbyLock);*/
  }
  
  /* Display for user how many of each theater player there are */
/*  Print("Number of Customers =        %i.\n", MAX_CUST, -1, -1);
  Print("Number of Groups =           %i.\n", GetMV(aNumGroups, 0), -1, -1);
  Print("Number of TicketClerks =     %i.\n", MAX_TC, -1, -1);
  Print("Number of ConcessionClerks = %i.\n", MAX_CC, -1, -1);
  Print("Number of TicketTakers =     %i.\n", MAX_TT, -1, -1); */
  
/*  Acquire(customerLobbyLock);*/
  customerInit(GetMV(aNumGroups, 0));
  Release(customerLobbyLock);
  
/*  Fork((void*)manager);*/
}

#endif
