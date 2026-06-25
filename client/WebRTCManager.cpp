#include "WebRTCManager.h"
#include <iostream>

WebRTCManager::WebRTCManager(QObject* parent)
    : QObject(parent)
    , m_webSocket(new QWebSocket())
    , m_pipeline(nullptr)
    , m_webrtcbin(nullptr)
    , m_isInitiator(false)
{
    connect(m_webSocket, &QWebSocket::connected, this, &WebRTCManager::onConnected);
    connect(m_webSocket, &QWebSocket::disconnected, this, &WebRTCManager::onDisconnected);
    connect(m_webSocket, &QWebSocket::textMessageReceived, this, &WebRTCManager::onTextMessageReceived);
}

WebRTCManager::~WebRTCManager() {
    leaveRoom();
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
    }
}

void WebRTCManager::connectToServer(const QString& url) {
    m_webSocket->open(QUrl(url));
}

void WebRTCManager::createRoom() {
    json msg = {{"type", "create_room"}};
    m_webSocket->sendTextMessage(QString::fromStdString(msg.dump()));
    m_isInitiator = true;
}

void WebRTCManager::joinRoom(const QString& roomId) {
    m_roomId = roomId;
    json msg = {{"type", "join_room"}, {"room_id", roomId.toStdString()}};
    m_webSocket->sendTextMessage(QString::fromStdString(msg.dump()));
    m_isInitiator = false;
}

void WebRTCManager::leaveRoom() {
    if (!m_roomId.isEmpty()) {
        json msg = {{"type", "leave_room"}, {"room_id", m_roomId.toStdString()}};
        m_webSocket->sendTextMessage(QString::fromStdString(msg.dump()));
        m_roomId.clear();
    }
}

