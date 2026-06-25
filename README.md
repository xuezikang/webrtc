# WebRTC Video Conference

基于 C++17 + Qt5 + GStreamer webrtcbin 的多人视频会议系统，支持局域网内点对点实时视频通信。

## 技术栈

- **客户端**: C++17 + Qt5 (Widgets/WebSockets) + GStreamer 1.20+
- **信令服务器**: C++17 + websocketpp + Boost.Asio + nlohmann/json
- **视频处理**: GStreamer webrtcbin 插件 (H.264 编码)
- **信令协议**: WebSocket + JSON (Offer/Answer/ICE Candidate)
- **安全传输**: DTLS/SRTP (WebRTC 内置)
- **构建系统**: CMake 3.16+

## 依赖安装

### Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install -y \
    cmake g++ \
    qtbase5-dev \
    libqt5websockets5-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libgstreamer-plugins-bad1.0-dev \
    libwebsocketpp-dev \
    libboost-system-dev \
    libboost-dev \
    nlohmann-json3-dev
```

## 构建

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## 运行

### 1. 启动信令服务器

```bash
./build/server/signaling_server [port]
```

默认监听端口 8080。

### 2. 启动客户端 (另一个终端)

```bash
./build/client/webrtc_client
```

### 3. 使用流程

1. 在客户端输入服务器地址（默认 `ws://localhost:8080`）
2. 点击 **Connect** 连接到信令服务器
3. 一方点击 **Create Room** 创建房间，获得房间 ID
4. 另一方输入房间 ID，点击 **Join Room** 加入
5. 视频连接自动建立，开始通信

## 功能特性

- ✅ WebSocket 信令服务器（房间管理、消息转发）
- ✅ Qt5 图形界面客户端
- ✅ GStreamer webrtcbin 视频采集与编码 (H.264)
- ✅ Offer/Answer/ICE Candidate 信令交换
- ✅ DTLS/SRTP 加密传输
- ✅ 局域网点对点通信
- ✅ 房间创建/加入/离开

## 项目结构

```
webrtc-video-conference/
├── CMakeLists.txt              # 主 CMake 配置
├── server/                     # 信令服务器
│   ├── CMakeLists.txt
│   ├── main.cpp                # 服务器入口
│   ├── WebSocketServer.h       # WebSocket 服务器头文件
│   └── WebSocketServer.cpp     # WebSocket 服务器实现
├── client/                     # Qt5 客户端
│   ├── CMakeLists.txt
│   ├── main.cpp                # 客户端入口
│   ├── MainWindow.h            # 主窗口头文件
│   ├── MainWindow.cpp          # 主窗口实现
│   ├── WebRTCManager.h         # WebRTC 管理器头文件
│   └── WebRTCManager.cpp       # WebRTC 管理器实现
└── README.md
```

## 信令协议

客户端与服务器之间使用 JSON 格式消息：

| 消息类型 | 方向 | 说明 |
|---------|------|------|
| `create_room` | 客户端 → 服务器 | 创建新房间 |
| `room_created` | 服务器 → 客户端 | 房间创建成功，返回 room_id |
| `join_room` | 客户端 → 服务器 | 加入指定房间 |
| `room_joined` | 服务器 → 客户端 | 成功加入房间 |
| `peer_joined` | 服务器 → 客户端 | 新用户加入通知 |
| `peer_left` | 服务器 → 客户端 | 用户离开通知 |
| `offer` | 双向转发 | SDP Offer |
| `answer` | 双向转发 | SDP Answer |
| `ice_candidate` | 双向转发 | ICE Candidate |

## WebRTC 连接流程

```
用户A (发起方)              信令服务器              用户B (接收方)
    |                          |                          |
    |--- create_room --------->|                          |
    |<-- room_created ---------|                          |
    |                          |                          |
    |                          |<--- join_room -----------|
    |                          |---- room_joined -------->|
    |<-- peer_joined ----------|                          |
    |                          |                          |
    |  [GStreamer negotiation]  |                          |
    |--- offer (SDP) --------->|---- offer (SDP) -------->|
    |                          |<--- answer (SDP) --------|
    |<-- answer (SDP) ---------|                          |
    |                          |                          |
    |<========== ICE Candidate 交换 ====================>|
    |                          |                          |
    |<============= DTLS/SRTP 加密视频流 ================>|
```

## 注意事项

- 当前版本仅支持局域网内通信，不使用 STUN/TURN 服务器
- 视频源使用 `videotestsrc` 测试模式，如需使用摄像头请修改 `WebRTCManager.cpp` 中的 pipeline 为 `v4l2src`
- 需要 GStreamer 1.20+ 版本以支持 webrtcbin 插件

## License

MIT
