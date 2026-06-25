#include "MainWindow.h"
#include <QMessageBox>
#include <gst/video/videooverlay.h>

// ============================================================
// VideoWidget - 单路视频卡片
// ============================================================

VideoWidget::VideoWidget(const QString& peerId, QWidget* parent)
    : QFrame(parent), m_peerId(peerId)
{
    setObjectName("videoCard");
    setFixedSize(320, 260);
    setStyleSheet(R"(
        #videoCard {
            background: #1a1a2e;
            border: 1px solid #2a2a4a;
            border-radius: 12px;
        }
    )");

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 视频渲染区域
    m_videoArea = new QWidget(this);
    m_videoArea->setStyleSheet("background: #0d0d1a; border-radius: 12px;");
    m_videoArea->setMinimumSize(320, 220);
    layout->addWidget(m_videoArea);

    // 底部标签
    m_label = new QLabel(peerId.isEmpty() ? "Local Camera" : ("Peer: " + peerId.left(8)), this);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setStyleSheet(R"(
        color: #8888aa;
        font-size: 12px;
        padding: 6px;
        background: transparent;
    )");
    layout->addWidget(m_label);
}

void VideoWidget::setSink(GstElement* sink) {
    // 将 GStreamer sink 嵌入 Qt 窗口
    if (!sink) return;

    // 使用 XOverlay 将视频渲染到 Qt 窗口
    // 注意: 需要窗口先 show 出来才能获取 winId
    m_videoArea->show();
    WId winId = m_videoArea->winId();

    if (GST_IS_VIDEO_OVERLAY(sink)) {
        gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(sink), winId);
    }
}

// ============================================================
// MainWindow - 主窗口
// ============================================================

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_manager(new WebRTCManager(this))
{
    setupUi();
    applyStyle();

    // 信号连接
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
    connect(m_manager, &WebRTCManager::localVideoReady, this, &MainWindow::onLocalVideoReady);
    connect(m_manager, &WebRTCManager::remoteVideoReady, this, &MainWindow::onRemoteVideoReady);
    connect(m_manager, &WebRTCManager::error, this, &MainWindow::onError);

    setWindowTitle("WebRTC Video Conference");
    resize(1100, 720);
}

MainWindow::~MainWindow() {}

// ============================================================
// UI 构建
// ============================================================

void MainWindow::setupUi() {
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);

    m_mainLayout = new QVBoxLayout(m_centralWidget);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // ---- 顶部导航栏 ----
    m_topBar = new QFrame(m_centralWidget);
    m_topBar->setObjectName("topBar");
    m_topBar->setFixedHeight(56);
    QHBoxLayout* topLayout = new QHBoxLayout(m_topBar);
    topLayout->setContentsMargins(24, 0, 24, 0);

    m_titleLabel = new QLabel("⬡  WebRTC Conference", m_topBar);
    m_titleLabel->setObjectName("titleLabel");

    m_statusDot = new QLabel("●", m_topBar);
    m_statusDot->setObjectName("statusDot");
    m_statusDot->setStyleSheet("color: #555; font-size: 10px;");

    m_statusLabel = new QLabel("Disconnected", m_topBar);
    m_statusLabel->setObjectName("statusLabel");

    topLayout->addWidget(m_titleLabel);
    topLayout->addStretch();
    topLayout->addWidget(m_statusDot);
    topLayout->addWidget(m_statusLabel);

    m_mainLayout->addWidget(m_topBar);

    // ---- 中间主体 ----
    QHBoxLayout* bodyLayout = new QHBoxLayout();
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    // 左侧控制面板
    QFrame* sidePanel = new QFrame(m_centralWidget);
    sidePanel->setObjectName("sidePanel");
    sidePanel->setFixedWidth(280);
    QVBoxLayout* sideLayout = new QVBoxLayout(sidePanel);
    sideLayout->setContentsMargins(20, 24, 20, 24);
    sideLayout->setSpacing(16);

    // 连接区域
    QLabel* connTitle = new QLabel("CONNECTION", sidePanel);
    connTitle->setObjectName("sectionTitle");

    m_serverUrlInput = new QLineEdit(sidePanel);
    m_serverUrlInput->setObjectName("inputField");
    m_serverUrlInput->setPlaceholderText("ws://localhost:8080");

    m_connectButton = new QPushButton("Connect", sidePanel);
    m_connectButton->setObjectName("primaryBtn");

    sideLayout->addWidget(connTitle);
    sideLayout->addWidget(m_serverUrlInput);
    sideLayout->addWidget(m_connectButton);

    // 分隔线
    QFrame* sep = new QFrame(sidePanel);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #2a2a4a;");
    sideLayout->addWidget(sep);

    // 房间区域
    QLabel* roomTitle = new QLabel("ROOM", sidePanel);
    roomTitle->setObjectName("sectionTitle");

    m_roomIdInput = new QLineEdit(sidePanel);
    m_roomIdInput->setObjectName("inputField");
    m_roomIdInput->setPlaceholderText("Enter room ID");
    m_roomIdInput->setEnabled(false);

    QHBoxLayout* roomBtnLayout = new QHBoxLayout();
    roomBtnLayout->setSpacing(8);
    m_createRoomButton = new QPushButton("Create", sidePanel);
    m_createRoomButton->setObjectName("accentBtn");
    m_createRoomButton->setEnabled(false);
    m_joinRoomButton = new QPushButton("Join", sidePanel);
    m_joinRoomButton->setObjectName("accentBtn");
    m_joinRoomButton->setEnabled(false);
    roomBtnLayout->addWidget(m_createRoomButton);
    roomBtnLayout->addWidget(m_joinRoomButton);

    m_leaveRoomButton = new QPushButton("Leave Room", sidePanel);
    m_leaveRoomButton->setObjectName("dangerBtn");
    m_leaveRoomButton->setEnabled(false);

    m_roomLabel = new QLabel("Not in room", sidePanel);
    m_roomLabel->setObjectName("roomInfo");

    sideLayout->addWidget(roomTitle);
    sideLayout->addWidget(m_roomIdInput);
    sideLayout->addLayout(roomBtnLayout);
    sideLayout->addWidget(m_leaveRoomButton);
    sideLayout->addWidget(m_roomLabel);
    sideLayout->addStretch();

    // 右侧视频网格
    m_videoPanel = new QFrame(m_centralWidget);
    m_videoPanel->setObjectName("videoPanel");
    QVBoxLayout* videoOuterLayout = new QVBoxLayout(m_videoPanel);
    videoOuterLayout->setContentsMargins(24, 24, 24, 24);

    m_videoGrid = new QGridLayout();
    m_videoGrid->setSpacing(16);
    videoOuterLayout->addLayout(m_videoGrid);

    bodyLayout->addWidget(sidePanel);
    bodyLayout->addWidget(m_videoPanel, 1);

    m_mainLayout->addLayout(bodyLayout, 1);
}

