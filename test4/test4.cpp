#include <iostream>
#include <velar.h>

struct MyData : public SocketAttachment {
    std::string type;

    MyData(const char* t) : type(t) {

    }
};

int main()
{
    Selector sel;
    ByteBuffer cli_buff(1024), srv_buff(1024);
    bool keep_running = true;

    sel.start_udp_server_ipv4(2024, std::make_shared<MyData>("SERVER"));

    //auto client = sel.start_udp_client("localhost", 2024, std::make_shared<MyData>("CLIENT"));

    //client->report_writable(true);

    while (keep_running) {
        sel.select();

        for (auto& s : sel.sockets) {
            if (s->is_writable()) {
                std::shared_ptr<MyData> attachment = std::static_pointer_cast<MyData>(s->attachment);

                if (attachment->type == "CLIENT") {
                    std::cout << "Client sending request." << std::endl;

                    std::shared_ptr<DatagramSocket> s_w = std::static_pointer_cast<DatagramSocket>(s);

                    cli_buff.clear();

                    auto sv = std::string_view("CLIENT REQUEST");

                    cli_buff.put(sv);

                    s_w->sendto(cli_buff);

                    s->report_writable(false);
                    s->report_readable(true);
                }
            } else if (s->is_readable()) {
                std::shared_ptr<MyData> attachment = std::static_pointer_cast<MyData>(s->attachment);

                if (attachment->type == "SERVER") {
                    std::cout << "Server received request." << std::endl;

                    srv_buff.clear();

                    sockaddr_in from{};
                    int from_len = sizeof(sockaddr_in);

                    int sz = s->recvfrom(srv_buff, (sockaddr*) &from, &from_len);

                    if (sz > 0) {
                        srv_buff.flip();
                        std::cout << srv_buff.to_string_view() << std::endl;

                        srv_buff.clear();
                        srv_buff.put(std::string_view("RESPONSE\r\n"));
                        srv_buff.flip();

                        int sz = s->sendto(srv_buff, (sockaddr*) &from, from_len);

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

    return 0;
}
