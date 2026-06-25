cmake_minimum_required(VERSION 3.16)
project(WebRTCVideoConference LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Network WebSockets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-sdp-1.0 gstreamer-webrtc-1.0 gstreamer-video-1.0)

find_package(Boost REQUIRED COMPONENTS system)
find_package(nlohmann_json REQUIRED)

add_subdirectory(server)
add_subdirectory(client)
