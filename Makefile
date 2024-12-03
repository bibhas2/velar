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
	
clean:
	rm $(OBJS)
	rm libvelar.a