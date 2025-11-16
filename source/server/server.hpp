
#pragma once
#include <string>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <queue>
#include <condition_variable>

struct OFSRequest {
    std::string raw_cmd;
    int client_fd;
    std::string addr;
    uint64_t timestamp;
};

struct OFSResponse {
    int client_fd;
    std::string response_json;
};
inline std::string make_response(const std::string& status, const std::string& operation,
                                 const std::string& request_id,
                                 const std::string& message = "",
                                 const std::string& data = "") {
    std::string json = "{";
    json += "\"status\":\"" + status + "\"";
    json += ",\"operation\":\"" + operation + "\"";
    json += ",\"request_id\":\"" + request_id + "\"";
    if (!message.empty()) json += ",\"error_message\":\"" + message + "\"";
    if (!data.empty()) json += ",\"data\":\"" + data + "\"";
    json += "}";
    return json;
}
template<typename T>
class TSQueue {
private:
    std::queue<T> q;
    std::mutex mtx;
    std::condition_variable cv;
public:
    void push(const T& item) {
        std::unique_lock<std::mutex> lock(mtx);
        q.push(item);
        cv.notify_one();
    }

    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&]{ return !q.empty(); });
        item = q.front();
        q.pop();
        return true;
    }
};

class OFSServer {
public:
    OFSServer();
    ~OFSServer();

    bool start(uint16_t port, void* fs_instance);
    void stop();

private:
    int server_fd;
    uint16_t listen_port;
    bool running;
    void* fs_inst;

    std::thread accept_thread;
    std::thread worker_thread;

    std::mutex buf_mtx;
    std::unordered_map<int, std::string> client_buffers;

    TSQueue<OFSRequest> op_queue;

    void acceptLoop();
    void workerLoop();
    void handleRequest(const OFSRequest& req);
    void sendResponse(const OFSResponse& resp);
};
