#include "WebRTCManager.h"
#include <iostream>

WebRTCManager* WebRTCManager::s_instance = nullptr;

WebRTCManager::WebRTCManager(QObject* parent)
    : QObject(parent)
    , m_webSocket(new QWebSocket())
    , m_localPipeline(nullptr)
    , m_localSink(nullptr)
{
    s_instance = this;
    connect(m_webSocket, &QWebSocket::connected, this, &WebRTCManager::onConnected);
    connect(m_webSocket, &QWebSocket::disconnected, this, &WebRTCManager::onDisconnected);
    connect(m_webSocket, &QWebSocket::textMessageReceived, this, &WebRTCManager::onTextMessageReceived);
}

WebRTCManager::~WebRTCManager() {
    leaveRoom();
    stopLocalPipeline();
    s_instance = nullptr;
}

void WebRTCManager::connectToServer(const QString& url) {
    m_webSocket->open(QUrl(url));
}

void WebRTCManager::createRoom() {
    json msg = {{"type", "create_room"}};
    m_webSocket->sendTextMessage(QString::fromStdString(msg.dump()));
}

void WebRTCManager::joinRoom(const QString& roomId) {
    m_roomId = roomId;
    json msg = {{"type", "join_room"}, {"room_id", roomId.toStdString()}};
    m_webSocket->sendTextMessage(QString::fromStdString(msg.dump()));
}

void WebRTCManager::leaveRoom() {
    if (!m_roomId.isEmpty()) {
        json msg = {{"type", "leave_room"}, {"room_id", m_roomId.toStdString()}};
        m_webSocket->sendTextMessage(QString::fromStdString(msg.dump()));
        m_roomId.clear();
    }
    // 清理所有 peer 连接
    for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
        if (it->pipeline) {
            gst_element_set_state(it->pipeline, GST_STATE_NULL);
            gst_object_unref(it->pipeline);
        }
    }
    m_peers.clear();
    m_webrtcbinToPeerId.clear();
}

void WebRTCManager::onConnected() {
    emit connected();
    setupLocalPipeline();
}

void WebRTCManager::onDisconnected() {
    emit disconnected();
}

void WebRTCManager::onTextMessageReceived(const QString& message) {
    try {
        json msg = json::parse(message.toStdString());
        handleMessage(msg);
    } catch (const std::exception& e) {
        std::cerr << "Error parsing message: " << e.what() << std::endl;
    }
}

// ============================================================
// 本地摄像头 Pipeline
// ============================================================

void WebRTCManager::setupLocalPipeline() {
    GError* error = nullptr;

    // 本地预览: v4l2src → videoconvert → autovideosink
    const char* pipeline_str =
        "v4l2src ! video/x-raw,width=320,height=240,framerate=15/1 ! "
        "videoconvert ! autovideosink name=localsink";

    m_localPipeline = gst_parse_launch(pipeline_str, &error);
    if (error) {
        std::cerr << "Local pipeline error: " << error->message << std::endl;
        g_error_free(error);
        return;
    }

    m_localSink = gst_bin_get_by_name(GST_BIN(m_localPipeline), "localsink");
    gst_element_set_state(m_localPipeline, GST_STATE_PLAYING);

    // 通知 UI 本地视频就绪
    emit localVideoReady(m_localSink);
}

void WebRTCManager::stopLocalPipeline() {
    if (m_localPipeline) {
        gst_element_set_state(m_localPipeline, GST_STATE_NULL);
        gst_object_unref(m_localPipeline);
        m_localPipeline = nullptr;
        m_localSink = nullptr;
    }
}

// ============================================================
// 每个 Peer 的 WebRTC 连接
// ============================================================

