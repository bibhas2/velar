#include <iostream>
#include <velar.h>

void client()
{
    Selector sel;
    HeapByteBuffer cli_buff(1024);
    bool keep_running = true;

    auto client = sel.start_udp_client("localhost", 2024, nullptr);

    client->report_writable(true);

    while (keep_running) {
        int n = sel.select(5);

        if (n == 0) {
            //Timeout

            keep_running = false;

            continue;
        }

        for (auto& s : sel.sockets()) {
            if (s->is_writable()) {
                std::cout << "Client sending request." << std::endl;

                std::shared_ptr<DatagramClientSocket> s_w = std::static_pointer_cast<DatagramClientSocket>(s);

                cli_buff.clear();

                auto sv = std::string_view("CLIENT REQUEST");

                cli_buff.put(sv);

                cli_buff.flip();

                int sz = s_w->sendto(cli_buff);

                std::cout << "Sent: " << sz << std::endl;

                s->report_writable(false);
                s->report_readable(true);
            }
            else if (s->is_readable()) {
                    std::cout << "Client received response." << std::endl;

                    cli_buff.clear();

                    int sz = s->recvfrom(cli_buff, nullptr, nullptr);

                    std::cout << "Read: " << sz << std::endl;

                    if (sz > 0) {
                        cli_buff.flip();

                        std::cout << cli_buff.to_string_view() << std::endl;
                    }

                    keep_running = false;
            }
        }
    }
}

void server()
{
    Selector sel;
    //A small buffer is used to test for partial read
    HeapByteBuffer srv_buff(10);
    bool keep_running = true;

    sel.start_udp_server(2024, nullptr);

    while (keep_running) {
        int n = sel.select();

        for (auto& s : sel.sockets()) {
            if (s->is_readable()) {
                std::cout << "Server received request." << std::endl;

                srv_buff.clear();

                /*
                * Using ipv6 here presumably will work with a ipv4 source
                * client also. We tested this by disabling ipv6 in linux using these commands:
                * sudo sysctl -w net.ipv6.conf.all.disable_ipv6=1
                * sudo sysctl -w net.ipv6.conf.default.disable_ipv6=1
                */
                sockaddr_in6 from{};
                int from_len = sizeof(sockaddr_in6);

                int sz = s->recvfrom(srv_buff, (sockaddr*)&from, &from_len);

                if (sz > 0) {
                    if (from.sin6_family == AF_INET) {
                        std::cout << "Client is: AF_INET" << std::endl;
                    }
                    else if (from.sin6_family == AF_INET6) {
                        std::cout << "Client is: AF_INET6" << std::endl;
                    }
                    else {
                        std::cout << "Unknown family: " << from.sin6_family << std::endl;
                    }

                    srv_buff.flip();
                    std::cout << srv_buff.to_string_view() << std::endl;

                    srv_buff.clear();
                    srv_buff.put(std::string_view("RESPONSE\r\n"));
                    srv_buff.flip();

                    int sz = s->sendto(srv_buff, (sockaddr*)&from, from_len);

                    std::cout << "Server sent: " << sz << std::endl;
                }
                else if (sz == 0) {
                    std::cout << "Nothing was read." << std::endl;
                }
                else {
                    std::cout << "Failed to receive data" << std::endl;
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
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