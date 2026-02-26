#include "camera.h"
#include "vos/log.h"
#include <random>
#include <sstream>

namespace vos {

static const char* TAG = "Camera";

CameraApp::CameraApp() = default;

Result<void> CameraApp::init() {
    log::info(TAG, "Camera app initialized");
    return Result<void>::success();
}

Result<void> CameraApp::open() {
    if (m_open) return Result<void>::error(StatusCode::ERR_ALREADY_EXISTS);

    m_open = true;
    log::info(TAG, "Camera opened (simulated viewfinder)");
    return Result<void>::success();
}

void CameraApp::close() {
    m_open = false;
    log::info(TAG, "Camera closed");
}

Result<CapturedImage> CameraApp::capture() {
    if (!m_open) return Result<CapturedImage>::error(StatusCode::ERR_NOT_INITIALIZED);

    m_capture_id++;

    // Generate a simulated image — a small noise pattern
    CapturedImage img;
    std::ostringstream oss;
    oss << "IMG_" << m_capture_id << ".vos";
    img.filename    = oss.str();
    img.width       = 64;
    img.height      = 64;
    img.captured_at = Clock::now();

    // Fill with random "pixel" data to simulate a capture
    img.data.resize(img.width * img.height * 3); // RGB
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint16_t> dist(0, 255);
    for (size_t i = 0; i < img.data.size(); i++) {
        img.data[i] = static_cast<uint8_t>(dist(gen));
    }

    m_gallery.push_back(img);

    log::info(TAG, "Captured %s (%dx%d, %zu bytes)",
              img.filename.c_str(), img.width, img.height, img.data.size());
    return Result<CapturedImage>::success(img);
}

} // namespace vos