void WebRTCManager::createPeerConnection(const QString& peerId, bool isInitiator) {
    if (m_peers.contains(peerId)) return;

    GError* error = nullptr;

    // 每个 peer 独立 pipeline: 摄像头 → H264编码 → webrtcbin
    // 远端流通过 pad-added 动态接入解码+显示
    const char* pipeline_str =
        "v4l2src ! video/x-raw,width=640,height=480,framerate=30/1 ! "
        "videoconvert ! x264enc tune=zerolatency bitrate=800 speed-preset=ultrafast key-int-max=30 ! "
        "rtph264pay config-interval=1 pt=96 ! "
        "application/x-rtp,media=video,encoding-name=H264,payload=96 ! "
        "webrtcbin name=sendrecv stun-server=stun://0.0.0.0:0";

    GstElement* pipeline = gst_parse_launch(pipeline_str, &error);
    if (error) {
        std::cerr << "Peer pipeline error: " << error->message << std::endl;
        g_error_free(error);
        return;
    }

    GstElement* webrtcbin = gst_bin_get_by_name(GST_BIN(pipeline), "sendrecv");
    if (!webrtcbin) {
        std::cerr << "Failed to get webrtcbin for peer: " << peerId.toStdString() << std::endl;
        gst_object_unref(pipeline);
        return;
    }

    // 记录映射关系
    m_webrtcbinToPeerId[webrtcbin] = peerId;

    // 连接信号 (user_data 传 webrtcbin，回调中通过映射找到 peerId)
    g_signal_connect(webrtcbin, "on-negotiation-needed", G_CALLBACK(on_negotiation_needed), webrtcbin);
    g_signal_connect(webrtcbin, "on-ice-candidate", G_CALLBACK(on_ice_candidate), webrtcbin);
    g_signal_connect(webrtcbin, "pad-added", G_CALLBACK(on_incoming_stream), webrtcbin);

    // 保存连接信息
    PeerConnection conn;
    conn.webrtcbin = webrtcbin;
    conn.pipeline = pipeline;
    conn.isInitiator = isInitiator;
    m_peers[peerId] = conn;

    // 启动 pipeline
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    // 如果是发起方，等 negotiation-needed 信号会自动触发 offer
}

void WebRTCManager::removePeerConnection(const QString& peerId) {
    auto it = m_peers.find(peerId);
    if (it == m_peers.end()) return;

    m_webrtcbinToPeerId.remove(it->webrtcbin);
    gst_element_set_state(it->pipeline, GST_STATE_NULL);
    gst_object_unref(it->pipeline);
    m_peers.erase(it);
}

// ============================================================
// GStreamer 静态回调
// ============================================================

void WebRTCManager::on_negotiation_needed(GstElement* webrtcbin, gpointer user_data) {
    WebRTCManager* self = s_instance;
    if (!self) return;

    QString peerId = self->m_webrtcbinToPeerId.value(webrtcbin);
    if (peerId.isEmpty()) return;

    auto it = self->m_peers.find(peerId);
    if (it != self->m_peers.end() && it->isInitiator) {
        self->sendOffer(peerId);
    }
}

void WebRTCManager::on_ice_candidate(GstElement* webrtcbin, guint mlineindex, gchar* candidate, gpointer user_data) {
    WebRTCManager* self = s_instance;
    if (!self) return;

    QString peerId = self->m_webrtcbinToPeerId.value(webrtcbin);
    if (!peerId.isEmpty()) {
        self->sendIceCandidate(peerId, mlineindex, candidate);
    }
}

