
#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <string>
#include <atomic>
#include <map>
#include <vector>
#include <memory>
#include "../core/ofs_instance.hpp"
struct OFSRequest {
    std::string raw_json;        
    int client_fd;                
    std::string client_addr;
    uint64_t request_time;        
};

struct OFSResponse {
    std::string response_json;   
    int client_fd;
};
class OperationQueue {
    std::queue<OFSRequest> queue;
    std::mutex mtx;
    std::condition_variable cv;
public:
    void push(const OFSRequest& req) {
        std::lock_guard<std::mutex> lock(mtx);
        queue.push(req);
        cv.notify_one();
    }
    bool pop(OFSRequest& out) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]{ return !queue.empty(); });
        out = queue.front();
        queue.pop();
        return true;
    }
    bool empty() {
        std::lock_guard<std::mutex> lock(mtx);
        return queue.empty();
    }
};
class OFSServer {
    int server_fd;
    uint16_t listen_port;
    std::atomic<bool> running;
    std::thread accept_thread;
    std::thread worker_thread;
    OperationQueue op_queue;
    std::map<int, std::string> client_buffers;
    std::mutex buf_mtx;

    FSInstance* fs_inst;

    void acceptLoop();
    void workerLoop();
    void handleRequest(const OFSRequest& req);
    void sendResponse(const OFSResponse& resp);
public:
    OFSServer();
    ~OFSServer();
    bool start(uint16_t port, FSInstance* fs_inst);
    void stop();
};
