#include <iostream>
#include <velar.h>
#include <memory>

struct MyData : public SocketAttachment {
    std::string type;

    MyData(const char* t) : type(t) {

    }
};

int main()
{
    Selector sel;
    ByteBuffer in_buff(1024 * 4);
    bool keep_running = true;

    /*
    * Use netcat like this to send messages to this port.
    * 
    * nc -4u 2024
    */
    sel.start_udp_server_ipv4(2024, std::make_shared<MyData>("UDP"));

    /*
    * Many household routers will send multicast messages to this group
    * IP and port.
    */
    sel.start_multicast_server_ipv4("239.255.255.250", 1900, std::make_shared<MyData>("UDP MULTICAST"));

    while (keep_running) {
        sel.select();

        for (auto& s : sel.sockets) {
            if (s->is_readable()) {
                std::shared_ptr<MyData> attachment = std::static_pointer_cast<MyData>(s->attachment);

                std::cout << "Messeage received by: " << attachment->type << std::endl;

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
