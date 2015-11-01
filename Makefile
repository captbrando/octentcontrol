#If you do not have gcc, change the setting for COMPILER, but you must
#use an ANSI standard C compiler (NOT the old SunOS 4.1.3 cc
#compiler; get gcc if you are still using it). 
COMPILER=gcc
#CFLAGS=-O
CFLAGS=-g -Wall
LIBS=-lpthread -lpq -lexpat -lefence
INCLUDEDIRS=-I. -I/usr/include -I/usr/include/postgresql
#INCLUDEDIRS=-I.
LIBDIRS=-L.

#
# Changes should not be required below here.
#
#

#VERSION=1.8.4

CC=$(COMPILER) $(CFLAGS) $(INCLUDEDIRS)
LINK=$(CC) $(CFLAGS) $(LIBDIRS) $(LIBS)

all: octentcontrol

octentcontrol: main.o queue.o dbfunc.o control.o brwhelpers.o mypthreads.o log.o
	$(LINK) -o octentcontrol main.o queue.o dbfunc.o control.o brwhelpers.o mypthreads.o log.o

main.o: main.c
	$(CC) -c main.c

queue.o: queue.c
	$(CC) -c queue.c

dbfunc.o: dbfunc.c
	$(CC) -c dbfunc.c

control.o: control.c
	$(CC) -c control.c

brwhelpers.o: brwhelpers.c
	$(CC) -c brwhelpers.c

log.o: log.c
	$(CC) -c log.c

mypthreads.o: mypthreads.c
	$(CC) -c mypthreads.c

clean:
	rm -f *.o 
	rm -f octentcontrol
