/* the_customer.c
 * Entity for movie theater to run a customer
 */
 
#include "syscall.h"
#include "creates.h"


void doBuyTickets(int custIndex, int groupIndex) {
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

    Acquire(ticketClerkLineLock);
    /* See if any TicketClerk not busy */
    for(i=0; i<MAX_TC; i++) {
      if(GetMV(ticketClerkState, i) == 0) {
        /* Found a clerk who's not busy */
        myTicketClerk = i;             /* and now you belong to me */
        SetMV(ticketClerkState, 1, i);
        Print("Customer %i in Group %i is walking up to TicketClerk %i ", custIndex, groupIndex, myTicketClerk);
        Print("to buy %i tickets.\n", GetMV(groupSize, groupIndex), -1, -1);
        break;
      }
    }
  
    /* All ticketClerks were occupied, find the shortest line instead */
/*    Print("My TicketClerk is %i.\n", myTicketClerk, -1, -1);*/
    if(myTicketClerk == 200) {
      Print("Customer %i did not find a free ticket clerk.\n", custIndex, -1, -1);
      shortestTCLine = 0;     /* default the first line to current shortest */
      shortestTCLineLength = 100000;
      for(j=0; j<MAX_TC; j++) {
        if((GetMV(ticketClerkState, j) == 1) && (shortestTCLineLength > GetMV(ticketClerkLineCount, j))) {
          /* current line is shorter */
          shortestTCLine = j;
          shortestTCLineLength = GetMV(ticketClerkLineCount, j);
        }
      }
    
      /* Found the TicketClerk with the shortest line */
      myTicketClerk = shortestTCLine;
      Print("Customer %i in Group %i is getting in TicketClerk line %i.\n", custIndex, groupIndex, myTicketClerk);
      
      /* Get in the shortest line */
      SetMV(ticketClerkLineCount, GetMV(ticketClerkLineCount,myTicketClerk)+1, myTicketClerk);
      Print("Customer %i is using CV %i to wait on lock %i.\n", custIndex, ticketClerkLineCV[myTicketClerk], ticketClerkLineLock);
      Wait(ticketClerkLineCV[myTicketClerk], ticketClerkLineLock);
      if((ticketClerkIsWorking[myTicketClerk] == 0) || (GetMV(ticketClerkState, myTicketClerk) == 2)) {
        findNewLine = 1;
        Print("Customer %i in Group %i sees TicketClerk %i is on break.\n", custIndex, groupIndex, myTicketClerk);
        Release(ticketClerkLineLock);
      }
    }
/*    Print("abcdefghi=%i\n", findNewLine, -1, -1);*/
/*  } while(findNewLine == 1); */

  Release(ticketClerkLineLock);
  /* Done with either going moving to TicketClerk or getting in line. Now sleep. */

  /* TicketClerk has acknowledged you. Time to wake up and talk to him.*/
  Acquire(ticketClerkLock[myTicketClerk]);

  SetMV(numberOfTicketsNeeded, GetMV(groupSize, groupIndex), myTicketClerk);

  Signal(ticketClerkCV[myTicketClerk], ticketClerkLock[myTicketClerk]);
  /* Done asking for 'myGroupSize' tickets from TicketClerk. Now sleep.*/

  /* TicketClerk has returned a price for you */
  Wait(ticketClerkCV[myTicketClerk], ticketClerkLock[myTicketClerk]);

  customers[custIndex].money -= GetMV(amountOwedTickets, myTicketClerk);               /* Pay the ticketClerk for the tickets */
  Print("Customer %i in Group %i in TicketClerk line %i ", custIndex, groupIndex, myTicketClerk);
  Print("is paying %i for tickets.\n", GetMV(amountOwedTickets, myTicketClerk), -1, -1);
  Signal(ticketClerkCV[myTicketClerk], ticketClerkLock[myTicketClerk]);
  
  /* TicketClerk has given you n tickets. Goodies for you! */
  Wait(ticketClerkCV[myTicketClerk], ticketClerkLock[myTicketClerk]);
  customers[custIndex].numTickets += GetMV(numberOfTicketsNeeded, myTicketClerk);      /* Receive tickets! */
  
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
      if(GetMV(ticketTakerState, i) == 0) {
        /* Found a clerk who's not busy */
        myTicketTaker = i;             /* and now you belong to me */
        SetMV(ticketTakerState, 1, i);
        Print("Customer %i in Group %i is getting in TicketTaker Line %i.\n", custIndex, groupIndex, myTicketTaker);
        break;
      }
    }
  
    /* All ticketTakers were occupied, find the shortest line instead */
    if(myTicketTaker == -1) {
      
      int shortestTTLine = 0;     /* default the first line to current shortest */
      int shortestTTLineLength = 1000000;
      for(j=0; j<MAX_TT; j++) {
        if((GetMV(ticketTakerState, j) == 1) && (shortestTTLineLength > GetMV(ticketTakerLineCount, j))) {
          /* current line is shorter */
          shortestTTLine = j;
          shortestTTLineLength = GetMV(ticketTakerLineCount, j);
        }
      }

      /* Found the TicketClerk with the shortest line */
      myTicketTaker = shortestTTLine;
      Print("Customer %i in Group %i is getting in TicketTaker line %i.\n", custIndex, groupIndex, myTicketTaker);
      
      /* Get in the shortest line */
      SetMV(ticketTakerLineCount, GetMV(ticketTakerLineCount,myTicketTaker)+1, myTicketTaker);
      Wait(ticketTakerLineCV[myTicketTaker], ticketTakerLineLock);
      if(GetMV(ticketTakerState, myTicketTaker) == 2) { /*ticketTakerIsWorking[myTicketTaker] == 0)) {*/
        findNewLine = 1;
        SetMV(ticketTakerLineCount, GetMV(ticketTakerLineCount,myTicketTaker)-1, myTicketTaker);
        Print("Customer %i in Group %i sees TicketTaker %i ", custIndex, groupIndex, myTicketTaker);
        Print("is on break (state=%i).\n", GetMV(ticketTakerState, myTicketTaker), -1, -1);
        Release(ticketTakerLineLock);
      }
    }
  } while(findNewLine);
  
  Release(ticketTakerLineLock);
  /* Done with either going moving to TicketTaker or getting in line. Now sleep. */
  
  /* TicketTaker has acknowledged you. Tell him how many tickets you have */
  Acquire(ticketTakerLock[myTicketTaker]);
  SetMV(numTicketsReceived, GetMV(groupSize, groupIndex), myTicketTaker);
  
  Print("Customer %i in Group %i is walking up to TicketTaker %i ", custIndex, groupIndex, myTicketTaker);
  Print("to give %i tickets.\n", GetMV(numTicketsReceived, myTicketTaker), -1, -1);
 
  Signal(ticketTakerCV[myTicketTaker], ticketTakerLock[myTicketTaker]);
  
  /* Did the TicketTaker let you in? */
  Wait(ticketTakerCV[myTicketTaker], ticketTakerLock[myTicketTaker]);
  if(!GetMV(allowedIn, myTicketTaker)) {
    /* Theater was either full or the movie had started */
    for(k=custIndex; k<custIndex+GetMV(groupSize, groupIndex); k++) {
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
    Release(ticketTakerLock[myTicketTaker]);                                    /* Fly free ticketTaker. You were a loyal friend. */
  }
}

