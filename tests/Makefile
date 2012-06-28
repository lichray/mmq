# ccconf mutual_calc CXX=g++47 CXXFLAGS+=-std=c++11 -I../include -Wall -O2 LDFLAGS+=-pthread
LDFLAGS  = -pthread  
CXXFLAGS = -std=c++11 -I../include -Wall -O2  
CXX      = g++47  

.PHONY : all clean
all : mutual_calc
clean :
	rm -f mutual_calc mutual_calc.o semaphore.o tags

tags : *.h mutual_calc.cc 
	ctags *.h mutual_calc.cc 

mutual_calc : mutual_calc.o semaphore.o
	${CXX} ${LDFLAGS} -o mutual_calc mutual_calc.o semaphore.o
mutual_calc.o: mutual_calc.cc ../include/ext/mutex_sem.h
semaphore.o: ../src/semaphore.cc ../include/ext/semaphore.h
