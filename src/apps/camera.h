#pragma once

#include "vos/types.h"
#include <string>
#include <vector>

namespace vos {

struct CapturedImage {
    std::string  filename;
    ByteBuffer   data;        // Raw pixel data (simulated)
    int          width;
    int          height;
    TimePoint    captured_at;
};

class CameraApp {
public:
    CameraApp();
    ~CameraApp() = default;

    Result<void> init();

    // Open/close camera
    Result<void> open();
    void         close();
    bool         is_open() const { return m_open; }

    // Capture a photo (simulated on desktop)
    Result<CapturedImage> capture();

    // Get gallery
    const std::vector<CapturedImage>& get_gallery() const { return m_gallery; }
    void clear_gallery() { m_gallery.clear(); }

    // Get capture count
    size_t capture_count() const { return m_gallery.size(); }

private:
    bool m_open = false;
    int  m_capture_id = 0;
    std::vector<CapturedImage> m_gallery;
};

} // namespace vos
