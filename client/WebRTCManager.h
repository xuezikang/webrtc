#pragma once

#include <QObject>
#include <QWebSocket>
#include <QMap>
#include <gst/gst.h>
#include <gst/webrtc/webrtc.h>
#include <gst/sdp/sdp.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct PeerConnection {
    GstElement* webrtcbin;
    GstElement* pipeline;
    bool isInitiator;
};

class WebRTCManager : public QObject {
    Q_OBJECT

public:
    explicit WebRTCManager(QObject* parent = nullptr);
    ~WebRTCManager();

    void connectToServer(const QString& url);
    void createRoom();
    void joinRoom(const QString& roomId);
    void leaveRoom();

signals:
    void connected();
    void disconnected();
    void roomCreated(const QString& roomId);
    void roomJoined(const QString& roomId);
    void peerJoined(const QString& peerId);
    void peerLeft(const QString& peerId);
    void localVideoReady(void* sink);
    void remoteVideoReady(const QString& peerId, void* sink);
    void error(const QString& message);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString& message);

private:
    // 本地摄像头 pipeline
    void setupLocalPipeline();
    void stopLocalPipeline();

    // 每个 peer 独立的 WebRTC 连接
    void createPeerConnection(const QString& peerId, bool isInitiator);
    void removePeerConnection(const QString& peerId);
    void sendOffer(const QString& peerId);
    void sendAnswer(const QString& peerId, const GstSDPMessage* sdp);
    void sendIceCandidate(const QString& peerId, guint mlineindex, const gchar* candidate);

    // 信令消息处理
    void handleMessage(const json& msg);
    void handleOffer(const json& msg);
    void handleAnswer(const json& msg);
    void handleIceCandidate(const json& msg);

    // GStreamer 回调
    static void on_negotiation_needed(GstElement* webrtcbin, gpointer user_data);
    static void on_ice_candidate(GstElement* webrtcbin, guint mlineindex, gchar* candidate, gpointer user_data);
    static void on_offer_created(GstPromise* promise, gpointer user_data);
    static void on_answer_created(GstPromise* promise, gpointer user_data);
    static void on_incoming_stream(GstElement* webrtcbin, GstPad* pad, gpointer user_data);

    QWebSocket* m_webSocket;
    QString m_roomId;

    // 本地摄像头
    GstElement* m_localPipeline;
    GstElement* m_localSink;

    // 多 peer 连接: peerId → PeerConnection
    QMap<QString, PeerConnection> m_peers;

    // 回调中需要找到对应 peerId
    static WebRTCManager* s_instance;
    QMap<GstElement*, QString> m_webrtcbinToPeerId;
};
