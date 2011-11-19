/* the_manager.c
 * Entity for movie theater to run a Manager
 */
 
#include "syscall.h"
#include "creates.h"

int main() {

  int numTConBreak = 0;
  int numCConBreak = 0;
  int numTTonBreak = 0;
  int i;
  int j;
  int k;
  int offBreakCC;
  int offBreakTT;
  
  initTheater();

  theaterFull = 0;
  totalRevenue = 0;
  
  /* Check for Employees to put  break */  
  while(1) {
    /* Checking TicketClerks to pull off break */
    int offBreakTC = 0;

    for(i=0; i<MAX_TC; i++) {
      /* Pull TC off break if line's long */
      Acquire(ticketClerkLineLock);
      if((GetMV(ticketClerkLineCount, i) >= 5) && (numTConBreak >= 1) && (ticketClerkIsWorking[i])) {
	      for(j=0; j<MAX_TC; j++) {
          if((j == i) || (ticketClerkIsWorking[j])) continue;
          Acquire(ticketClerkLock[j]);
          SetMV(ticketClerkState, 0, j);          /* TicketClerk is off Break */

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
      if(!ticketClerkIsWorking[i] && GetMV(ticketClerkLineCount, i) > 0) {
        Broadcast(ticketClerkLineCV[i], ticketClerkLineLock);
      }
      if((GetMV(ticketClerkLineCount, i) < 3) && (GetMV(ticketClerkState, i) != 2) && (MAX_TC-numTConBreak > 1)) {
        if(1 == 0) {
	/* if(rand()%5 == 0) { *************************** */
          Acquire(ticketClerkLock[i]);
          SetMV(ticketClerkState, 2, i);          /* TicketClerk is on Break */
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
      if((GetMV(concessionClerkLineCount, i) >= 5) && (numCConBreak >= 1) && (concessionClerkIsWorking[i])) {
        for(j=0; j<MAX_CC; j++) {
          if((j == i) || (concessionClerkIsWorking[j])) continue;
          Acquire(concessionClerkLock[j]);
          SetMV(concessionClerkState, 0, j);          /* ConcessionClerk is off Break */
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
      if(!concessionClerkIsWorking[i] && GetMV(concessionClerkLineCount, i) > 0) {
        Broadcast(concessionClerkLineCV[i], concessionClerkLineLock);
      }
      /* Potentially put CC on break if line is small and someone else is working */
      if((GetMV(concessionClerkLineCount, i) < 3) && (GetMV(concessionClerkState, i) != 2) && (MAX_CC-numCConBreak > 1)) {
        if(1 == 0) {
	  /* if(rand()%5 == 0) { ******************** */
          Acquire(concessionClerkLock[i]);
          SetMV(concessionClerkState, 2, i);          /* ConcessionClerk is on Brea */
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
      if((GetMV(ticketTakerLineCount, i) >= 5) && (numTTonBreak >= 1) && (ticketTakerIsWorking[i])) {
        for(j=0; j<MAX_TT; j++) {
          if((j == i) || (ticketTakerIsWorking[j])) continue;
          Acquire(ticketTakerLock[j]);
          SetMV(ticketTakerState, 0, j);          /* TicketTaker is off Break */
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
      if(!ticketTakerIsWorking[i] && GetMV(ticketTakerLineCount, i) > 0) {
        Broadcast(ticketTakerLineCV[i], ticketTakerLineLock);
      }
      /* Potentially put TT on break if line is small and someone else is working */
      if((GetMV(ticketTakerLineCount, i) < 3) && (GetMV(ticketTakerState, i) != 2) && (MAX_TT-numTTonBreak > 1)) {
        if(1 == 0) {
	  /* if(rand()%5 == 0) { ********************************* */
          Acquire(ticketTakerLock[i]);
          SetMV(ticketTakerState, 2, i);          /* TicketTaker is on Break */
          Print("Manager has told TicketTaker %i to go on break.\n", i, -1, -1);

          numTTonBreak++;
          
          ticketTakerIsWorking[i] = 0;
          SetMV(numTicketsReceived, 0, i);
          
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
      if((theaterFull || (GetMV(totalCustomers,0)-(GetMV(totalCustomersServed,0)+GetMV(numSeatsOccupied,0))) <= (MAX_SEATS-GetMV(numSeatsOccupied,0)))  && GetMV(totalTicketsTaken, 0) != 0) { /*!theaterFull || (totalTicketsTaken == 0 && numSeatsOccupied == 0)) { */
	  
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
		
    if(GetMV(totalCustomersServed,0) == GetMV(totalCustomers,0)){
      Print("\n\nManager: Everyone is happy and has left. Closing the theater.\n\n\n", -1, -1, -1);
      theaterDone = 1;
      break;
    }
		
    for(i=0; i<10; i++) {
      Yield();
    }
  }
  
  Exit(0);

  return 0;
  
}
