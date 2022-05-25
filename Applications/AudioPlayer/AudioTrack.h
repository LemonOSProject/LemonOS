#pragma once

#include <string>

struct TrackInfo {
    std::string filepath;
    std::string durationString;
    struct {
        std::string title;
        std::string artist;
        std::string album;
        std::string year;
    } metadata;
};