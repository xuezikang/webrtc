#pragma once

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include "WebRTCManager.h"

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
    void onPeerJoined();
    void onPeerLeft();
    void onError(const QString& message);

private:
    void setupUi();

    QLineEdit* m_serverUrlInput;
    QLineEdit* m_roomIdInput;
    QPushButton* m_connectButton;
    QPushButton* m_createRoomButton;
    QPushButton* m_joinRoomButton;
    QPushButton* m_leaveRoomButton;
    QLabel* m_roomLabel;
    QLabel* m_statusLabel;
    WebRTCManager* m_manager;
};
