# use normal make for this Makefile
#
# Makefile for building user programs to run on top of Nachos
#
# Several things to be aware of:
#
#    Nachos assumes that the location of the program startup routine (the
# 	location the kernel jumps to when the program initially starts up)
#       is at location 0.  This means: start.o must be the first .o passed 
# 	to ld, in order for the routine "Start" to be loaded at location 0
#

# If you are running on SunOS 5.x machines, uncomment the next line
GCCDIR = /auto/home-scf-22/csci402/testgnu/decstation-ultrix/bin/
LDFLAGS = -T newscript -N
ASFLAGS = -mips1
CPPFLAGS = $(INCDIR)

# if you aren't cross-compiling:
# GCCDIR =
# LDFLAGS = -N -T 0
# ASFLAGS =
# CPPFLAGS = -P $(INCDIR)


CC = $(GCCDIR)gcc
AS = $(GCCDIR)as
LD = $(GCCDIR)ld
CPP = $(GCCDIR)cpp

INCDIR =-I../userprog -I../threads
CFLAGS = -G 0 -c $(INCDIR)

all: halt matmult sort theater_sim two_theaters two_matmults networktests \
  execnetworktests the_customer the_ticketclerk the_tickettaker the_movietech \
  dist_theater1 dist_theater2 the_concessionclerk the_manager

start.o: start.s ../userprog/syscall.h
	$(CPP) $(CPPFLAGS) start.s > strt.s
	$(AS) $(ASFLAGS) -o start.o strt.s
	rm strt.s

halt.o: halt.c
	$(CC) $(CFLAGS) -c halt.c
halt: halt.o start.o
	$(LD) $(LDFLAGS) start.o halt.o -o halt.coff
	../bin/coff2noff halt.coff halt

sort.o: sort.c
	$(CC) $(CFLAGS) -c sort.c
sort: sort.o start.o
	$(LD) $(LDFLAGS) start.o sort.o -o sort.coff
	../bin/coff2noff sort.coff sort

matmult.o: matmult.c
	$(CC) $(CFLAGS) -c matmult.c
matmult: matmult.o start.o
	$(LD) $(LDFLAGS) start.o matmult.o -o matmult.coff
	../bin/coff2noff matmult.coff matmult

networktests.o: networktests.c
	$(CC) $(CFLAGS) -c networktests.c
networktests: networktests.o start.o
	$(LD) $(LDFLAGS) start.o networktests.o -o networktests.coff
	../bin/coff2noff networktests.coff networktests

execnetworktests.o: execnetworktests.c
	$(CC) $(CFLAGS) -c execnetworktests.c
execnetworktests: execnetworktests.o start.o
	$(LD) $(LDFLAGS) start.o execnetworktests.o -o execnetworktests.coff
	../bin/coff2noff execnetworktests.coff execnetworktests

testfiles.o: testfiles.c
	$(CC) $(CFLAGS) -c testfiles.c
testfiles: testfiles.o start.o
	$(LD) $(LDFLAGS) start.o testfiles.o -o testfiles.coff
	../bin/coff2noff testfiles.coff testfiles

exectest.o: exectest.c
	$(CC) $(CFLAGS) -c exectest.c
exectest: exectest.o start.o
	$(LD) $(LDFLAGS) start.o exectest.o -o exectest.coff
	../bin/coff2noff exectest.coff exectest

theater_sim.o: theater_sim.c
	$(CC) $(CFLAGS) -c theater_sim.c
theater_sim: theater_sim.o start.o
	$(LD) $(LDFLAGS) start.o theater_sim.o -o theater_sim.coff
	../bin/coff2noff theater_sim.coff theater_sim

theater_sim_test.o: theater_sim_test.c
	$(CC) $(CFLAGS) -c theater_sim_test.c
theater_sim_test: theater_sim_test.o start.o
	$(LD) $(LDFLAGS) start.o theater_sim_test.o -o theater_sim_test.coff
	../bin/coff2noff theater_sim_test.coff theater_sim_test

two_theaters.o: two_theaters.c
	$(CC) $(CFLAGS) -c two_theaters.c
