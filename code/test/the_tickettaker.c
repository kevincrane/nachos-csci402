/* the_tickettaker.c
 * Entity for movie theater to run a TicketTaker
 */
 
#include "syscall.h"
#include "creates.h"


int main() {

  int myIndex;
  
  initTheater();
  
  myIndex = GetMV(nextTT, 0);
  SetMV(nextTT, myIndex+1, 0);

  while(1) {
    /* TicketTaker is on break, go away. */
    if(GetMV(ticketTakerState, myIndex) == 2) {
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
      SetMV(ticketTakerState, 0, myIndex);
      Release(ticketTakerLineLock);
    }

    /* Is there a customer in my line? */
    Acquire(ticketTakerLineLock);
    if(GetMV(ticketTakerLineCount, myIndex) > 0) {	/* There's a customer in my line */
      SetMV(ticketTakerState, 1, myIndex);        /* I must be busy, decrement line count */
      SetMV(ticketTakerLineCount, GetMV(ticketTakerLineCount,myIndex)+1, myIndex);
      Print("TicketTaker %i has a line length %i and is signaling a customer.\n", myIndex,GetMV(ticketTakerLineCount, myIndex)+1, -1);

      Signal(ticketTakerLineCV[myIndex], ticketTakerLineLock); /* Wake up 1 customer */
    } else {
      /* No one in my line */
      Print("TicketTaker %i has no one in line. I am available for a customer.\n", myIndex, -1, -1);
      SetMV(ticketTakerState, 0, myIndex);
    }
    Acquire(ticketTakerLock[myIndex]);
    Release(ticketTakerLineLock);
  
    /* If put on break instead, bail and wait */
    if((!ticketTakerIsWorking[myIndex]) && (GetMV(ticketTakerState, myIndex) == 2)) { /* numTicketsReceived[myIndex] < 0) { */
      Release(ticketTakerLock[myIndex]);
      continue;
    }
  
    /* Wait for Customer to tell me how many tickets he has */
    Wait(ticketTakerCV[myIndex], ticketTakerLock[myIndex]);
     
    Acquire(ticketTakerLineLock);
    Acquire(ticketTakerCounterLock);
    if((GetMV(numTicketsReceived, myIndex) > (MAX_SEATS-GetMV(totalTicketsTaken,0))) || theaterFull || movieStarted)
    {
      /* Movie has either started or is too full */
      if(theaterFull || movieStarted) {
        Print("TicketTaker %i has stopped taking tickets.\n", myIndex, -1, -1);
      }
      
      else {
        theaterFull = 1;
        
        Print("TicketTaker %i is not allowing the group into the theater. ", myIndex, -1, -1);
        Print("The number of taken tickets is %i and the group size is %i.\n", GetMV(totalTicketsTaken, 0), GetMV(numTicketsReceived, myIndex), -1);
      }
      
      SetMV(allowedIn, 0, myIndex);
      Release(ticketTakerLineLock);
      Release(ticketTakerCounterLock);
      Signal(ticketTakerCV[myIndex], ticketTakerLock[myIndex]);
      Wait(ticketTakerCV[myIndex], ticketTakerLock[myIndex]);
      Release(ticketTakerLock[myIndex]);
      continue;
    } else {
      SetMV(allowedIn, 1, myIndex);
      Release(ticketTakerLineLock);
    }
    Signal(ticketTakerCV[myIndex], ticketTakerLock[myIndex]);

    /*Wait for Customer to give me their tickets */
    Acquire(ticketTakerCounterLock); /* ************************************************** */
    Print("TicketTaker %i has received %i tickets.\n", myIndex, GetMV(numTicketsReceived, myIndex), -1);

    SetMV(totalTicketsTaken, GetMV(numTicketsReceived, myIndex), 0);
    
    if(GetMV(totalCustomers,0)-GetMV(totalCustomersServed,0) <= GetMV(totalTicketsTaken,0)) 
      theaterFull = 1;
  
    Release(ticketTakerCounterLock);
/*    Wait(ticketTakerCV[myIndex], ticketTakerLock[myIndex]);*/
    Release(ticketTakerLock[myIndex]);
    /* TicketTaker is done with customer, allows them to move into the theater */
    
    if((GetMV(totalCustomers, 0) == 0) && (GetMV(totalCustomersServed, 0) > 0)) {
      break;
    }
  }
    
  Exit(0);
  
  return 0;

}
