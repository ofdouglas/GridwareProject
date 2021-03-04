#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "definitions.pb.h"
#include "protobuf_codec.tpp"


/*
* Read binary file (server)
* Generate multiple FirmwareImagePage messages
* Send each page to device
* Device decodes and returns a response (true iff CRC correct)
*
* Simulation: spawn two threads. Have the server read a
* binary file, encode&send chunk by chunk, to device.
*/

void err_exit(const char* s) {
    perror(s);
    exit(-1);
}

static const char* socket_path = "temporary_unix_socket";

void server_process(int sock_fd, std::ifstream& fw_image) {
    std::cout << "Server process " << getpid() << " starting\n";

    for (int i = 0; i < 10; i++) {
        const char* msg = "hello\n";
        int res = send(sock_fd, msg, strlen(msg) + 1, 0);
        //sleep(1);
    }

    std::cout << "Server process exiting\n";
}

void device_process(int sock_fd, pid_t ppid) {
    std::cout << "Device process " << getpid() << " starting\n";

    char buf[1024] = {};
    int n = 0;
    size_t total = 0;

    while (getppid() == ppid) { // PPID will change when the original parent dies
         n = recv(sock_fd, buf, sizeof(buf), MSG_DONTWAIT);

         if (n > 0) {
            std::cout << "Device received " << n << " bytes\n";
             for (size_t i = 0; i < n; i++)
                putchar(buf[i]);
             total += n;
         }
    }

    std::cout << "Device processing exiting, " << n << " bytes received\n";
}

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage " << argv[0] << " filename" << std::endl;;
        exit(-1);
    }

    std::ifstream input_file(argv[1]);
    if (!input_file) {
        err_exit("Unable to read input file");
    }

    enum { DEVICE_SOCKET, SERVER_SOCKET };
    int sock_fd[2];

    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, sock_fd) < 0) {
        err_exit("Socketpair failure\n");
    }

    pid_t ppid = getpid();
    pid_t res = fork();
    if (res < 0) {
        err_exit("Fork failure");
    } else if (res == 0) {  
        device_process(sock_fd[DEVICE_SOCKET], ppid);        // Child: device process
    } else {                
        server_process(sock_fd[SERVER_SOCKET], input_file);  // Parent: server process
    }

    return 0;
}