#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cassert>
#include <cstdlib>
#include <ctime>
#include "definitions.pb.h"
#include "protobuf_codec.tpp"


static const char* socket_path = "temporary_unix_socket";
constexpr size_t page_size = sizeof(((gridware_FirmwareImagePage*)0)->page.bytes);
static const bool enable_bit_errors = false;

void err_exit(const char* s) {
    perror(s);
    exit(-1);
}

uint16_t crc16(char* data, size_t len) {
    return ~len;   // TODO: implement
}

bool server_send_page_attempt(int sock_fd, gridware_FirmwareImagePage& page) {
    ProtoBufCodec<gridware_FirmwareImagePage> page_codec;
    ProtoBufCodec<gridware_DeviceResponse> response_codec;
    gridware_DeviceResponse response;
    uint8_t msg_buf[page_codec.MaxEncodedSize()];
    uint8_t rsp_buf[response_codec.MaxEncodedSize()];

    uint16_t crc = crc16(reinterpret_cast<char*>(&page.page.bytes), page.page.size);
    memcpy(page.crc.bytes, &crc, sizeof(uint16_t));
    page.crc.size = sizeof(uint16_t);

    int bytes_encoded = page_codec.Encode(&page, msg_buf, sizeof(msg_buf));
    assert(bytes_encoded > 0);

    send(sock_fd, msg_buf, bytes_encoded, 0);
    std::cout << "Server: TX " << page.page.size << " bytes\n";

    int n = recv(sock_fd, rsp_buf, sizeof(rsp_buf), 0);
    if (n > 0) {
        int res = response_codec.Decode(&response, rsp_buf, n);
        if (!response.verified) {
            std::cout << "Server: page rejected\n";
        } else {
            std::cout << "Server: page accepted\n";
        }
        return response.verified;
    } else {
        err_exit("Server: connection failure");
        return false;
    }
}

void server_process(int sock_fd, std::ifstream& file) {
    gridware_FirmwareImagePage page;

    while (file) {
        file.read(reinterpret_cast<char*>(&page.page.bytes), page_size);
        page.page.size = file.gcount();
        if (page.page.size < page_size) {
            page.last = true;
            assert(file.eof());
        }

        bool success = false;
        for (int i = 0; !success && i < 3; i++) {
            success = server_send_page_attempt(sock_fd, page);
        }

        if (!success) {
            std::cout << "Server: Maximum page retry exceeded. Transfer failed\n";
            return;
        }
    }
}

void device_process(int sock_fd, std::ofstream& file) {
    ProtoBufCodec<gridware_FirmwareImagePage> page_codec;
    ProtoBufCodec<gridware_DeviceResponse> response_codec;
    gridware_FirmwareImagePage page;
    gridware_DeviceResponse response;
    uint8_t msg_buf[page_codec.MaxEncodedSize()];
    uint8_t rsp_buf[response_codec.MaxEncodedSize()];
    srand(time(NULL));

    while (true) {
        int n = recv(sock_fd, msg_buf, sizeof(msg_buf), 0);
        if (n <= 0) {
            err_exit("Device: connection failure");
        } else {
            int res = page_codec.Decode(&page, msg_buf, n);

            uint16_t expected_crc = crc16(reinterpret_cast<char*>(&page.page.bytes), page.page.size);
            uint16_t msg_crc;
            memcpy(&msg_crc, page.crc.bytes, sizeof(uint16_t));

            if (enable_bit_errors && !(rand() % 5)) {
                msg_crc ^= 0x1234;
            }

            if (msg_crc == expected_crc) {
                std::cout << "Device: RX " << page.page.size << " bytes\n";
                file.write(reinterpret_cast<char*>(&page.page.bytes), page.page.size);
                response.verified = true;
            } else {
                std::cout << "Invalid CRC: expected " << expected_crc << ", got " << msg_crc << "\n";

                // TODO: this doesn't work. For some reason, pb_encode writes zero bytes, but reports
                // success, when encoding a gridware_DeviceResponse with 'verified=false'.
                response.verified = false;
            }

            res = response_codec.Encode(&response, rsp_buf, sizeof(rsp_buf));
            assert(res > 0); // TODO: Encoding always fails if response.verified is false.
            if (res > 0) {
                send(sock_fd, rsp_buf, res, 0);
            }

            if (page.last && response.verified) {
                break;
            }
        }
    }

    file.close();
}

int main(int argc, const char* argv[]) {
    if (argc != 3) {
        std::cout << "Usage " << argv[0] << " infile outfile\n";
        exit(-1);
    }

    std::ifstream input_file(argv[1]);
    if (!input_file) {
        err_exit("Unable to open input file");
    }

    std::ofstream output_file(argv[2]);
    if (!output_file) {
        err_exit("Unable to open output file");
    }

    enum { DEVICE_SOCKET, SERVER_SOCKET };
    int sock_fd[2];

    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, sock_fd) < 0) {
        err_exit("Socketpair failure\n");
    }

    pid_t res = fork();
    if (res < 0) {
        err_exit("Fork failure");
    } else if (res == 0) {
        std::cout << "Device: PID " << getpid() << " starting\n";
        device_process(sock_fd[DEVICE_SOCKET], output_file);
        std::cout << "Device: PID " << getpid() << " exiting \n";
    } else {                
        std::cout << "Server: PID " << getpid() << " starting\n";
        server_process(sock_fd[SERVER_SOCKET], input_file);
        std::cout << "Server: PID " << getpid() << " exiting \n";
    }

    return 0;
}