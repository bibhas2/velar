#include <iostream>
#include <velar.h>

void client()
{
    Selector sel;
    ByteBuffer cli_buff(1024);
    bool keep_running = true;

    auto client = sel.start_udp_client("localhost", 2024, nullptr);

    client->report_writable(true);

    while (keep_running) {
        sel.select();

        for (auto& s : sel.sockets) {
            if (s->is_writable()) {
                std::cout << "Client sending request." << std::endl;

                std::shared_ptr<DatagramSocket> s_w = std::static_pointer_cast<DatagramSocket>(s);

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
    ByteBuffer srv_buff(1024);
    bool keep_running = true;

    sel.start_udp_server_ipv4(2024, nullptr);

    while (keep_running) {
        sel.select();

        for (auto& s : sel.sockets) {
            if (s->is_readable()) {
                std::cout << "Server received request." << std::endl;

                srv_buff.clear();

                /*
                * The server sokcet was created using ipv4 so use 
                * sockaddr_in here.
                */
                sockaddr_in from{};
                int from_len = sizeof(sockaddr_in);

                int sz = s->recvfrom(srv_buff, (sockaddr*)&from, &from_len);

                if (sz > 0) {
                    srv_buff.flip();
                    std::cout << srv_buff.to_string_view() << std::endl;

                    srv_buff.clear();
                    srv_buff.put(std::string_view("RESPONSE\r\n"));
                    srv_buff.flip();

                    int sz = s->sendto(srv_buff, (sockaddr*)&from, from_len);

                    std::cout << "Server sent: " << sz << std::endl;
                }
                else if (sz == 0) {
                    std::cout << "Client disconnected" << std::endl;
                }
                else {
                    std::cout << "Failed to receive data" << std::endl;
                }

                //keep_running = false;
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