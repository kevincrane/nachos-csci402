#include "syscall.h"

int main() {
	int myid;
	int lock1;
	int cv1;
	lock1 = CreateLock("Lock1", 5);	
	cv1 = CreateCV("CV1", 3);
	myid = Identify();
	Print("My super awesome id is %d\n", myid, -1, -1);
	Acquire(lock1);
	
		Print("AHHH BUSY WAITING\n", -1, -1, -1);
	
}