void WebRTCManager::on_offer_created(GstPromise* promise, gpointer user_data) {
    // user_data 是 webrtcbin
    GstElement* webrtcbin = static_cast<GstElement*>(user_data);
    WebRTCManager* self = s_instance;
    if (!self) return;

    QString peerId = self->m_webrtcbinToPeerId.value(webrtcbin);
    if (peerId.isEmpty()) return;

    const GstStructure* reply = gst_promise_get_reply(promise);
    GstWebRTCSessionDescription* offer = nullptr;
    gst_structure_get(reply, "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offer, nullptr);
    gst_promise_unref(promise);

    if (offer) {
        GstPromise* local_promise = gst_promise_new();
        g_signal_emit_by_name(webrtcbin, "set-local-description", offer, local_promise);
        gst_promise_interrupt(local_promise);
        gst_promise_unref(local_promise);

        gchar* sdp_str = gst_sdp_message_as_text(offer->sdp);
        json msg = {
            {"type", "offer"},
            {"sdp", sdp_str},
            {"room_id", self->m_roomId.toStdString()}
        };
        self->m_webSocket->sendTextMessage(QString::fromStdString(msg.dump()));
        g_free(sdp_str);
        gst_webrtc_session_description_free(offer);
    }
}

void WebRTCManager::on_answer_created(GstPromise* promise, gpointer user_data) {
    GstElement* webrtcbin = static_cast<GstElement*>(user_data);
    WebRTCManager* self = s_instance;
    if (!self) return;

    QString peerId = self->m_webrtcbinToPeerId.value(webrtcbin);
    if (peerId.isEmpty()) return;

    const GstStructure* reply = gst_promise_get_reply(promise);
    GstWebRTCSessionDescription* answer = nullptr;
    gst_structure_get(reply, "answer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &answer, nullptr);
    gst_promise_unref(promise);

    if (answer) {
        GstPromise* local_promise = gst_promise_new();
        g_signal_emit_by_name(webrtcbin, "set-local-description", answer, local_promise);
        gst_promise_interrupt(local_promise);
        gst_promise_unref(local_promise);

        gchar* sdp_str = gst_sdp_message_as_text(answer->sdp);
        json msg = {
            {"type", "answer"},
            {"sdp", sdp_str},
            {"room_id", self->m_roomId.toStdString()}
        };
        self->m_webSocket->sendTextMessage(QString::fromStdString(msg.dump()));
        g_free(sdp_str);
        gst_webrtc_session_description_free(answer);
    }
}

void WebRTCManager::on_incoming_stream(GstElement* webrtcbin, GstPad* pad, gpointer user_data) {
    WebRTCManager* self = s_instance;
    if (!self) return;

    QString peerId = self->m_webrtcbinToPeerId.value(webrtcbin);
    if (peerId.isEmpty()) return;

    GstCaps* caps = gst_pad_get_current_caps(pad);
    if (!caps) caps = gst_pad_query_caps(pad, nullptr);
    const GstStructure* s = gst_caps_get_structure(caps, 0);
    const gchar* name = gst_structure_get_name(s);

    if (g_str_has_prefix(name, "application/x-rtp")) {
        auto it = self->m_peers.find(peerId);
        if (it == self->m_peers.end()) { gst_caps_unref(caps); return; }

        // 动态创建远端解码+显示管线
        GstElement* queue = gst_element_factory_make("queue", nullptr);
        GstElement* depay = gst_element_factory_make("rtph264depay", nullptr);
        GstElement* dec = gst_element_factory_make("avdec_h264", nullptr);
        GstElement* conv = gst_element_factory_make("videoconvert", nullptr);
        GstElement* sink = gst_element_factory_make("autovideosink", nullptr);

        gst_bin_add_many(GST_BIN(it->pipeline), queue, depay, dec, conv, sink, nullptr);
        gst_element_sync_state_with_parent(queue);
        gst_element_sync_state_with_parent(depay);
        gst_element_sync_state_with_parent(dec);
        gst_element_sync_state_with_parent(conv);
        gst_element_sync_state_with_parent(sink);
        gst_element_link_many(queue, depay, dec, conv, sink, nullptr);

        GstPad* sink_pad = gst_element_get_static_pad(queue, "sink");
        gst_pad_link(pad, sink_pad);
        gst_object_unref(sink_pad);

        // 通知 UI 远端视频就绪
        emit self->remoteVideoReady(peerId, sink);
    }
    gst_caps_unref(caps);
}

// ============================================================
// SDP / ICE 发送
// ============================================================

void WebRTCManager::sendOffer(const QString& peerId) {
    auto it = m_peers.find(peerId);
    if (it == m_peers.end()) return;

    GstPromise* promise = gst_promise_new_with_change_func(on_offer_created, it->webrtcbin, nullptr);
    g_signal_emit_by_name(it->webrtcbin, "create-offer", nullptr, promise);
}

void WebRTCManager::sendAnswer(const QString& peerId, const GstSDPMessage* sdp) {
    auto it = m_peers.find(peerId);
    if (it == m_peers.end()) return;

    GstSDPMessage* copy = nullptr;
    gst_sdp_message_copy(sdp, &copy);
    GstPromise* promise = gst_promise_new_with_change_func(on_answer_created, it->webrtcbin, nullptr);
    GstWebRTCSessionDescription* answer = gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_ANSWER, copy);
    g_signal_emit_by_name(it->webrtcbin, "set-remote-description", answer, nullptr);
    gst_webrtc_session_description_free(answer);
    g_signal_emit_by_name(it->webrtcbin, "create-answer", nullptr, promise);
}

void WebRTCManager::sendIceCandidate(const QString& peerId, guint mlineindex, const gchar* candidate) {
    json msg = {
        {"type", "ice_candidate"},
        {"candidate", candidate},
        {"sdpMid", ""},
        {"sdpMLineIndex", mlineindex},
        {"room_id", m_roomId.toStdString()}
    };
    m_webSocket->sendTextMessage(QString::fromStdString(msg.dump()));
}

// ============================================================
// 信令消息处理
// ============================================================

void WebRTCManager::handleMessage(const json& msg) {
    std::string type = msg["type"];

    if (type == "room_created") {
        m_roomId = QString::fromStdString(msg["room_id"]);
        emit roomCreated(m_roomId);
    } else if (type == "room_joined") {
        emit roomJoined(m_roomId);
    } else if (type == "peer_joined") {
        QString peerId = QString::fromStdString(msg.value("peer_id", "unknown"));
        emit peerJoined(peerId);
        // 新 peer 加入，作为发起方创建连接
        createPeerConnection(peerId, true);
    } else if (type == "peer_left") {
        QString peerId = QString::fromStdString(msg.value("peer_id", "unknown"));
        removePeerConnection(peerId);
        emit peerLeft(peerId);
    } else if (type == "offer") {
        handleOffer(msg);
    } else if (type == "answer") {
        handleAnswer(msg);
    } else if (type == "ice_candidate") {
        handleIceCandidate(msg);
    } else if (type == "error") {
        emit error(QString::fromStdString(msg["message"]));
    }
}

void WebRTCManager::handleOffer(const json& msg) {
    // 收到 offer，说明对方是发起方，我方是接收方
    QString peerId = QString::fromStdString(msg.value("peer_id", "unknown"));

    // 如果还没有这个 peer 的连接，先创建（接收方角色）
    if (!m_peers.contains(peerId)) {
        createPeerConnection(peerId, false);
    }

    auto it = m_peers.find(peerId);
    if (it == m_peers.end()) return;

    std::string sdp_str = msg["sdp"];
    GstSDPMessage* sdp = nullptr;
    gst_sdp_message_new_from_text(sdp_str.c_str(), &sdp);

    GstWebRTCSessionDescription* offer = gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_OFFER, sdp);
    g_signal_emit_by_name(it->webrtcbin, "set-remote-description", offer, nullptr);
    gst_webrtc_session_description_free(offer);

    // 创建 answer
    GstPromise* promise = gst_promise_new_with_change_func(on_answer_created, it->webrtcbin, nullptr);
    g_signal_emit_by_name(it->webrtcbin, "create-answer", nullptr, promise);
    gst_sdp_message_free(sdp);
}

void WebRTCManager::handleAnswer(const json& msg) {
    QString peerId = QString::fromStdString(msg.value("peer_id", "unknown"));
    auto it = m_peers.find(peerId);
    if (it == m_peers.end()) return;

    std::string sdp_str = msg["sdp"];
    GstSDPMessage* sdp = nullptr;
    gst_sdp_message_new_from_text(sdp_str.c_str(), &sdp);

    GstWebRTCSessionDescription* answer = gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_ANSWER, sdp);
    g_signal_emit_by_name(it->webrtcbin, "set-remote-description", answer, nullptr);
    gst_webrtc_session_description_free(answer);
    gst_sdp_message_free(sdp);
}

void WebRTCManager::handleIceCandidate(const json& msg) {
    QString peerId = QString::fromStdString(msg.value("peer_id", "unknown"));
    auto it = m_peers.find(peerId);
    if (it == m_peers.end()) return;

    guint mlineindex = msg["sdpMLineIndex"];
    std::string candidate = msg["candidate"];
    g_signal_emit_by_name(it->webrtcbin, "add-ice-candidate", mlineindex, candidate.c_str());
}
