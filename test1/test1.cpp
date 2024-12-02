#include <iostream>
#include <tcp-server.h>

int main()
{
    Selector sel;
    ByteBuffer in_buff(128), out_buff(128);
    bool keep_running = true;

    auto client = sel.start_client("www.example.com", 80, nullptr);

    client->report_writable(true);
    bool request_sent = false;

    while (keep_running) {
        int n = sel.select(5);

        if (n == 0) {
            //Timeout
            std::cout << "\nTimeout detected" << std::endl;
            return 0;
        }

        for (auto& s : sel.sockets) {
            if (s->is_writable()) {
                if (!request_sent) {
                    const char* request = "GET / HTTP/1.1\r\n"
                        "Host: www.example.com\r\n"
                        "Accept: */*\r\n\r\n";

                    out_buff.clear();
                    out_buff.put(request, 0, strlen(request));
                    out_buff.flip();

                    request_sent = true;
                }

                if (out_buff.has_remaining()) {
                    s->write(out_buff);
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

                if (sz == 0) {
                    std::cout << "\nClient disconnected\n" << std::endl;

                    sel.cancel_socket(s);
                }
                else {
                    in_buff.flip();

                    auto sv = in_buff.to_string_view();

                    std::cout << sv;
                }
            }
        }
    }

    return 0;
}
