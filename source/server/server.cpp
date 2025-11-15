
#include "server.hpp"
#include <iostream>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <cstring>
#include <chrono>
#include <nlohmann/json.hpp> 
#include "../core/ofs_core.hpp"
using json = nlohmann::json;
static uint64_t now_epoch() {
    return static_cast<uint64_t>(time(nullptr));
}
OFSRequest make_request(const std::string& raw_json, int client_fd, const std::string& addr) {
    return OFSRequest{raw_json, client_fd, addr, now_epoch()};
}
OFSServer::OFSServer()
    : server_fd(-1), listen_port(8080), running(false), fs_inst(nullptr) {}

OFSServer::~OFSServer() { stop(); }

bool OFSServer::start(uint16_t port, FSInstance* _fs_inst) {
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
        timeval tv{1, 0};
        int ret = select(max_fd + 1, &read_set, nullptr, nullptr, &tv);
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
                    size_t brace;
                    while ((brace = client_buffers[fd].find('}')) != std::string::npos) {
                        std::string json_str = client_buffers[fd].substr(0, brace + 1);
                        client_buffers[fd] = client_buffers[fd].substr(brace + 1);
                        OFSRequest req = make_request(json_str, fd, "unknown");
                        op_queue.push(req);
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

    json jreq;
    try {
        jreq = json::parse(req.raw_json);
    } catch (...) {
        resp.response_json = R"({"status":"error","operation":"unknown","request_id":"0","error_code":-4,"error_message":"JSON parse error"})";
        sendResponse(resp);
        return;
    }
    std::string op = jreq.value("operation", "");
    json params = jreq.value("parameters", json::object());
    std::string session_id = jreq.value("session_id", "");
    std::string req_id = jreq.value("request_id", "0");

    int result = 0;
    json data_response;
    int error_code = 0;
    std::string error_msg;
    void* session_ptr = nullptr;
    if (session_id != "") {
        auto s_ptr = fs_inst->sessions.find(session_id);
        if (s_ptr) session_ptr = (*s_ptr).get();
    }
    if (op == "user_login") {
        std::string uname = params.value("username", "");
        std::string pwd = params.value("password", "");
        void* session_out = nullptr;
        result = user_login(&session_out, uname.c_str(), pwd.c_str());
        if (result == 0 && session_out) {
            SessionInfo* sinfo = reinterpret_cast<SessionInfo*>(session_out);
            data_response = {
                {"session_id", std::string(sinfo->session_id)},
                {"username", std::string(sinfo->user.username)}
            };
        }
    } else if (op == "user_logout") {
        if (!session_ptr) result = -9;
        else result = user_logout(session_ptr);
    } else if (op == "user_create") {
        if (!session_ptr) result = -9;
        std::string uname = params.value("username", "");
        std::string pwd = params.value("password", "");
        UserRole role = params.value("role", 0) == 1 ? UserRole::ADMIN : UserRole::NORMAL;
        result = user_create(session_ptr, uname.c_str(), pwd.c_str(), role);
    }
    else if (op == "file_create") {
        if (!session_ptr) result = -9;
        std::string path = params.value("path", "");
        std::string content = params.value("data", "");
        size_t sz = params.value("size", content.size());
        result = file_create(session_ptr, path.c_str(), content.c_str(), sz);
    }
    else if (op == "file_read") {
        if (!session_ptr) result = -9;
        std::string path = params.value("path", "");
        char* buf = nullptr;
        size_t sz = 0;
        result = file_read(session_ptr, path.c_str(), &buf, &sz);
        if (result == 0) {
            data_response = { {"data", std::string(buf, sz)}, {"size", sz} };
            free_buffer(buf);
        }
    }

    if (result < 0) {
        error_code = result;
        error_msg = get_error_message(result);
        resp.response_json = json{
            {"status", "error"},
            {"operation", op},
            {"request_id", req_id},
            {"error_code", error_code},
            {"error_message", error_msg}
        }.dump();
    } else {
        resp.response_json = json{
            {"status", "success"},
            {"operation", op},
            {"request_id", req_id},
            {"data", data_response}
        }.dump();
    }
    sendResponse(resp);
}

void OFSServer::sendResponse(const OFSResponse& resp) {
    std::string msg = resp.response_json + "\n";
    send(resp.client_fd, msg.data(), msg.size(), 0);
}
