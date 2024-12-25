#include <iostream>
#include <velar.h>
#include <cassert>

void test_read_write_new() {
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

void test_read_write_existing() {
    MappedByteBuffer b{ "__test.dat", false};

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
    test_read_write_new();
    test_read_write_existing();
    test_readonly();

    std::remove("__test.dat");
    
    return 0;
}
