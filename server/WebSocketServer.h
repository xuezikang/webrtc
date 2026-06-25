#pragma once

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <nlohmann/json.hpp>
#include <map>
#include <set>
#include <string>
#include <mutex>

using json = nlohmann::json;
using server = websocketpp::server<websocketpp::config::asio>;
using connection_hdl = websocketpp::connection_hdl;

struct Room {
    std::string id;
    std::set<connection_hdl, std::owner_less<connection_hdl>> peers;
};

class WebSocketServer {
public:
    WebSocketServer();
    void run(uint16_t port);

private:
    void on_open(connection_hdl hdl);
    void on_close(connection_hdl hdl);
    void on_message(connection_hdl hdl, server::message_ptr msg);

    void handle_create_room(connection_hdl hdl, const json& msg);
    void handle_join_room(connection_hdl hdl, const json& msg);
    void handle_leave_room(connection_hdl hdl, const json& msg);
    void handle_offer(connection_hdl hdl, const json& msg);
    void handle_answer(connection_hdl hdl, const json& msg);
    void handle_ice_candidate(connection_hdl hdl, const json& msg);

    void send_json(connection_hdl hdl, const json& msg);
    void broadcast_to_room(const std::string& room_id, const json& msg, connection_hdl exclude = {});

    server m_server;
    std::map<std::string, Room> m_rooms;
    std::map<connection_hdl, std::string, std::owner_less<connection_hdl>> m_peer_rooms;
    std::mutex m_mutex;
};
