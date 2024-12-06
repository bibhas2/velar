#include <iostream>
#include <velar.h>

int main()
{
    Selector sel;
    ByteBuffer in_buff(1024 * 4);
    bool keep_running = true;

    sel.start_multicast_receiver_ipv6("239.255.255.250", 1900, nullptr);

    while (keep_running) {
        sel.select();

        for (auto& s : sel.sockets) {
            if (s->is_readable()) {
                in_buff.clear();

                int sz = s->read(in_buff);

                if (sz < 0) {
                    std::cout << "Client disconnected" << std::endl;

                    sel.cancel_socket(s);
                }
                else if (sz > 0) {
                    in_buff.flip();

                    auto sv = in_buff.to_string_view();

                    std::cout << sv << std::endl;
                }
            }
        }
    }

    return 0;
}