void WebRTCManager::onConnected() {
    emit connected();
    setupPipeline();
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

void WebRTCManager::setupPipeline() {
    GError* error = nullptr;

    // Pipeline: 本机摄像头采集 → H264编码 → WebRTC发送
    // 远端视频流通过 pad-added 回调动态接入
    const char* pipeline_str =
        "v4l2src ! video/x-raw,width=640,height=480,framerate=30/1 ! "
        "videoconvert ! x264enc tune=zerolatency bitrate=800 speed-preset=ultrafast key-int-max=30 ! "
        "rtph264pay config-interval=1 pt=96 ! "
        "application/x-rtp,media=video,encoding-name=H264,payload=96 ! "
        "webrtcbin name=sendrecv stun-server=stun://0.0.0.0:0";

    m_pipeline = gst_parse_launch(pipeline_str, &error);
    if (error) {
        std::cerr << "Pipeline error: " << error->message << std::endl;
        g_error_free(error);
        return;
    }

    m_webrtcbin = gst_bin_get_by_name(GST_BIN(m_pipeline), "sendrecv");
    if (!m_webrtcbin) {
        std::cerr << "Failed to get webrtcbin element" << std::endl;
        return;
    }

    // 连接 WebRTC 信号
    g_signal_connect(m_webrtcbin, "on-negotiation-needed", G_CALLBACK(on_negotiation_needed), this);
    g_signal_connect(m_webrtcbin, "on-ice-candidate", G_CALLBACK(on_ice_candidate), this);
    g_signal_connect(m_webrtcbin, "pad-added", G_CALLBACK(on_media_stream), this);

    // 启动 pipeline
    gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
}

void WebRTCManager::on_negotiation_needed(GstElement* webrtcbin, gpointer user_data) {
    WebRTCManager* self = static_cast<WebRTCManager*>(user_data);
    if (self->m_isInitiator) {
        self->sendOffer();
    }
}

void WebRTCManager::on_ice_candidate(GstElement* webrtcbin, guint mlineindex, gchar* candidate, gpointer user_data) {
    WebRTCManager* self = static_cast<WebRTCManager*>(user_data);
    self->sendIceCandidate(mlineindex, candidate);
}

void WebRTCManager::on_offer_created(GstPromise* promise, gpointer user_data) {
    WebRTCManager* self = static_cast<WebRTCManager*>(user_data);
    const GstStructure* reply = gst_promise_get_reply(promise);
    GstWebRTCSessionDescription* offer = nullptr;
    gst_structure_get(reply, "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offer, nullptr);
    gst_promise_unref(promise);

    if (offer) {
        // Set local description
        GstPromise* local_desc_promise = gst_promise_new();
        g_signal_emit_by_name(self->m_webrtcbin, "set-local-description", offer, local_desc_promise);
        gst_promise_interrupt(local_desc_promise);
        gst_promise_unref(local_desc_promise);

        // Send offer to signaling server
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
    WebRTCManager* self = static_cast<WebRTCManager*>(user_data);
    const GstStructure* reply = gst_promise_get_reply(promise);
    GstWebRTCSessionDescription* answer = nullptr;
    gst_structure_get(reply, "answer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &answer, nullptr);
    gst_promise_unref(promise);

    if (answer) {
        // Set local description
        GstPromise* local_desc_promise = gst_promise_new();
        g_signal_emit_by_name(self->m_webrtcbin, "set-local-description", answer, local_desc_promise);
        gst_promise_interrupt(local_desc_promise);
        gst_promise_unref(local_desc_promise);

        // Send answer to signaling server
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

void WebRTCManager::on_media_stream(GstElement* webrtcbin, GstPad* pad, gpointer user_data) {
    WebRTCManager* self = static_cast<WebRTCManager*>(user_data);

    // 远端视频流到达时，动态创建解码+显示管线
    GstCaps* caps = gst_pad_get_current_caps(pad);
    if (!caps) caps = gst_pad_query_caps(pad, nullptr);

    const GstStructure* s = gst_caps_get_structure(caps, 0);
    const gchar* name = gst_structure_get_name(s);

    if (g_str_has_prefix(name, "application/x-rtp")) {
        GstElement* queue = gst_element_factory_make("queue", nullptr);
        GstElement* depay = gst_element_factory_make("rtph264depay", nullptr);
        GstElement* dec = gst_element_factory_make("avdec_h264", nullptr);
        GstElement* conv = gst_element_factory_make("videoconvert", nullptr);
        GstElement* sink = gst_element_factory_make("autovideosink", nullptr);

        gst_bin_add_many(GST_BIN(self->m_pipeline), queue, depay, dec, conv, sink, nullptr);
        gst_element_sync_state_with_parent(queue);
        gst_element_sync_state_with_parent(depay);
        gst_element_sync_state_with_parent(dec);
        gst_element_sync_state_with_parent(conv);
        gst_element_sync_state_with_parent(sink);

        gst_element_link_many(queue, depay, dec, conv, sink, nullptr);

        GstPad* sink_pad = gst_element_get_static_pad(queue, "sink");
        if (gst_pad_link(pad, sink_pad) != GST_PAD_LINK_OK) {
            std::cerr << "Failed to link remote video pad" << std::endl;
        }
        gst_object_unref(sink_pad);
    }
    gst_caps_unref(caps);
}

void WebRTCManager::sendOffer() {
    GstPromise* promise = gst_promise_new_with_change_func(on_offer_created, this, nullptr);
    g_signal_emit_by_name(m_webrtcbin, "create-offer", nullptr, promise);
}

void WebRTCManager::sendAnswer(const GstSDPMessage* sdp) {
    GstSDPMessage* copy = nullptr;
    gst_sdp_message_copy(sdp, &copy);
    GstPromise* promise = gst_promise_new_with_change_func(on_answer_created, this, nullptr);
    GstWebRTCSessionDescription* answer = gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_ANSWER, copy);
    g_signal_emit_by_name(m_webrtcbin, "set-remote-description", answer, nullptr);
    gst_webrtc_session_description_free(answer);
    g_signal_emit_by_name(m_webrtcbin, "create-answer", nullptr, promise);
}

void WebRTCManager::sendIceCandidate(guint mlineindex, const gchar* candidate) {
    json msg = {
        {"type", "ice_candidate"},
        {"candidate", candidate},
        {"sdpMid", ""},
        {"sdpMLineIndex", mlineindex},
        {"room_id", m_roomId.toStdString()}
    };
    m_webSocket->sendTextMessage(QString::fromStdString(msg.dump()));
}

void WebRTCManager::handleMessage(const json& msg) {
    std::string type = msg["type"];

    if (type == "room_created") {
        m_roomId = QString::fromStdString(msg["room_id"]);
        emit roomCreated(m_roomId);
    } else if (type == "room_joined") {
        emit roomJoined(m_roomId);
    } else if (type == "peer_joined") {
        emit peerJoined();
        if (m_isInitiator) {
            sendOffer();
        }
    } else if (type == "peer_left") {
        emit peerLeft();
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
    std::string sdp_str = msg["sdp"];
    GstSDPMessage* sdp = nullptr;
    gst_sdp_message_new_from_text(sdp_str.c_str(), &sdp);

    GstPromise* promise = gst_promise_new_with_change_func(on_answer_created, this, nullptr);
    GstWebRTCSessionDescription* offer = gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_OFFER, sdp);
    g_signal_emit_by_name(m_webrtcbin, "set-remote-description", offer, nullptr);
    gst_webrtc_session_description_free(offer);
    g_signal_emit_by_name(m_webrtcbin, "create-answer", nullptr, promise);
    gst_sdp_message_free(sdp);
}

void WebRTCManager::handleAnswer(const json& msg) {
    std::string sdp_str = msg["sdp"];
    GstSDPMessage* sdp = nullptr;
    gst_sdp_message_new_from_text(sdp_str.c_str(), &sdp);

    GstWebRTCSessionDescription* answer = gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_ANSWER, sdp);
    g_signal_emit_by_name(m_webrtcbin, "set-remote-description", answer, nullptr);
    gst_webrtc_session_description_free(answer);
    gst_sdp_message_free(sdp);
}

void WebRTCManager::handleIceCandidate(const json& msg) {
    guint mlineindex = msg["sdpMLineIndex"];
    std::string candidate = msg["candidate"];
    g_signal_emit_by_name(m_webrtcbin, "add-ice-candidate", mlineindex, candidate.c_str());
}