void MainWindow::applyStyle() {
    setStyleSheet(R"(
        QMainWindow {
            background: #0d0d1a;
        }

        #topBar {
            background: #111128;
            border-bottom: 1px solid #1e1e3a;
        }

        #titleLabel {
            color: #e0e0ff;
            font-size: 18px;
            font-weight: bold;
            letter-spacing: 1px;
        }

        #statusLabel {
            color: #6666aa;
            font-size: 13px;
        }

        #sidePanel {
            background: #111128;
            border-right: 1px solid #1e1e3a;
        }

        #sectionTitle {
            color: #5555aa;
            font-size: 11px;
            font-weight: bold;
            letter-spacing: 2px;
        }

        #inputField {
            background: #1a1a2e;
            border: 1px solid #2a2a4a;
            border-radius: 8px;
            padding: 10px 14px;
            color: #c0c0e0;
            font-size: 13px;
            selection-background-color: #4444aa;
        }

        #inputField:focus {
            border-color: #5555cc;
        }

        #inputField:disabled {
            background: #131325;
            color: #444466;
        }

        #primaryBtn {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #4444cc, stop:1 #6666ee);
            color: white;
            border: none;
            border-radius: 8px;
            padding: 10px;
            font-size: 13px;
            font-weight: bold;
        }

        #primaryBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #5555dd, stop:1 #7777ff);
        }

        #primaryBtn:disabled {
            background: #222244;
            color: #444466;
        }

        #accentBtn {
            background: #1e1e3a;
            color: #8888cc;
            border: 1px solid #2a2a5a;
            border-radius: 8px;
            padding: 10px;
            font-size: 13px;
            font-weight: bold;
        }

        #accentBtn:hover {
            background: #2a2a4a;
            color: #aaaaff;
            border-color: #4444aa;
        }

        #accentBtn:disabled {
            background: #151530;
            color: #333355;
            border-color: #1e1e3a;
        }

        #dangerBtn {
            background: #2a1520;
            color: #cc4466;
            border: 1px solid #442233;
            border-radius: 8px;
            padding: 10px;
            font-size: 13px;
        }

        #dangerBtn:hover {
            background: #3a2030;
            color: #ff5577;
        }

        #dangerBtn:disabled {
            background: #1a1018;
            color: #443344;
        }

        #roomInfo {
            color: #5555aa;
            font-size: 13px;
            padding: 8px 0;
        }

        #videoPanel {
            background: #0d0d1a;
        }

        QScrollBar:vertical {
            background: #0d0d1a;
            width: 8px;
        }

        QScrollBar::handle:vertical {
            background: #2a2a4a;
            border-radius: 4px;
        }
    )");
}

// ============================================================
// 槽函数
// ============================================================

void MainWindow::onConnectClicked() {
    QString url = m_serverUrlInput->text().trimmed();
    if (url.isEmpty()) url = "ws://localhost:8080";
    m_manager->connectToServer(url);
    m_connectButton->setEnabled(false);
    m_statusLabel->setText("Connecting...");
    m_statusDot->setStyleSheet("color: #ccaa00; font-size: 10px;");
}

void MainWindow::onCreateRoomClicked() {
    m_manager->createRoom();
}

