SHELL = /bin/sh
CC = g++
AR = ar
 
CFLAG = -O2 -W -Wall -DNC_HAVE_KQUEUE # -DNC_HAVE_EPOLL
 
INCLUDE_PATH = -I. -I../
EVENT_TARGET = libnc_event.a
EVENT_OBJ = nc_epoll.o nc_kqueue.o
EVENT_SRC = nc_epoll.cpp nc_kqueue.cpp
 
all : $(EVENT_TARGET)
$(EVENT_OBJ): $(EVENT_SRC)
	$(CC) $(CFLAG) $(INCLUDE_PATH) -c $(EVENT_SRC)
 
$(EVENT_TARGET): $(EVENT_OBJ)
	$(AR) cq $@ $^
 
clean:
	rm -f *.o $(EVENT_TARGET)