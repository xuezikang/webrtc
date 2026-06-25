#pragma once

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QScrollArea>
#include <QMap>
#include "WebRTCManager.h"

class VideoWidget : public QFrame {
    Q_OBJECT
public:
    explicit VideoWidget(const QString& peerId, QWidget* parent = nullptr);
    void setSink(GstElement* sink);
    QString peerId() const { return m_peerId; }

private:
    QString m_peerId;
    QLabel* m_label;
    QWidget* m_videoArea;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onConnectClicked();
    void onCreateRoomClicked();
    void onJoinRoomClicked();
    void onLeaveRoomClicked();

    void onConnected();
    void onDisconnected();
    void onRoomCreated(const QString& roomId);
    void onRoomJoined(const QString& roomId);
    void onPeerJoined(const QString& peerId);
    void onPeerLeft(const QString& peerId);
    void onLocalVideoReady(void* sink);
    void onRemoteVideoReady(const QString& peerId, void* sink);
    void onError(const QString& message);

private:
    void setupUi();
    void applyStyle();
    void addVideoWidget(const QString& peerId, bool isLocal);
    void removeVideoWidget(const QString& peerId);
    void updateGridLayout();

    // UI components
    QWidget* m_centralWidget;
    QVBoxLayout* m_mainLayout;

    // Top bar
    QFrame* m_topBar;
    QLabel* m_titleLabel;
    QLabel* m_statusDot;
    QLabel* m_statusLabel;

    // Connection panel
    QFrame* m_connPanel;
    QLineEdit* m_serverUrlInput;
    QPushButton* m_connectButton;

    // Room panel
    QFrame* m_roomPanel;
    QLineEdit* m_roomIdInput;
    QPushButton* m_createRoomButton;
    QPushButton* m_joinRoomButton;
    QPushButton* m_leaveRoomButton;
    QLabel* m_roomLabel;

    // Video grid
    QFrame* m_videoPanel;
    QGridLayout* m_videoGrid;
    QMap<QString, VideoWidget*> m_videoWidgets;

    WebRTCManager* m_manager;
};