/* Customer sitting in position [row, col] */
void choseSeat(int custIndex, int row, int col, int groupIndex) {
  customers[custIndex].seatRow = row+1; /* defaults rows from 1-5 as opposed to 0-4 */
  customers[custIndex].seatCol = col;
 
  Print("Customer %i in Group %i has found the following seat: ", custIndex, groupIndex, -1);
  Print("row %i and seat %i.\n", customers[custIndex].seatRow, customers[custIndex].seatCol, -1);
 
  SetMV(numSeatsOccupied, GetMV(numSeatsOccupied,0)+1, 0);
}

/* Try to find ideal seats in the theater */
void doChooseSeats(int custIndex, int groupIndex)
{
  int gSize = GetMV(groupSize, groupIndex);
  int i;
  int j;

  /* Try to find a row with enough free seats */
  for(i=0; i<NUM_ROWS; i++) {
    if(GetMV(freeSeatsInRow, i) >= gSize) {
      /* enough consecutive seats in one row */
      for(j=custIndex; j<(custIndex+gSize); j++) {
        /* Seating a customer */
        choseSeat(j, i, GetMV(freeSeatsInRow, i), groupIndex);
        SetMV(freeSeatsInRow, GetMV(freeSeatsInRow, i)-1, i);
      }
      return;
    }
  }
  
  /* Find two consecutive rows to sit in? */
  for(i=1; i<NUM_ROWS; i++) {
    if((GetMV(freeSeatsInRow, i-1) + GetMV(freeSeatsInRow, i)) >= gSize) {
      int toBeSeated = gSize;
      int freeInFirstRow = GetMV(freeSeatsInRow, i-1);
      for(j=0; j<freeInFirstRow; j++) {
        /* Seating a customer */
        choseSeat(custIndex+j, i-1, GetMV(freeSeatsInRow, i-1), groupIndex);
        SetMV(freeSeatsInRow, GetMV(freeSeatsInRow, i-1)-1, i-1);
        toBeSeated--;
      }
      for(j=0; j<toBeSeated; j++) {
        /* Seating remaining customers in group */
        choseSeat((custIndex+freeInFirstRow+j), i, GetMV(freeSeatsInRow, i), groupIndex);
        SetMV(freeSeatsInRow, GetMV(freeSeatsInRow, i)-1, i);
      }
      return;
    }
  }
  
  /* Just find somewhere to sit before someone gets mad */
  for(i=0; i<NUM_ROWS; i++) {
    int toBeSeated = gSize;
    if(GetMV(freeSeatsInRow, i) > 0) {
      for(j=0; j<GetMV(freeSeatsInRow, i); j++) {
        /* Sit customer down here */
        choseSeat(custIndex+(gSize-toBeSeated), i, GetMV(freeSeatsInRow, i)-j, groupIndex);
        toBeSeated--;
        if(toBeSeated == 0) {
          SetMV(freeSeatsInRow, GetMV(freeSeatsInRow, i)-j+1, i);
          return;
        }
      }
      SetMV(freeSeatsInRow, 0, i);
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

  for(i=0; i<GetMV(totalCustomers, 0); i++) {
/*    if(customers[custIndex].group == customers[i].group) {
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
    }*/
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
      if(GetMV(concessionClerkState, i) == 0) {
        /* Found a clerk who's not busy */
        myConcessionClerk = i;
        SetMV(concessionClerkState, 1, i);
        break;
      }
    }
    
    /* All concessionClerks were occupied, find the shortest line instead */
    if(myConcessionClerk == -1) {
      
      int shortestCCLine = 0; /* default the first line to current shortest */
      int shortestCCLineLength = 10000;
      for(i=1; i<MAX_CC; i++) {
        if((GetMV(concessionClerkState, i) == 1) && (shortestCCLineLength > GetMV(concessionClerkLineCount, i))) {
          /* current line is shorter */
          shortestCCLine = i;
          shortestCCLineLength = GetMV(concessionClerkLineCount, i);
        }
      }
      
      /* Found the ConcessionClerk with the shortest line */
      myConcessionClerk = shortestCCLine;
      Print("Customer %i in Group %i is getting in ConcessionClerk line %i.\n", custIndex, groupIndex, myConcessionClerk);
      
      /* Get in the shortest line */
      SetMV(concessionClerkLineCount, GetMV(concessionClerkLineCount,myConcessionClerk)+1, myConcessionClerk);
      Wait(concessionClerkLineCV[myConcessionClerk], concessionClerkLineLock);
      if(concessionClerkIsWorking[myConcessionClerk] == 0 || GetMV(concessionClerkState, myConcessionClerk) == 2) {
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
  Print("Customer %i in Group %i is walking up to ConcessionClerk %i ", custIndex, groupIndex, myConcessionClerk);
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
  for(i=0; i<GetMV(totalCustomers, 0); i++) {
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
  int i;
  
  for(i=custIndex; i<custIndex+GetMV(groupSize, groupIndex); i++) {
    randNum = 30; /* ************rand()%100;********** */ 
    if(randNum < 25) {
      customers[i].needsRestroom = 1;
    }
  }

  for(i=custIndex; i<custIndex+GetMV(groupSize, groupIndex); i++) {
    if(customers[i].needsRestroom == 1) {
      Print("Customer %i in Group %i is going to the bathroom and has left the movie.\n", i, groupIndex, -1);
      numInRestroom++;
    } else {
      Print("Customer %i in Group %i is in the lobby after the movie\n", i, groupIndex, -1);
    }
  }
    
  Acquire(waitingOnGroupLock[groupIndex]);
  
  for(i=custIndex; i<custIndex+GetMV(groupSize, groupIndex); i++) {
    if(customers[i].needsRestroom == 1) {
      Fork((void*)doReturnFromRestroom);
    }
  }
  
  while(numInRestroom > 0) {
    Wait(waitingOnGroupCV[groupIndex], waitingOnGroupLock[groupIndex]);
    numInRestroom = 0;
    for(i=custIndex; i<custIndex+GetMV(groupSize, groupIndex); i++) {
      if(customers[i].needsRestroom == 1) {
        numInRestroom++;
      }
    }
  }
  Release(waitingOnGroupLock[groupIndex]);
}
 

int main() {

  int i;
  int groupIndex;
  int custIndex;

  initTheater();

  groupIndex = GetMV(nextGroup, 0);
  custIndex = GetMV(groupHeads, groupIndex);
  SetMV(nextGroup, GetMV(nextGroup,0)+1, 0);
  
  Print("Starting customer routine for cust %d in group %d\n", custIndex, groupIndex, -1);
  /*int groupIndex = customers[custIndex].group;*/
  
  doBuyTickets(custIndex, groupIndex);
  
  Print("Customer %i entering takeFoodOrders.\n", custIndex, -1, -1);
  takeFoodOrders(custIndex);
  
  Print("Customer %i just took food orders.\n", custIndex, -1, -1);
  if((customers[custIndex].totalSodas > 0) || (customers[custIndex].totalPopcorns > 0)) {
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
  Acquire(customerLobbyLock);
  doLeaveTheaterAndUseRestroom(custIndex, groupIndex);
  
  /* Counter to see how many customers have left movies */
  SetMV(totalCustomersServed, GetMV(totalCustomersServed,0)+GetMV(groupSize,groupIndex), 0);
  
  for(i=custIndex; i<custIndex+GetMV(groupSize, groupIndex); i++)
  {
    SetMV(totalCustomers, GetMV(totalCustomers,0)-1, 0);
    if(GetMV(totalCustomers,0) >=0) {
      Print("Customer %i in Group %i has left the movie theater (%d left).\n", i, groupIndex, GetMV(totalCustomers,0));
      if(GetMV(totalCustomers, 0) <= 0)
        Print("\nMovie Theater simulation completed successfully!!\n\n", -1, -1, -1);
    }
  }
  Release(customerLobbyLock);
  
  Exit(0);
  
  return 0;
}
