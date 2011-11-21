/* the_movietech.c
 * Entity for movie theater to run a MovieTechnician
 */
 
#include "syscall.h"
#include "creates.h"

int main() {

  int i;
  
  initTheater();

  while(theaterDone == 0) {	
    
    if(theaterFull || (GetMV(totalCustomers,0)-(GetMV(totalCustomersServed,0)+GetMV(numSeatsOccupied,0))) <= (MAX_SEATS-GetMV(numSeatsOccupied,0)) || 1) {
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
        SetMV(freeSeatsInRow, NUM_COLS, i);
      
      theaterFull = 0;
      movieStarted = 0;
      SetMV(numSeatsOccupied, 0, 0);
      SetMV(totalTicketsTaken, 0, 0);
      
      /* Tell Customers to go to theater */
      if(GetMV(totalCustomers,0) != GetMV(totalCustomersServed,0)) {
        Acquire(customerLobbyLock);
        Broadcast(customerLobbyCV, customerLobbyLock);
        Release(customerLobbyLock);
      }
        
      Print("The MovieTechnician has told all customers to leave the theater room.\n", -1, -1, -1);
    }
    
    if(movieStatus == -1) {
      Yield();
    } else {
      while(((GetMV(numSeatsOccupied,0) < MAX_SEATS) && (GetMV(numSeatsOccupied,0) < GetMV(totalCustomers,0))) 
          || ((GetMV(numSeatsOccupied,0) == 0) && (GetMV(totalCustomersServed,0) == 0))) {
/*        Print("THERE ARE %d CUSTOMERS AND %d OF THE BITCHES ARE IN THEIR SEATS\n", GetMV(totalCustomers,0), GetMV(numSeatsOccupied,0), -1);*/
        Yield();
      }
/*      Acquire(movieStatusLock);
      Wait(movieStatusLockCV, movieStatusLock);  /* Wait until manager says it's time to start the movie */
/*      Release(movieStatusLock);*/
    }
    
    if((GetMV(totalCustomers, 0) == 0) && (GetMV(totalCustomersServed, 0) > 0)) {
      Print("\n Everyone has left, Manager is closing the theater. Good work boys!\n\n", -1, -1 ,-1);
      break;
    }
  }
  
  Exit(0);

  return 0;
  
}
