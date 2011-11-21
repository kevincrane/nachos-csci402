/* the_ticketclerk.c
 * Entity for movie theater to run a TicketClerk
 */
 
#include "syscall.h"
#include "creates.h"


/* TicketClerk */
/* -Process that is run when Customer is interacting with TicketClerk to buy movie tickets */
int main() {

  int numberOfTicketsHeld;
  int myIndex;
  
  initTheater();
  
  myIndex = GetMV(nextTC, 0);
  SetMV(nextTC, myIndex+1, 0);

  while(1) {
    /* TicketClerk is on break, go away. */
    if(GetMV(ticketClerkState, myIndex) == 2) {
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
      SetMV(ticketClerkState, 0, myIndex);
      Release(ticketClerkLineLock);
    }

    /* Is there a customer in my line? */
    Acquire(ticketClerkLineLock);
    if(GetMV(ticketClerkLineCount, myIndex) > 0) {	/* There's a customer in my line */
      SetMV(ticketClerkState, 1, myIndex);        /* I must be busy, decrement line count */
      SetMV(ticketClerkLineCount, GetMV(ticketClerkLineCount,myIndex)-1, myIndex);
      Print("TicketClerk %i has a line length %i and is signaling a customer.\n", myIndex, GetMV(ticketClerkLineCount, myIndex)+1, -1);
      Signal(ticketClerkLineCV[myIndex], ticketClerkLineLock); /* Wake up 1 customer */
    } else {
      /* No one in my line */
      Print("TicketClerk %i has no one in line. I am available for a customer.\n", myIndex, -1, -1);
      SetMV(ticketClerkState, 0, myIndex);
    }
      
    Release(ticketClerkLineLock);
    Acquire(ticketClerkLock[myIndex]);         /* Call dibs on this current ticketClerk for a while */
  
    /* If put on break, bail and wait */
    if((!ticketClerkIsWorking[myIndex]) && (GetMV(ticketClerkState, myIndex) == 2)) {
      Release(ticketClerkLock[myIndex]);
      continue;
    }
    /* Wait for Customer to come to my counter and ask for tickets; tell him what he owes. */
    Wait(ticketClerkCV[myIndex], ticketClerkLock[myIndex]);
  
    SetMV(amountOwedTickets, GetMV(numberOfTicketsNeeded,myIndex) * TICKET_PRICE, myIndex);   /* Tell customer how much money he owes */

    Print("TicketClerk %i has an order for %i and the cost is %i.\n", myIndex, GetMV(numberOfTicketsNeeded, myIndex), GetMV(amountOwedTickets, myIndex));
    
    Signal(ticketClerkCV[myIndex], ticketClerkLock[myIndex]);
  
    /* Customer has given you money, give him the correct number of tickets, send that fool on his way */
    Wait(ticketClerkCV[myIndex], ticketClerkLock[myIndex]);
    ticketClerkRegister[myIndex] += GetMV(amountOwedTickets, myIndex);     /* Add money to cash register */
    numberOfTicketsHeld = GetMV(numberOfTicketsNeeded, myIndex);           /* Give customer his tickets */

    Signal(ticketClerkCV[myIndex], ticketClerkLock[myIndex]);
  
    Wait(ticketClerkCV[myIndex], ticketClerkLock[myIndex]);         /* Wait until customer is out of the way before beginning next one */
    Release(ticketClerkLock[myIndex]); /* Temp? */
    
    if((GetMV(totalCustomers, 0) == 0) && (GetMV(totalCustomersServed, 0) > 0)) {
      break;
    }
  }
  
  Exit(0);

  return 0;
}
