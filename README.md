# WebRTC Multi-Party Video Conference

基于 **GStreamer webrtcbin** 的局域网多人视频会议系统，采用 C++17 开发，Qt5 提供图形化客户端界面，WebSocket 实现信令控制。

## 架构概览

```
┌─────────────────────────────────────────────────────────────┐
│                      信令服务器 (C++)                         │
│              websocketpp + Boost.Asio + JSON                 │
│         房间管理 / Offer-Answer转发 / ICE中继                  │
└──────────────────────┬──────────────────────────────────────┘
                       │ WebSocket (JSON)
          ┌────────────┴────────────┐
          ▼                         ▼
┌──────────────────┐      ┌──────────────────┐
│   客户端 A (Qt5)  │      │   客户端 B (Qt5)  │
│                  │      │                  │
│ ┌──────────────┐ │      │ ┌──────────────┐ │
│ │  v4l2src     │ │      │ │  v4l2src     │ │
│ │  ↓           │ │      │ │  ↓           │ │
│ │  x264enc     │ │      │ │  x264enc     │ │
│ │  ↓           │ │      │ │  ↓           │ │
│ │  rtph264pay  │ │      │ │  rtph264pay  │ │
│ │  ↓           │ │      │ │  ↓           │ │
│ │  webrtcbin ──┼─┼──────┼─┼──► webrtcbin │ │
│ │  DTLS/SRTP   │ │      │ │  DTLS/SRTP   │ │
│ └──────────────┘ │      │ └──────────────┘ │
└──────────────────┘      └──────────────────┘
```

## 技术栈

| 层级 | 技术选型 | 说明 |
|------|---------|------|
| 信令服务器 | websocketpp + Boost.Asio | 异步 WebSocket，支持多房间并发 |
| 信令协议 | JSON over WebSocket | Offer/Answer/ICE Candidate 交换 |
| 客户端 UI | Qt5 Widgets + Qt5 WebSockets | 跨平台图形界面 |
| 视频采集 | GStreamer v4l2src | Linux V4L2 摄像头接口 |
| 视频编码 | GStreamer x264enc | H.264 实时编码，zerolatency 模式 |
| 传输层 | GStreamer webrtcbin | DTLS/SRTP 加密，ICE 连接建立 |
| 视频解码 | GStreamer avdec_h264 | 远端流解码与渲染 |
| 构建系统 | CMake 3.16+ | 自动 MOC、pkg-config 集成 |

## 依赖安装

### Ubuntu 22.04 / 24.04

```bash
sudo apt-get update && sudo apt-get install -y \
    cmake g++ \
    qtbase5-dev libqt5websockets5-dev \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    libgstreamer-plugins-bad1.0-dev \
    libwebsocketpp-dev libboost-system-dev libboost-dev \
    nlohmann-json3-dev
```

## 构建

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

构建产物：

| 文件 | 说明 |
|------|------|
| `build/server/signaling_server` | 信令服务器可执行文件 |
| `build/client/webrtc_client` | Qt5 客户端可执行文件 |

## 使用方法

### 1. 启动信令服务器

```bash
./build/server/signaling_server 8080
```

输出 `Signaling server running on port 8080` 表示启动成功。

### 2. 启动客户端

```bash
./build/client/webrtc_client
```

客户端界面操作：

1. **Server** 栏输入服务器地址（默认 `ws://localhost:8080`），点击 **Connect**
2. **创建房间**：点击 **Create Room**，界面显示房间 ID（如 `Room: A3X7K2`）
3. **加入房间**：在另一台机器启动客户端，输入房间 ID，点击 **Join Room**
4. 连接建立后，双方摄像头画面实时显示

### 3. 多机测试

在局域网内不同机器上启动客户端，Server 地址填写信令服务器所在机器的 IP：

```
ws://192.168.1.100:8080
```

## 信令协议

客户端与服务器之间通过 JSON 消息交互：

```json
// 创建房间
{"type": "create_room"}

// 房间创建成功
{"type": "room_created", "room_id": "A3X7K2"}

// 加入房间
{"type": "join_room", "room_id": "A3X7K2"}

// SDP Offer 转发
{"type": "offer", "sdp": "v=0\r\n...", "room_id": "A3X7K2"}

// ICE Candidate 转发
{"type": "ice_candidate", "candidate": "...", "sdpMLineIndex": 0, "room_id": "A3X7K2"}
```

## WebRTC 连接流程

```
发起方                    信令服务器                  接收方
  │                          │                          │
  │──── create_room ────────►│                          │
  │◄─── room_created ────────│                          │
  │                          │◄──── join_room ──────────│
  │◄─── peer_joined ─────────│───── room_joined ───────►│
  │                          │                          │
  │  [GStreamer 协商触发]      │                          │
  │──── offer (SDP) ────────►│───── offer (SDP) ───────►│
  │                          │◄──── answer (SDP) ───────│
  │◄─── answer (SDP) ────────│                          │
  │                          │                          │
  │◄═════════ ICE Candidate 交换 ══════════════════════►│
  │                          │                          │
  │◄══════════ DTLS/SRTP 加密视频流 ═══════════════════►│
```

## 项目结构

```
├── CMakeLists.txt                  # 顶层构建配置
├── README.md
├── server/
│   ├── CMakeLists.txt
│   ├── main.cpp                    # 服务器入口
│   ├── WebSocketServer.h           # 会话管理与消息路由
│   └── WebSocketServer.cpp
└── client/
    ├── CMakeLists.txt
    ├── main.cpp                    # GStreamer/Qt 初始化
    ├── MainWindow.h                # 主窗口 UI 控制
    ├── MainWindow.cpp
    ├── WebRTCManager.h             # GStreamer Pipeline 管理
    └── WebRTCManager.cpp
```

## License

MIT
