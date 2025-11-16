
#include "server.hpp"
#include "../core/ofs_core.hpp"
#include "../core/ofs_instance.hpp"

#include <iostream>
#include <vector>
#include <cstring>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <thread>
#include <arpa/inet.h>
#include <ctime>

static uint64_t now_epoch() {
    return static_cast<uint64_t>(time(nullptr));
}

OFSRequest make_request(const std::string& raw_cmd, int fd, const std::string& addr) {
    return OFSRequest{raw_cmd, fd, addr, now_epoch()};
}

OFSServer::OFSServer() : server_fd(-1), listen_port(8080), running(false), fs_inst(nullptr) {}
OFSServer::~OFSServer() { stop(); }

bool OFSServer::start(uint16_t port, void* _fs_inst) {
    listen_port = port;
    fs_inst = _fs_inst;
    running = true;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { std::cerr << "Socket creation failed\n"; return false; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(listen_port);

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Bind error\n"; close(server_fd); return false;
    }
    if (listen(server_fd, 20) < 0) {
        std::cerr << "Listen error\n"; close(server_fd); return false;
    }

    accept_thread = std::thread(&OFSServer::acceptLoop, this);
    worker_thread = std::thread(&OFSServer::workerLoop, this);

    std::cout << "[OFS] Server started, listening on port " << listen_port << std::endl;
    return true;
}

void OFSServer::stop() {
    running = false;
    if (server_fd >= 0) close(server_fd);
    if (accept_thread.joinable()) accept_thread.join();
    if (worker_thread.joinable()) worker_thread.join();
}

void OFSServer::acceptLoop() {
    fd_set master_set, read_set;
    FD_ZERO(&master_set);
    FD_SET(server_fd, &master_set);
    int max_fd = server_fd;
    std::vector<int> client_fds;

    while (running) {
        read_set = master_set;
        timeval tv{1,0};
        int ret = select(max_fd+1, &read_set, nullptr, nullptr, &tv);
        if (ret < 0) continue;

        if (FD_ISSET(server_fd, &read_set)) {
            sockaddr_in cli_addr{};
            socklen_t cli_len = sizeof(cli_addr);
            int cli_fd = accept(server_fd, (sockaddr*)&cli_addr, &cli_len);
            if (cli_fd >= 0) {
                fcntl(cli_fd, F_SETFL, O_NONBLOCK);
                FD_SET(cli_fd, &master_set);
                if (cli_fd > max_fd) max_fd = cli_fd;
                client_fds.push_back(cli_fd);
                std::cout << "[OFS] New connection: FD " << cli_fd << std::endl;
            }
        }

        for (auto it = client_fds.begin(); it != client_fds.end(); ) {
            int fd = *it;
            if (FD_ISSET(fd, &read_set)) {
                char buf[2048];
                ssize_t n = recv(fd, buf, sizeof(buf), 0);
                if (n <= 0) {
                    std::cout << "[OFS] Client disconnected: FD " << fd << std::endl;
                    close(fd);
                    FD_CLR(fd, &master_set);
                    it = client_fds.erase(it);
                    continue;
                }
                std::string data(buf, n);
                {
                    std::lock_guard<std::mutex> lock(buf_mtx);
                    client_buffers[fd] += data;
                    size_t pos;
                    while ((pos = client_buffers[fd].find('\n')) != std::string::npos) {
                        std::string cmd = client_buffers[fd].substr(0, pos);
                        client_buffers[fd] = client_buffers[fd].substr(pos+1);
                        op_queue.push(make_request(cmd, fd, "unknown"));
                    }
                }
            }
            ++it;
        }
    }
}

void OFSServer::workerLoop() {
    while (running) {
        OFSRequest req;
        if (op_queue.pop(req)) {
            handleRequest(req);
        }
    }
}

void OFSServer::handleRequest(const OFSRequest& req) {
    OFSResponse resp;
    resp.client_fd = req.client_fd;

    std::string cmd = req.raw_cmd;
    std::string operation = "unknown";
    std::string request_id = "0";
    std::string message = "Command executed";

    // Here, parse cmd as needed and call filesystem functions
    // Example: "login user pass"
    if (cmd.rfind("login", 0) == 0) operation = "user_login";
    else if (cmd.rfind("logout", 0) == 0) operation = "user_logout";

    resp.response_json = make_response("success", operation, request_id, "", cmd);
    sendResponse(resp);
}

void OFSServer::sendResponse(const OFSResponse& resp) {
    std::string msg = resp.response_json + "\n";
    send(resp.client_fd, msg.data(), msg.size(), 0);
}
