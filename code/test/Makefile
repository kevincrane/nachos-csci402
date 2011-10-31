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

all: halt matmult sort testfiles exectest theater_sim theater_sim_test two_theaters networktests

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

clean:
	rm -f *.o *.coff
