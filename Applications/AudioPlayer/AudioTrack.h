#pragma once

#include <string>

struct TrackInfo {
    std::string filepath;
    struct {
        std::string title;
        std::string artist;
        std::string album;
        std::string year;
    } metadata;
};