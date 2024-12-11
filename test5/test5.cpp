// test5.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <velar.h>
#include <assert.h>

void test_put1() {
	ByteBuffer b(128);

	b.put("Hello World");
	b.put('!');

	b.flip();

	assert(b.remaining() == 12);
}

void test_get1() {
	ByteBuffer b(128);

	b.put("Hello World");

	b.flip();

	char dest[5];

	b.get(dest, 0, 5);

	std::string_view sv(dest, 5), expected("Hello");

	assert(sv == expected);

	assert(b.position == 5);

	expected = " World";

	assert(b.to_string_view() == expected);
}

void test_get2() {
	ByteBuffer b(128);

	b.put("Hello World");

	b.flip();

	char ch;

	b.get(ch);
	assert(ch == 'H');

	b.get(ch);
	assert(ch == 'e');

	assert(b.to_string_view() == "llo World");
}

int main()
{
	test_put1();
	test_get1();
	test_get2();
}
