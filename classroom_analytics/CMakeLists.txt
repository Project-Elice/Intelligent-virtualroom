# Copyright (C) 2019 Intel Corporation
#
# Cmakelist for intelligent classroom analytics

ie_add_sample(NAME classroom-analytics
	      INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/src/"
	      INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include/"
              SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
              SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/cnn.cpp"
              SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/action_detector.cpp"
              SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/detector.cpp"
              SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/tracker.cpp"
              SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/reid_gallery.cpp"
              SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/logger.cpp"
              SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/image_grabber.cpp"
              SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/align_transform.cpp"
              HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/action_detector.hpp"
              HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/detector.hpp"
              HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/face_reid.hpp"
              HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/tracker.hpp"
              HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/image_grabber.hpp"
              HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/logger.hpp"
	      OPENCV_DEPENDENCIES highgui opencv_dnn)
