#include "WebSocketServer.h"
#include <iostream>
#include <random>

WebSocketServer::WebSocketServer() {
    m_server.init_asio();
    m_server.set_open_handler(std::bind(&WebSocketServer::on_open, this, std::placeholders::_1));
    m_server.set_close_handler(std::bind(&WebSocketServer::on_close, this, std::placeholders::_1));
    m_server.set_message_handler(std::bind(&WebSocketServer::on_message, this, std::placeholders::_1, std::placeholders::_2));
}

void WebSocketServer::run(uint16_t port) {
    m_server.listen(port);
    m_server.start_accept();
    std::cout << "Signaling server running on port " << port << std::endl;
    m_server.run();
}

std::string WebSocketServer::generate_peer_id() {
    return "peer_" + std::to_string(++m_peer_counter);
}

void WebSocketServer::on_open(connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string peer_id = generate_peer_id();
    m_peer_ids[hdl] = peer_id;
    std::cout << "Client connected: " << peer_id << std::endl;
}

void WebSocketServer::on_close(connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string peer_id = m_peer_ids.count(hdl) ? m_peer_ids[hdl] : "unknown";
    auto it = m_peer_rooms.find(hdl);
    if (it != m_peer_rooms.end()) {
        std::string room_id = it->second;
        auto room_it = m_rooms.find(room_id);
        if (room_it != m_rooms.end()) {
            room_it->second.peers.erase(hdl);
            if (room_it->second.peers.empty()) {
                m_rooms.erase(room_it);
            } else {
                json notify = {{"type", "peer_left"}, {"room_id", room_id}, {"peer_id", peer_id}};
                broadcast_to_room(room_id, notify);
            }
        }
        m_peer_rooms.erase(it);
    }
    m_peer_ids.erase(hdl);
    std::cout << "Client disconnected: " << peer_id << std::endl;
}

void WebSocketServer::on_message(connection_hdl hdl, server::message_ptr msg) {
    try {
        json j = json::parse(msg->get_payload());
        std::string type = j["type"];

        if (type == "create_room") {
            handle_create_room(hdl, j);
        } else if (type == "join_room") {
            handle_join_room(hdl, j);
        } else if (type == "leave_room") {
            handle_leave_room(hdl, j);
        } else if (type == "offer") {
            handle_offer(hdl, j);
        } else if (type == "answer") {
            handle_answer(hdl, j);
        } else if (type == "ice_candidate") {
            handle_ice_candidate(hdl, j);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error processing message: " << e.what() << std::endl;
    }
}

std::string generate_room_id() {
    static const char chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::string id;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);
    for (int i = 0; i < 6; ++i) {
        id += chars[dis(gen)];
    }
    return id;
}

void WebSocketServer::handle_create_room(connection_hdl hdl, const json& msg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string room_id = generate_room_id();
    std::string peer_id = m_peer_ids.count(hdl) ? m_peer_ids[hdl] : "unknown";

    Room room;
    room.id = room_id;
    room.peers.insert(hdl);
    m_rooms[room_id] = room;
    m_peer_rooms[hdl] = room_id;

    json response = {
        {"type", "room_created"},
        {"room_id", room_id}
    };
    send_json(hdl, response);
    std::cout << "Room created: " << room_id << " by " << peer_id << std::endl;
}

void WebSocketServer::handle_join_room(connection_hdl hdl, const json& msg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string room_id = msg["room_id"];
    std::string peer_id = m_peer_ids.count(hdl) ? m_peer_ids[hdl] : "unknown";

    auto it = m_rooms.find(room_id);
    if (it == m_rooms.end()) {
        json error = {{"type", "error"}, {"message", "Room not found"}};
        send_json(hdl, error);
        return;
    }

    // 先通知已在房间的人：有新 peer 加入
    json notify = {
        {"type", "peer_joined"},
        {"room_id", room_id},
        {"peer_id", peer_id}
    };
    broadcast_to_room(room_id, notify);

    // 把已在房间的 peer 列表发给新加入者
    for (auto& existing_peer : it->second.peers) {
        std::string existing_id = m_peer_ids.count(existing_peer) ? m_peer_ids[existing_peer] : "unknown";
        json existing_notify = {
            {"type", "peer_joined"},
            {"room_id", room_id},
            {"peer_id", existing_id}
        };
        send_json(hdl, existing_notify);
    }

    it->second.peers.insert(hdl);
    m_peer_rooms[hdl] = room_id;

    json response = {
        {"type", "room_joined"},
        {"room_id", room_id},
        {"peer_count", static_cast<int>(it->second.peers.size())}
    };
    send_json(hdl, response);
    std::cout << peer_id << " joined room: " << room_id << std::endl;
}

void WebSocketServer::handle_leave_room(connection_hdl hdl, const json& msg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_peer_rooms.find(hdl);
    if (it == m_peer_rooms.end()) return;

    std::string room_id = it->second;
    std::string peer_id = m_peer_ids.count(hdl) ? m_peer_ids[hdl] : "unknown";

    auto room_it = m_rooms.find(room_id);
    if (room_it != m_rooms.end()) {
        room_it->second.peers.erase(hdl);
        if (room_it->second.peers.empty()) {
            m_rooms.erase(room_it);
        } else {
            json notify = {{"type", "peer_left"}, {"room_id", room_id}, {"peer_id", peer_id}};
            broadcast_to_room(room_id, notify);
        }
    }
    m_peer_rooms.erase(it);
}

void WebSocketServer::handle_offer(connection_hdl hdl, const json& msg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string room_id = msg["room_id"];
    std::string peer_id = m_peer_ids.count(hdl) ? m_peer_ids[hdl] : "unknown";

    json offer_msg = {
        {"type", "offer"},
        {"sdp", msg["sdp"]},
        {"room_id", room_id},
        {"peer_id", peer_id}
    };
    broadcast_to_room(room_id, offer_msg, hdl);
}

void WebSocketServer::handle_answer(connection_hdl hdl, const json& msg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string room_id = msg["room_id"];
    std::string peer_id = m_peer_ids.count(hdl) ? m_peer_ids[hdl] : "unknown";

    json answer_msg = {
        {"type", "answer"},
        {"sdp", msg["sdp"]},
        {"room_id", room_id},
        {"peer_id", peer_id}
    };
    broadcast_to_room(room_id, answer_msg, hdl);
}

void WebSocketServer::handle_ice_candidate(connection_hdl hdl, const json& msg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string room_id = msg["room_id"];
    std::string peer_id = m_peer_ids.count(hdl) ? m_peer_ids[hdl] : "unknown";

    json ice_msg = {
        {"type", "ice_candidate"},
        {"candidate", msg["candidate"]},
        {"sdpMid", msg["sdpMid"]},
        {"sdpMLineIndex", msg["sdpMLineIndex"]},
        {"room_id", room_id},
        {"peer_id", peer_id}
    };
    broadcast_to_room(room_id, ice_msg, hdl);
}

void WebSocketServer::send_json(connection_hdl hdl, const json& msg) {
    m_server.send(hdl, msg.dump(), websocketpp::frame::opcode::text);
}

void WebSocketServer::broadcast_to_room(const std::string& room_id, const json& msg, connection_hdl exclude) {
    auto it = m_rooms.find(room_id);
    if (it == m_rooms.end()) return;

    for (auto& peer : it->second.peers) {
        if (!exclude.lock() || peer.lock() != exclude.lock()) {
            send_json(peer, msg);
        }
    }
}
