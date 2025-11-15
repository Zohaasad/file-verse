
#include "server_network.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>
static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\n\r");
    auto end   = s.find_last_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    return s.substr(start, end - start + 1);
}
ClientRequest parse_request(const std::string& raw) {
    ClientRequest req;
    std::string s = trim(raw);
    std::istringstream iss(s);
    iss >> req.command;
    if (req.command.empty()) return req;
    char c;
    while (iss >> std::ws) {
        if (iss.peek() == '"') {
            iss.get(); 
            std::string arg;
            std::getline(iss, arg, '"');
            req.args.push_back(arg);
        } else {
            std::string arg;
            iss >> arg;
            req.args.push_back(arg);
        }
    }
    return req;
}
std::string serialize_response(const ServerResponse& res) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"success\":" << (res.success ? "true" : "false") << ",";
    oss << "\"message\":\"" << res.message << "\"";

    if (!res.data.empty()) {
        oss << ",\"data\":\"";
        for (char c : res.data) {
            if (c == '\"') oss << "\\\"";
            else oss << c;
        }
        oss << "\"";
    }
    oss << "}\n";
    return oss.str();
}
