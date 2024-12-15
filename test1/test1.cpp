#include <iostream>
#include <velar.h>

int main()
{
    Selector sel;
    ByteBuffer in_buff(128), out_buff(128);

    sel.start_client("www.example.com", 80, nullptr);

    while (true) {
        int n = sel.select(2);

        if (n == 0) {
            //Timeout
            std::cout << "\nTimeout detected" << std::endl;
            return 0;
        }

        for (auto& s : sel.sockets) {
            if (s->is_connection_failed()) {
                std::cout << "Connection failed." << std::endl;

                return 1;
            }
            else if (s->is_connection_success()) {
                std::cout << "Connection success." << std::endl;

                //Prepare the request
                const char* request = "GET / HTTP/1.1\r\n"
                    "Host: www.example.com\r\n"
                    "Accept: */*\r\n\r\n";

                out_buff.clear();
                out_buff.put(request);

                //Start writing the request
                out_buff.flip();
                s->report_writable(true);
            }
            else if (s->is_writable()) {
                if (out_buff.has_remaining()) {
                    int sz = s->write(out_buff);

                    if (sz < 0) {
                        std::cout << "Server disconnected\n" << std::endl;

                        return 1;
                    }
                }
                else {
                    //We are done writing request
                    s->report_writable(false);
                    //Start reading
                    s->report_readable(true);
                }
            }
            else if (s->is_readable()) {
                //Read as much data as available
                in_buff.clear();
                int sz = s->read(in_buff);

                if (sz < 0) {
                    std::cout << "Server disconnected\n" << std::endl;

                    return 1;
                }
                else if (sz > 0) {
                    in_buff.flip();

                    std::cout << in_buff.to_string_view();
                }
            }
        }
    }

    return 0;
}
