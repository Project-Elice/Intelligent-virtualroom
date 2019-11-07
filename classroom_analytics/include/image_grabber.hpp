// Copyright (C) 2018 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

class ImageGrabber {
public:
    explicit ImageGrabber(const std::string& fname);
    bool GrabNext();
    bool Retrieve(cv::Mat& img);
    bool IsOpened() const;
    bool read() const;
    int GetFrameIndex() const;
    int GetFPS() const;
    std::string GetVideoPath() const;
    bool LoopVideo();

private:
    bool is_sequence;
    bool is_opened;
    bool is_read;
    cv::VideoCapture cap;

    std::vector<std::string> videos;
    std::vector<std::vector<int>> frames;
    int current_video_idx;
    int cap_frame_index;
    bool NextVideo();
};
