#include "MainWindow.h"
#include <QMessageBox>
#include <QFontDatabase>
#include <gst/video/videooverlay.h>

// ============================================================
// VideoWidget - 单路视频卡片
// ============================================================

VideoWidget::VideoWidget(const QString& peerId, QWidget* parent)
    : QFrame(parent), m_peerId(peerId)
{
    setObjectName("videoCard");

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 视频渲染区域
    m_videoArea = new QWidget(this);
    m_videoArea->setObjectName("videoArea");
    m_videoArea->setMinimumSize(320, 220);
    layout->addWidget(m_videoArea);

    // 底部标签
    m_label = new QLabel(peerId, this);
    m_label->setObjectName("videoLabel");
    m_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_label);
}

void VideoWidget::setSink(GstElement* sink) {
    if (!sink) return;
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

    setWindowTitle("WebRTC Conference");
    resize(1200, 750);
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
    m_topBar->setFixedHeight(52);
    QHBoxLayout* topLayout = new QHBoxLayout(m_topBar);
    topLayout->setContentsMargins(28, 0, 28, 0);

    m_titleLabel = new QLabel("WebRTC Conference", m_topBar);
    m_titleLabel->setObjectName("titleLabel");

    m_statusDot = new QLabel("●", m_topBar);
    m_statusDot->setObjectName("statusDot");

    m_statusLabel = new QLabel("Disconnected", m_topBar);
    m_statusLabel->setObjectName("statusLabel");

    topLayout->addWidget(m_titleLabel);
    topLayout->addStretch();
    topLayout->addWidget(m_statusDot);
    topLayout->addSpacing(6);
    topLayout->addWidget(m_statusLabel);

    m_mainLayout->addWidget(m_topBar);

    // ---- 中间主体 ----
    QHBoxLayout* bodyLayout = new QHBoxLayout();
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    // 左侧控制面板
    QFrame* sidePanel = new QFrame(m_centralWidget);
    sidePanel->setObjectName("sidePanel");
    sidePanel->setFixedWidth(260);
    QVBoxLayout* sideLayout = new QVBoxLayout(sidePanel);
    sideLayout->setContentsMargins(24, 28, 24, 28);
    sideLayout->setSpacing(20);

    // -- 连接区域 --
    QLabel* connTitle = new QLabel("CONNECTION", sidePanel);
    connTitle->setObjectName("sectionTitle");

    m_serverUrlInput = new QLineEdit(sidePanel);
    m_serverUrlInput->setObjectName("inputField");
    m_serverUrlInput->setPlaceholderText("ws://localhost:8080");

    m_connectButton = new QPushButton("Connect", sidePanel);
    m_connectButton->setObjectName("primaryBtn");

    sideLayout->addWidget(connTitle);
    sideLayout->addSpacing(8);
    sideLayout->addWidget(m_serverUrlInput);
    sideLayout->addWidget(m_connectButton);

    // 分隔线
    QFrame* sep = new QFrame(sidePanel);
    sep->setObjectName("separator");
    sep->setFrameShape(QFrame::HLine);
    sideLayout->addWidget(sep);

    // -- 房间区域 --
    QLabel* roomTitle = new QLabel("ROOM", sidePanel);
    roomTitle->setObjectName("sectionTitle");

    m_roomIdInput = new QLineEdit(sidePanel);
    m_roomIdInput->setObjectName("inputField");
    m_roomIdInput->setPlaceholderText("Enter room ID");
    m_roomIdInput->setEnabled(false);

    QHBoxLayout* roomBtnLayout = new QHBoxLayout();
    roomBtnLayout->setSpacing(10);
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
    sideLayout->addSpacing(8);
    sideLayout->addWidget(m_roomIdInput);
    sideLayout->addLayout(roomBtnLayout);
    sideLayout->addWidget(m_leaveRoomButton);
    sideLayout->addSpacing(4);
    sideLayout->addWidget(m_roomLabel);
    sideLayout->addStretch();

    // 右侧视频区域
    m_videoPanel = new QFrame(m_centralWidget);
    m_videoPanel->setObjectName("videoPanel");
    QVBoxLayout* videoOuterLayout = new QVBoxLayout(m_videoPanel);
    videoOuterLayout->setContentsMargins(28, 28, 28, 28);

    m_videoGrid = new QGridLayout();
    m_videoGrid->setSpacing(20);
    videoOuterLayout->addLayout(m_videoGrid);

    bodyLayout->addWidget(sidePanel);
    bodyLayout->addWidget(m_videoPanel, 1);

    m_mainLayout->addLayout(bodyLayout, 1);
}

// ============================================================
// 样式表 - Claude 黑橙风格
// ============================================================

