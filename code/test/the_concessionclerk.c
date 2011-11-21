/* the_concessionclerk.c
 * Entity for movie theater to run a ConcessionClerk
 */
 
#include "syscall.h"
#include "creates.h"


int main() {
  
  int myIndex;
  
  initTheater();
  
  myIndex = GetMV(nextCC, 0);
  SetMV(nextCC, myIndex+1, 0);
  
  while(1) {
    if(GetMV(concessionClerkState, myIndex) == 2) {
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
      SetMV(concessionClerkState, 0, myIndex);
      Release(concessionClerkLineLock);
    }
    
    /* Is there a customer in my line? */
    Acquire(concessionClerkLineLock);
    if(GetMV(concessionClerkLineCount, myIndex) > 0) {   /* There's a customer in my line */
      SetMV(concessionClerkState, 1, myIndex);          /* I must be busy */        
      Print("ConcessionClerk %i has a line length %i and is signaling a customer.\n", myIndex, GetMV(concessionClerkLineCount, myIndex), -1);
      SetMV(concessionClerkLineCount, GetMV(concessionClerkLineCount,myIndex)-1, myIndex);
      Signal(concessionClerkLineCV[myIndex], concessionClerkLineLock); /* Wake up 1 customer */
    } else {
      /* No one in my line */
      Print("ConcessionClerk %i has no one in line. I am available for a customer.\n", myIndex, -1, -1);
      SetMV(concessionClerkState, 0, myIndex);
    }
    
    Release(concessionClerkLineLock);
    Acquire(concessionClerkLock[myIndex]);
    
    /* If put on break instead, bail and wait */
    if((!concessionClerkIsWorking[myIndex]) && (GetMV(concessionClerkState, myIndex) == 2)) {
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
    
    if((GetMV(totalCustomers, 0) == 0) && (GetMV(totalCustomersServed, 0) > 0)) {
      break;
    }
  }
  Exit(0);

  return 0;
  
}
