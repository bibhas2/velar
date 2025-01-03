// test5.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <velar.h>
#include <assert.h>

void test_put1() {
	HeapByteBuffer b(128);

	b.put("Hello World");
	b.put('!');

	b.flip();

	assert(b.remaining() == 12);
}

void test_get1() {
	HeapByteBuffer b(128);

	b.put("Hello World");

	b.flip();

	char dest[5];

	b.get(dest, 0, 5);

	std::string_view sv(dest, 5), expected("Hello");

	assert(sv == expected);

	assert(b.position() == 5);

	expected = " World";

	assert(b.to_string_view() == expected);
}

void test_get2() {
	HeapByteBuffer b(128);

	b.put("Hello World");

	b.flip();

	char ch;

	b.get(ch);
	assert(ch == 'H');

	b.get(ch);
	assert(ch == 'e');

	assert(b.to_string_view() == "llo World");
}

void test_get3() {
	HeapByteBuffer b(128);

	b.put("Hello World");

	b.flip();

	char dest[10];
	size_t offset = 2;

	b.get(dest, offset, 5);

	std::string_view sv{ dest + offset, 5 };

	assert(sv == "Hello");

	char ch;

	b.get(ch);

	assert(ch == ' ');

	b.get(dest, offset, 5);

	sv = { dest + offset, 5 };

	assert(sv == "World");

	assert(b.remaining() == 0);
}

void test_wrapped1() {
	//Test wrapped storage
	char storage[128];
	WrappedByteBuffer b(storage, sizeof(storage));

	b.put("Hello Wonderful World");

	b.flip();

	std::string_view sv{};

	b.get(sv, 5);

	assert(sv == "Hello");

	char ch;

	b.get(ch);

	assert(ch == ' ');

	b.get(sv);

	assert(sv == "Wonderful World");

	assert(b.has_remaining() == false);
}

void test_get5() {
	//Test integer I/O

	StaticByteBuffer<128> b;
	uint32_t i1 = UINT32_MAX, j1{};
	uint16_t i2 = UINT16_MAX, j2{};
	uint64_t i3 = UINT64_MAX, j3{};

	b.clear();

	b.put(i1);
	b.put(i2);
	b.put(i3);

	b.flip();

	b.get(j1);
	b.get(j2);
	b.get(j3);

	assert(i1 == j1);
	assert(i2 == j2);
	assert(i3 == j3);

	assert(b.has_remaining() == false);
}

void test_static1() {
	//Test static storage
	StaticByteBuffer<128> b;

	b.put("Hello Wonderful World");

	b.flip();

	std::string_view sv{};

	b.get(sv, 5);

	assert(sv == "Hello");
}

int main()
{
	test_put1();
	test_get1();
	test_get2();
	test_get3();
	test_wrapped1();
	test_get5();
	test_static1();
}
