#include "server/server.hpp"
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=2) return 2;
    wb::LocalServer server(argv[1]);
    if(!server.start()) return 1;
    // Private pipe to the automated browser test; never log the bootstrap URL.
    std::cout << "http://127.0.0.1:" << server.port() << "/session?token=" << server.token() << std::endl;
    std::string stop;std::getline(std::cin,stop);
    server.stop();return 0;
}
