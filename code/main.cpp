#include <unistd.h>
#include "server/webserver.h"

int main() {
    WebServer server(
        1025, 3, 60000, false,
        3306, "root", "root", "yourdb",
        12, 6, true, 3, 1024);
    server.Start();
}