two_theaters: two_theaters.o start.o
	$(LD) $(LDFLAGS) start.o two_theaters.o -o two_theaters.coff
	../bin/coff2noff two_theaters.coff two_theaters
	
two_matmults.o: two_matmults.c
	$(CC) $(CFLAGS) -c two_matmults.c
two_matmults: two_matmults.o start.o
	$(LD) $(LDFLAGS) start.o two_matmults.o -o two_matmults.coff
	../bin/coff2noff two_matmults.coff two_matmults
	


dist_theater1.o: dist_theater1.c
	$(CC) $(CFLAGS) -c dist_theater1.c
dist_theater1: dist_theater1.o start.o
	$(LD) $(LDFLAGS) start.o dist_theater1.o -o dist_theater1.coff
	../bin/coff2noff dist_theater1.coff dist_theater1
	
dist_theater2.o: dist_theater2.c
	$(CC) $(CFLAGS) -c dist_theater2.c
dist_theater2: dist_theater2.o start.o
	$(LD) $(LDFLAGS) start.o dist_theater2.o -o dist_theater2.coff
	../bin/coff2noff dist_theater2.coff dist_theater2
	
dist_theater3.o: dist_theater3.c
	$(CC) $(CFLAGS) -c dist_theater3.c
dist_theater3: dist_theater3.o start.o
	$(LD) $(LDFLAGS) start.o dist_theater3.o -o dist_theater3.coff
	../bin/coff2noff dist_theater3.coff dist_theater3
	
dist_theater4.o: dist_theater4.c
	$(CC) $(CFLAGS) -c dist_theater4.c
dist_theater4: dist_theater4.o start.o
	$(LD) $(LDFLAGS) start.o dist_theater4.o -o dist_theater4.coff
	../bin/coff2noff dist_theater4.coff dist_theater4
	
dist_theater5.o: dist_theater5.c
	$(CC) $(CFLAGS) -c dist_theater5.c
dist_theater5: dist_theater5.o start.o
	$(LD) $(LDFLAGS) start.o dist_theater5.o -o dist_theater5.coff
	../bin/coff2noff dist_theater5.coff dist_theater5
	
	
the_concessionclerk.o: the_concessionclerk.c
	$(CC) $(CFLAGS) -c the_concessionclerk.c
the_concessionclerk: the_concessionclerk.o start.o
	$(LD) $(LDFLAGS) start.o the_concessionclerk.o -o the_concessionclerk.coff
	../bin/coff2noff the_concessionclerk.coff the_concessionclerk
	
the_customer.o: the_customer.c creates.h
	$(CC) $(CFLAGS) -c the_customer.c
the_customer: the_customer.o start.o
	$(LD) $(LDFLAGS) start.o the_customer.o -o the_customer.coff
	../bin/coff2noff the_customer.coff the_customer
	
the_manager.o: the_manager.c creates.h
	$(CC) $(CFLAGS) -c the_manager.c
the_manager: the_manager.o start.o
	$(LD) $(LDFLAGS) start.o the_manager.o -o the_manager.coff
	../bin/coff2noff the_manager.coff the_manager
	
the_movietech.o: the_movietech.c creates.h
	$(CC) $(CFLAGS) -c the_movietech.c
the_movietech: the_movietech.o start.o
	$(LD) $(LDFLAGS) start.o the_movietech.o -o the_movietech.coff
	../bin/coff2noff the_movietech.coff the_movietech
	
the_ticketclerk.o: the_ticketclerk.c creates.h
	$(CC) $(CFLAGS) -c the_ticketclerk.c
the_ticketclerk: the_ticketclerk.o start.o
	$(LD) $(LDFLAGS) start.o the_ticketclerk.o -o the_ticketclerk.coff
	../bin/coff2noff the_ticketclerk.coff the_ticketclerk
	
the_tickettaker.o: the_tickettaker.c creates.h
	$(CC) $(CFLAGS) -c the_tickettaker.c
the_tickettaker: the_tickettaker.o start.o
	$(LD) $(LDFLAGS) start.o the_tickettaker.o -o the_tickettaker.coff
	../bin/coff2noff the_tickettaker.coff the_tickettaker


clean:
	rm -f *.o *.coff
