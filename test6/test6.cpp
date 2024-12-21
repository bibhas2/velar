#include <iostream>
#include <velar.h>
#include <cassert>

/*
Create a function that works as follows.
- Creates a MappedByteBuffer object with file name "__test.dat", read only false and maximum size 128 bytes.
- Puts a uint32_t of value 10 in the buffer.
- Puts a uint64_t of value 20 in the buffer.
*/  
void test_read_write() {
    MappedByteBuffer b{ "__test.dat", false, 128 };

    uint32_t i32 = 10;
    uint64_t i64 = 20;

    b.put(i32);
    b.put(i64);
    b.put("Hello World");

    b.flip();

    b.get(i32);
    b.get(i64);

    std::string_view sv{};
    b.get(sv);

    assert(i32 == 10);
    assert(i64 == 20);
    assert(sv == "Hello World");
}

/*
Create a function called test_readonly that works as follows.
- Creates a MappedByteBuffer object with file name "__test.dat".
- Gets a uint32_t from the buffer and asserts it is 10.
- Puts a uint64_t from the buffer and asserts it is 20.
- Gets a string_view from the buffer and asserts it is "Hello World".
*/
void test_readonly() {
    MappedByteBuffer b{ "__test.dat" };

    uint32_t i32{};
    uint64_t i64{};

    b.get(i32);
    b.get(i64);

    std::string_view sv{};

    /*
    The file is larger than the data we are trying to read.
    So, we must specify the number of bytes to read.
    */
    b.get(sv, 11);

    assert(i32 == 10);
    assert(i64 == 20);
    assert(sv == "Hello World");
}    

int main(int argc, char **argv)
{
    test_read_write();
    test_readonly();

    std::remove("__test.dat");
    
    return 0;
}
