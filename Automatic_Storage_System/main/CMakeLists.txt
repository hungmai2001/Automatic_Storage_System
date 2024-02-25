set(sources "src/camera_server.c"
            "src/wifi_connection.c"
            "src/face_detector.cpp")

idf_component_register(SRCS ${sources}
                       INCLUDE_DIRS "include")
