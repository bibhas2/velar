#include <iostream>
#include <velar.h>

int main()
{
    Selector sel;
    ByteBuffer in_buff(128), out_buff(128);
    bool keep_running = true;

    sel.start_client("www.example.com", 80, nullptr);

    while (keep_running) {
        int n = sel.select(10);

        if (n == 0) {
            //Timeout
            std::cout << "\nTimeout detected" << std::endl;
            return 0;
        }

        for (auto& s : sel.sockets) {
            if (s->is_connection_failed()) {
                std::cout << "Connection failed." << std::endl;

                keep_running = false;
            }
            else if (s->is_connection_success()) {
                std::cout << "Connection success." << std::endl;

                //Prepare the request
                const char* request = "GET / HTTP/1.1\r\n"
                    "Host: www.example.com\r\n"
                    "Accept: */*\r\n\r\n";

                out_buff.clear();
                out_buff.put(request, 0, strlen(request));
                out_buff.flip();

                //Start writing the request
                s->report_writable(true);
            }
            else if (s->is_writable()) {
                if (out_buff.has_remaining()) {
                    int sz = s->write(out_buff);

                    if (sz < 0) {
                        std::cout << "\nClient disconnected\n" << std::endl;

                        sel.cancel_socket(s);
                    }
                }
                else {
                    //Stop writing
                    s->report_writable(false);
                    //Start reading
                    s->report_readable(true);
                }
            }
            else if (s->is_readable()) {
                in_buff.clear();

                //Read as much as available
                int sz = s->read(in_buff);

                if (sz < 0) {
                    std::cout << "\nClient disconnected\n" << std::endl;

                    sel.cancel_socket(s);
                }
                else if (sz > 0) {
                    in_buff.flip();

                    auto sv = in_buff.to_string_view();

                    std::cout << sv;
                }
            }
        }
    }

    return 0;
}
