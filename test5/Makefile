CC=g++
CFLAGS=-std=gnu++20 -I../
EXECNAME=test5
OBJS=$(EXECNAME).o
HEADERS=

all: test

%.o: %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(OBJS) $(HEADERS)
	mkdir -p build
	$(CC) -L../ -o build/$(EXECNAME) $(OBJS) -lvelar

clean:
	rm $(OBJS)
	rm -rf build