void MainWindow::onJoinRoomClicked() {
    QString roomId = m_roomIdInput->text().trimmed();
    if (roomId.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please enter a room ID");
        return;
    }
    m_manager->joinRoom(roomId);
}

void MainWindow::onLeaveRoomClicked() {
    m_manager->leaveRoom();
    // 清除所有视频卡片
    for (auto* w : m_videoWidgets) {
        m_videoGrid->removeWidget(w);
        w->deleteLater();
    }
    m_videoWidgets.clear();
    m_createRoomButton->setEnabled(true);
    m_joinRoomButton->setEnabled(true);
    m_leaveRoomButton->setEnabled(false);
    m_roomIdInput->setEnabled(true);
    m_roomLabel->setText("Not in room");
    m_statusLabel->setText("Connected");
    m_statusDot->setStyleSheet("color: #44cc66; font-size: 10px;");
}

void MainWindow::onConnected() {
    m_connectButton->setEnabled(false);
    m_createRoomButton->setEnabled(true);
    m_joinRoomButton->setEnabled(true);
    m_roomIdInput->setEnabled(true);
    m_statusLabel->setText("Connected");
    m_statusDot->setStyleSheet("color: #44cc66; font-size: 10px;");
}

void MainWindow::onDisconnected() {
    m_connectButton->setEnabled(true);
    m_createRoomButton->setEnabled(false);
    m_joinRoomButton->setEnabled(false);
    m_leaveRoomButton->setEnabled(false);
    m_roomIdInput->setEnabled(false);
    m_statusLabel->setText("Disconnected");
    m_statusDot->setStyleSheet("color: #555; font-size: 10px;");
    m_roomLabel->setText("Not in room");
}

void MainWindow::onRoomCreated(const QString& roomId) {
    m_roomLabel->setText("Room: " + roomId);
    m_createRoomButton->setEnabled(false);
    m_joinRoomButton->setEnabled(false);
    m_leaveRoomButton->setEnabled(true);
    m_roomIdInput->setEnabled(false);
    m_statusLabel->setText("Waiting for peers...");
}

void MainWindow::onRoomJoined(const QString& roomId) {
    m_roomLabel->setText("Room: " + roomId);
    m_createRoomButton->setEnabled(false);
    m_joinRoomButton->setEnabled(false);
    m_leaveRoomButton->setEnabled(true);
    m_roomIdInput->setEnabled(false);
    m_statusLabel->setText("In room");
}

void MainWindow::onPeerJoined(const QString& peerId) {
    m_statusLabel->setText("Peer joined: " + peerId.left(8));
}

void MainWindow::onPeerLeft(const QString& peerId) {
    removeVideoWidget(peerId);
    m_statusLabel->setText("Peer left: " + peerId.left(8));
}

void MainWindow::onLocalVideoReady(void* sink) {
    addVideoWidget("", true);
    if (sink) {
        VideoWidget* w = m_videoWidgets.value("");
        if (w) w->setSink(static_cast<GstElement*>(sink));
    }
}

void MainWindow::onRemoteVideoReady(const QString& peerId, void* sink) {
    addVideoWidget(peerId, false);
    if (sink) {
        VideoWidget* w = m_videoWidgets.value(peerId);
        if (w) w->setSink(static_cast<GstElement*>(sink));
    }
}

void MainWindow::onError(const QString& message) {
    QMessageBox::warning(this, "Error", message);
}

// ============================================================
// 视频网格管理
// ============================================================

void MainWindow::addVideoWidget(const QString& peerId, bool isLocal) {
    if (m_videoWidgets.contains(peerId)) return;

    QString label = isLocal ? "Local Camera" : ("Peer: " + peerId.left(8));
    VideoWidget* w = new VideoWidget(label, m_videoPanel);
    m_videoWidgets[peerId] = w;
    updateGridLayout();
}

void MainWindow::removeVideoWidget(const QString& peerId) {
    auto it = m_videoWidgets.find(peerId);
    if (it == m_videoWidgets.end()) return;

    m_videoGrid->removeWidget(it.value());
    it.value()->deleteLater();
    m_videoWidgets.erase(it);
    updateGridLayout();
}

void MainWindow::updateGridLayout() {
    // 清除现有布局
    while (m_videoGrid->count() > 0) {
        QLayoutItem* item = m_videoGrid->takeAt(0);
        // 不 delete widget，只是从布局移除
    }

    // 重新排列: 本地视频固定在左上角
    int col = 0, row = 0;
    int maxCols = 2; // 每行最多2个，可扩展

    if (m_videoWidgets.contains("")) {
        m_videoGrid->addWidget(m_videoWidgets[""], row, col);
        col++;
        if (col >= maxCols) { col = 0; row++; }
    }

    for (auto it = m_videoWidgets.begin(); it != m_videoWidgets.end(); ++it) {
        if (it.key().isEmpty()) continue;
        m_videoGrid->addWidget(it.value(), row, col);
        col++;
        if (col >= maxCols) { col = 0; row++; }
    }
}
