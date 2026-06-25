add_executable(signaling_server
    main.cpp
    WebSocketServer.cpp
)

target_include_directories(signaling_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(signaling_server PRIVATE
    Boost::system
    nlohmann_json::nlohmann_json
)

target_compile_options(signaling_server PRIVATE -pthread)
target_link_options(signaling_server PRIVATE -pthread)

target_compile_definitions(signaling_server PRIVATE
    _WEBSOCKETPP_CPP11_STL_
    _WEBSOCKETPP_CPP11_INTERNAL_
)
