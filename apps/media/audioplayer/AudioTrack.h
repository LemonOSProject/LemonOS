#pragma once

#include <string>

struct TrackInfo {
    std::string filepath;
    // For displaying the file, only display the name not the full path
    std::string filename;

    // Track duration in seconds
    float duration;
    std::string durationString;

    struct {
        std::string title;
        std::string artist;
        std::string album;
        std::string year;
    } metadata;
};