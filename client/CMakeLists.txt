qt5_wrap_cpp(MOC_SOURCES
    MainWindow.h
    WebRTCManager.h
)

add_executable(webrtc_client
    main.cpp
    MainWindow.cpp
    WebRTCManager.cpp
    ${MOC_SOURCES}
)

target_include_directories(webrtc_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${GST_INCLUDE_DIRS}
)

target_link_libraries(webrtc_client PRIVATE
    Qt5::Widgets
    Qt5::Network
    Qt5::WebSockets
    ${GST_LIBRARIES}
)

target_compile_definitions(webrtc_client PRIVATE
    GST_USE_UNSTABLE_API
)

target_compile_options(webrtc_client PRIVATE -pthread)
target_link_options(webrtc_client PRIVATE -pthread)
