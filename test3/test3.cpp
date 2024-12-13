#include <iostream>
#include <velar.h>
#include <memory>

struct MyData : public SocketAttachment {
    std::string type;

    MyData(const char* t) : type(t) {

    }
};

void server() {
    Selector sel;
    ByteBuffer buff(1024);
    bool keep_running = true;

    /*
    * Group address and port.
    */
    sel.start_multicast_server("224.0.0.251", 5454, nullptr);

    while (keep_running) {
        sel.select();

        for (auto& s : sel.sockets) {
            if (s->is_readable()) {
                std::cout << "Received a request." << std::endl;

                buff.clear();

                sockaddr_in6 from_addr{};
                int from_len = sizeof(sockaddr_in6);

                int sz = s->recvfrom(buff, (sockaddr*) &from_addr, &from_len);

                if (sz <= 0) {
                    std::cout << "Error receiving message." << std::endl;
                }
                else if (sz > 0) {
                    buff.flip();

                    auto sv = buff.to_string_view();

                    std::cout << sv << std::endl;

                    buff.clear();
                    buff.put("MULTICAST RESPONSE");

                    buff.flip();
                    sz = s->sendto(buff, (const sockaddr*)&from_addr, from_len);

                    if (sz <= 0) {
                        std::cout << "Failed to send message." << std::endl;
                    }
                    else {
                        std::cout << "Sent reply." << std::endl;
                    }
                }
            }
        }
    }
}

void client() {
    Selector sel;
    ByteBuffer buff(1024);
    bool keep_running = true;

    auto client = sel.start_udp_client("224.0.0.251", 5454, nullptr);
    
    client->report_writable(true);

    while (keep_running) {
        int n = sel.select(5);

        if (n == 0) {
            std::cout << "Timeout." << std::endl;

            return;
        }
        else if (client->is_readable()) {
            buff.clear();

            int sz = client->recvfrom(buff);

            if (sz <= 0) {
                std::cout << "Failed to receive reply." << std::endl;

                return;
            }
            else {
                std::cout << "Received response." << std::endl;

                buff.flip();

                auto sv = buff.to_string_view();

                std::cout << sv << std::endl;

                keep_running = false;
            }
        } 
        else if (client->is_writable()) {
            buff.clear();
            buff.put("MULTICAST REQUEST");
            buff.flip();
            int sz = client->sendto(buff);

            if (sz <= 0) {
                std::cout << "Failed to send request." << std::endl;

                return;
            }
            else if (sz > 0) {
                std::cout << "Sent request." << std::endl;

                client->report_writable(false);
                client->report_readable(true);
            }
        }
    }
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::cout << "Usage: program [--client | --server]" << std::endl;

        return 1;
    }
    
    std::string_view mode = argv[1];

    if (mode == "--client") {
        client();
    }
    else if (mode == "--server") {
        server();
    }
    else {
        std::cout << "Invalid mode: " << mode << std::endl;

        return 1;
    }

    return 0;
}
