// theater_sim.h
// Header file for theater_sim.cc
//
// TODO: add documentation here

#ifndef THEATER_SIM_H
#define THEATER_SIM_H

#include "copyright.h"
#include "utility.h"
#include "synch.h"


// CONSTANTS
int MAX_TC = 5;							// constant: defines maximum number of ticket clerks
int MAX_CUST = 50;          // constant: maximum number of customers

// Customer variables
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

// Group variables
int groupHeads[MAX_CUST];   // List of indices pointing to the leaders of each group
int groupSize[MAX_CUST];    // The size of each group


// TicketClerk variables
int   ticketClerkState[MAX_TC];           // Current state of a ticketClerk
Lock*	ticketClerkLock[MAX_TC];            // array of Locks, each corresponding to one of the TicketClerks
Condition*	ticketClerkCV[MAX_TC];        // condition variable assigned to particular TicketClerk
int 	ticketClerkLineCount[MAX_TC];       // How many people in each ticketClerk's line?
int		numberOfTicketsNeeded[MAX_TC];      // How many tickets does the current customer need?

Lock*	ticketClerkLineLock;                // Lock for line of customers
Condition*	ticketClerkLineCV[MAX_TC];		// Condition variable assigned to particular line of customers


#endif // THEATER_SIM_H
