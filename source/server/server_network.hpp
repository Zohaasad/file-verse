
#pragma once
#include <string>
#include <vector>

struct ClientRequest {
    std::string command;
    std::vector<std::string> args;
};

struct ServerResponse {
    bool success;
    std::string message;
    std::string data;
};

ClientRequest parse_request(const std::string& raw);
std::string serialize_response(const ServerResponse& res);
