CC=g++
CFLAGS=-std=gnu++20 -I../
EXECNAME=test1
OBJS=$(EXECNAME).o
HEADERS=

all: test1

%.o: %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

test1: $(OBJS) $(HEADERS)
	mkdir -p build
	$(CC) -L../ -o build/$(EXECNAME) $(OBJS) -lvelar

clean:
	rm $(OBJS)
	rm -rf build