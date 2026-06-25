#pragma once

#include <QObject>
#include <QWebSocket>
#include <gst/gst.h>
#include <gst/webrtc/webrtc.h>
#include <gst/sdp/sdp.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

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
    void peerJoined();
    void peerLeft();
    void error(const QString& message);
    void localVideoReady(GstElement* sink);
    void remoteVideoReady(GstElement* sink);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString& message);

private:
    void setupPipeline();
    void sendOffer();
    void sendAnswer(const GstSDPMessage* sdp);
    void sendIceCandidate(guint mlineindex, const gchar* candidate);
    void handleMessage(const json& msg);
    void handleOffer(const json& msg);
    void handleAnswer(const json& msg);
    void handleIceCandidate(const json& msg);

    static void on_negotiation_needed(GstElement* webrtcbin, gpointer user_data);
    static void on_ice_candidate(GstElement* webrtcbin, guint mlineindex, gchar* candidate, gpointer user_data);
    static void on_offer_created(GstPromise* promise, gpointer user_data);
    static void on_answer_created(GstPromise* promise, gpointer user_data);
    static void on_media_stream(GstElement* webrtcbin, GstPad* pad, GstElement* media);

    QWebSocket* m_webSocket;
    GstElement* m_pipeline;
    GstElement* m_webrtcbin;
    QString m_roomId;
    bool m_isInitiator;
};
