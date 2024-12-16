CC=g++
CFLAGS=-std=gnu++20
OBJS=velar.o
HEADERS=velar.h

all: libvelar.a test

%.o: %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<
libvelar.a: $(OBJS) 
	ar rcs libvelar.a $(OBJS)

test:
	make -C test1
	make -C test2
	make -C test3
	make -C test4
	make -C test5
	
clean:
	rm $(OBJS)
	rm libvelar.a
	make -C test1 clean
	make -C test2 clean
	make -C test3 clean
	make -C test4 clean
	make -C test5 clean
