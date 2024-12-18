#include <iostream>
#include <velar.h>

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::cout << "Usage: program file_to_map [--readonly]" << std::endl;

        return 1;
    }

    std::string_view flag{};

    if (argc == 3) {
        flag = argv[2];
    }

    bool read_only = (flag == "--readonly");
    const char* file_name = argv[1];

    try {
        MappedByteBuffer b{ file_name, read_only };

        std::cout << b.to_string_view() << std::endl;
    }
    catch (std::runtime_error err) {
        std::cout << ::GetLastError() << std::endl;

        std::cout << err.what() << std::endl;
    }
    




}
