#include <iostream>
#include <velar.h>

int main()
{
    Selector sel;
    HeapByteBuffer in_buff(128), out_buff(128);
    bool keep_running = true;

    sel.start_server(9080, nullptr);

    while (keep_running) {
        sel.select();

        for (auto& s : sel.sockets()) {
            if (s->is_acceptable()) {
                std::cout << "Client connected" << std::endl;

                auto client = sel.accept(s, nullptr);

                const char* reply = "START SENDING\r\n";

                out_buff.clear();
                out_buff.put(reply, 0, strlen(reply));
                out_buff.flip();

                client->report_writable(true);
            }
            else if (s->is_readable()) {
                in_buff.clear();

                int sz = s->read(in_buff);

                if (sz < 0) {
                    std::cout << "Client disconnected" << std::endl;

                    sel.cancel_socket(s);
                }
                else if (sz > 0) {
                    in_buff.flip();

                    auto sv = in_buff.to_string_view();

                    if (sv == "quit\n") {
                        sel.cancel_socket(s);
                    }
                    else if (sv == "shutdown\n") {
                        keep_running = false;
                    }
                    else if (sv == "list\n") {
                        for (auto& s2 : sel.sockets()) {
                            std::cout
                                << " Report acceptable: "
                                << (s2->is_report_acceptable() ? "Yes" : "No")
                                << " Report readable: "
                                << (s2->is_report_readable() ? "Yes" : "No")
                                << " Report writable: "
                                << (s2->is_report_writable() ? "Yes" : "No")
                                << " Is readable: "
                                << (s2->is_readable() ? "Yes" : "No")
                                << " Is writable: "
                                << (s2->is_writable() ? "Yes" : "No")
                                << std::endl;
                        }
                    }
                }
            }
            else if (s->is_writable()) {
                if (out_buff.has_remaining()) {
                    int sz = s->write(out_buff);

                    if (sz < 0) {
                        std::cout << "Client disconnected" << std::endl;

                        sel.cancel_socket(s);
                    }
                }
                else {
                    s->report_writable(false);
                    s->report_readable(true);
                }
            }
        }
    }

    return 0;
}