void MainWindow::applyStyle() {
    // 尝试加载 Inter 字体，回退到系统默认
    int fontId = QFontDatabase::addApplicationFont(":/fonts/Inter-Regular.ttf");
    QString fontFamily = "Inter";
    if (fontId == -1) {
        // Inter 不可用，使用系统 sans-serif
        fontFamily = "Cantarell, Helvetica Neue, Arial, sans-serif";
    }

    setStyleSheet(QStringLiteral(R"(
        /* ========== 全局 ========== */
        * {
            font-family: '%1', 'Helvetica Neue', Arial, sans-serif;
        }

        QMainWindow, QWidget#centralWidget {
            background: #0a0a0a;
        }

        /* ========== 顶部栏 ========== */
        #topBar {
            background: #0a0a0a;
            border-bottom: 1px solid #1e1e1e;
        }

        #titleLabel {
            color: #e8e8e8;
            font-size: 15px;
            font-weight: 600;
            letter-spacing: 0.5px;
        }

        #statusDot {
            color: #3a3a3a;
            font-size: 8px;
        }

        #statusLabel {
            color: #6b6b6b;
            font-size: 13px;
            font-weight: 400;
        }

        /* ========== 侧边栏 ========== */
        #sidePanel {
            background: #0f0f0f;
            border-right: 1px solid #1e1e1e;
        }

        #sectionTitle {
            color: #d97757;
            font-size: 10px;
            font-weight: 700;
            letter-spacing: 2.5px;
        }

        #separator {
            background: #1e1e1e;
            max-height: 1px;
        }

        /* ========== 输入框 ========== */
        #inputField {
            background: #1a1a1a;
            border: 1px solid #2a2a2a;
            border-radius: 10px;
            padding: 11px 14px;
            color: #e0e0e0;
            font-size: 13px;
            selection-background-color: #d97757;
        }

        #inputField:focus {
            border-color: #d97757;
            background: #141414;
        }

        #inputField:disabled {
            background: #111111;
            color: #3a3a3a;
            border-color: #1a1a1a;
        }

        #inputField::placeholder {
            color: #4a4a4a;
        }

        /* ========== 主按钮 (橙色实心) ========== */
        #primaryBtn {
            background: #d97757;
            color: #ffffff;
            border: none;
            border-radius: 10px;
            padding: 11px 16px;
            font-size: 13px;
            font-weight: 600;
        }

        #primaryBtn:hover {
            background: #e08a6a;
        }

        #primaryBtn:pressed {
            background: #c46a4a;
        }

        #primaryBtn:disabled {
            background: #2a2a2a;
            color: #4a4a4a;
        }

        /* ========== 次级按钮 (描边) ========== */
        #accentBtn {
            background: transparent;
            color: #d97757;
            border: 1px solid #3a2a22;
            border-radius: 10px;
            padding: 11px 16px;
            font-size: 13px;
            font-weight: 600;
        }

        #accentBtn:hover {
            background: #1a1410;
            border-color: #d97757;
        }

        #accentBtn:pressed {
            background: #241a14;
        }

        #accentBtn:disabled {
            background: transparent;
            color: #2a2a2a;
            border-color: #1e1e1e;
        }

        /* ========== 危险按钮 ========== */
        #dangerBtn {
            background: transparent;
            color: #6b4a4a;
            border: 1px solid #2a1a1a;
            border-radius: 10px;
            padding: 11px 16px;
            font-size: 13px;
            font-weight: 500;
        }

        #dangerBtn:hover {
            background: #1a0f0f;
            color: #c45050;
            border-color: #4a2020;
        }

        #dangerBtn:disabled {
            background: transparent;
            color: #222222;
            border-color: #161616;
        }

        /* ========== 房间信息 ========== */
        #roomInfo {
            color: #d97757;
            font-size: 13px;
            font-weight: 500;
            padding: 4px 0;
        }

        /* ========== 视频区域 ========== */
        #videoPanel {
            background: #0a0a0a;
        }

        #videoCard {
            background: #141414;
            border: 1px solid #1e1e1e;
            border-radius: 14px;
        }

        #videoCard:hover {
            border-color: #2a2a2a;
        }

        #videoArea {
            background: #0d0d0d;
            border-radius: 14px 14px 0 0;
        }

        #videoLabel {
            color: #6b6b6b;
            font-size: 11px;
            font-weight: 500;
            padding: 10px 12px;
            background: transparent;
            border-radius: 0 0 14px 14px;
        }

        /* ========== 滚动条 ========== */
        QScrollBar:vertical {
            background: #0a0a0a;
            width: 6px;
        }

        QScrollBar::handle:vertical {
            background: #2a2a2a;
            border-radius: 3px;
        }

        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0;
        }
    )").arg(fontFamily));

    // 更新状态指示灯颜色
    m_statusDot->setStyleSheet("color: #3a3a3a; font-size: 8px;");
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
    m_statusDot->setStyleSheet("color: #d97757; font-size: 8px;");
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
    m_statusDot->setStyleSheet("color: #d97757; font-size: 8px;");
}

void MainWindow::onConnected() {
    m_connectButton->setEnabled(false);
    m_createRoomButton->setEnabled(true);
    m_joinRoomButton->setEnabled(true);
    m_roomIdInput->setEnabled(true);
    m_statusLabel->setText("Connected");
    m_statusDot->setStyleSheet("color: #d97757; font-size: 8px;");
}

void MainWindow::onDisconnected() {
    m_connectButton->setEnabled(true);
    m_createRoomButton->setEnabled(false);
    m_joinRoomButton->setEnabled(false);
    m_leaveRoomButton->setEnabled(false);
    m_roomIdInput->setEnabled(false);
    m_statusLabel->setText("Disconnected");
    m_statusDot->setStyleSheet("color: #3a3a3a; font-size: 8px;");
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
    m_statusLabel->setText("Peer joined");
}

void MainWindow::onPeerLeft(const QString& peerId) {
    removeVideoWidget(peerId);
    m_statusLabel->setText("Peer left");
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

    QString label = isLocal ? "Local Camera" : ("Remote · " + peerId);
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
    while (m_videoGrid->count() > 0) {
        m_videoGrid->takeAt(0);
    }

    int col = 0, row = 0;
    int maxCols = 2;

    // 本地视频固定左上
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
