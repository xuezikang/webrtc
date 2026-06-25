#include "MainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_manager(new WebRTCManager(this))
{
    setupUi();

    // Connect signals
    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(m_createRoomButton, &QPushButton::clicked, this, &MainWindow::onCreateRoomClicked);
    connect(m_joinRoomButton, &QPushButton::clicked, this, &MainWindow::onJoinRoomClicked);
    connect(m_leaveRoomButton, &QPushButton::clicked, this, &MainWindow::onLeaveRoomClicked);

    connect(m_manager, &WebRTCManager::connected, this, &MainWindow::onConnected);
    connect(m_manager, &WebRTCManager::disconnected, this, &MainWindow::onDisconnected);
    connect(m_manager, &WebRTCManager::roomCreated, this, &MainWindow::onRoomCreated);
    connect(m_manager, &WebRTCManager::roomJoined, this, &MainWindow::onRoomJoined);
    connect(m_manager, &WebRTCManager::peerJoined, this, &MainWindow::onPeerJoined);
    connect(m_manager, &WebRTCManager::peerLeft, this, &MainWindow::onPeerLeft);
    connect(m_manager, &WebRTCManager::error, this, &MainWindow::onError);

    // Initial state
    m_createRoomButton->setEnabled(false);
    m_joinRoomButton->setEnabled(false);
    m_leaveRoomButton->setEnabled(false);
    m_roomIdInput->setEnabled(false);

    setWindowTitle("WebRTC Video Conference");
    resize(800, 600);
}

MainWindow::~MainWindow() {
}

void MainWindow::setupUi() {
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

    // Connection layout
    QHBoxLayout* connectionLayout = new QHBoxLayout();
    QLabel* serverLabel = new QLabel("Server:", this);
    m_serverUrlInput = new QLineEdit(this);
    m_serverUrlInput->setPlaceholderText("ws://localhost:8080");
    m_connectButton = new QPushButton("Connect", this);

    connectionLayout->addWidget(serverLabel);
    connectionLayout->addWidget(m_serverUrlInput);
    connectionLayout->addWidget(m_connectButton);
    mainLayout->addLayout(connectionLayout);

    // Room layout
    QHBoxLayout* roomLayout = new QHBoxLayout();
    QLabel* roomIdLabel = new QLabel("Room ID:", this);
    m_roomIdInput = new QLineEdit(this);
    m_roomIdInput->setPlaceholderText("Enter room ID");
    m_createRoomButton = new QPushButton("Create Room", this);
    m_joinRoomButton = new QPushButton("Join Room", this);
    m_leaveRoomButton = new QPushButton("Leave Room", this);

    roomLayout->addWidget(roomIdLabel);
    roomLayout->addWidget(m_roomIdInput);
    roomLayout->addWidget(m_createRoomButton);
    roomLayout->addWidget(m_joinRoomButton);
    roomLayout->addWidget(m_leaveRoomButton);
    mainLayout->addLayout(roomLayout);

    // Status labels
    m_roomLabel = new QLabel("Not in room", this);
    m_roomLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_roomLabel);

    m_statusLabel = new QLabel("Disconnected", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_statusLabel);

    mainLayout->addStretch();
}

void MainWindow::onConnectClicked() {
    QString serverUrl = m_serverUrlInput->text();
    if (serverUrl.isEmpty()) {
        serverUrl = "ws://localhost:8080";
    }
    m_manager->connectToServer(serverUrl);
    m_connectButton->setEnabled(false);
    m_statusLabel->setText("Connecting...");
}

void MainWindow::onCreateRoomClicked() {
    m_manager->createRoom();
}

void MainWindow::onJoinRoomClicked() {
    QString roomId = m_roomIdInput->text();
    if (roomId.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please enter a room ID");
        return;
    }
    m_manager->joinRoom(roomId);
}

void MainWindow::onLeaveRoomClicked() {
    m_manager->leaveRoom();
    m_createRoomButton->setEnabled(true);
    m_joinRoomButton->setEnabled(true);
    m_leaveRoomButton->setEnabled(false);
    m_roomIdInput->setEnabled(true);
    m_roomLabel->setText("Not in room");
    m_statusLabel->setText("Connected");
}

void MainWindow::onConnected() {
    m_connectButton->setEnabled(false);
    m_createRoomButton->setEnabled(true);
    m_joinRoomButton->setEnabled(true);
    m_roomIdInput->setEnabled(true);
    m_statusLabel->setText("Connected");
}

void MainWindow::onDisconnected() {
    m_connectButton->setEnabled(true);
    m_createRoomButton->setEnabled(false);
    m_joinRoomButton->setEnabled(false);
    m_leaveRoomButton->setEnabled(false);
    m_roomIdInput->setEnabled(false);
    m_statusLabel->setText("Disconnected");
    m_roomLabel->setText("Not in room");
}

void MainWindow::onRoomCreated(const QString& roomId) {
    m_roomLabel->setText("Room: " + roomId);
    m_createRoomButton->setEnabled(false);
    m_joinRoomButton->setEnabled(false);
    m_leaveRoomButton->setEnabled(true);
    m_roomIdInput->setEnabled(false);
    m_statusLabel->setText("Waiting for peer...");
}

void MainWindow::onRoomJoined(const QString& roomId) {
    m_roomLabel->setText("Room: " + roomId);
    m_createRoomButton->setEnabled(false);
    m_joinRoomButton->setEnabled(false);
    m_leaveRoomButton->setEnabled(true);
    m_roomIdInput->setEnabled(false);
    m_statusLabel->setText("Connected to room");
}

void MainWindow::onPeerJoined() {
    m_statusLabel->setText("Peer joined, establishing connection...");
}

void MainWindow::onPeerLeft() {
    m_statusLabel->setText("Peer left");
}

void MainWindow::onError(const QString& message) {
    QMessageBox::warning(this, "Error", message);
}
