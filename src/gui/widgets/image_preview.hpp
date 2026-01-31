/**
 * @file    image_preview.hpp
 * @brief   Image Preview Widget
 * @author  AllenK (Kwyshell)
 * @license MIT
 */

#pragma once

#include "gui/app/app_controller.hpp"
#include <imgui.h>

namespace gwt::gui {

class ImagePreview {
public:
    explicit ImagePreview(AppController& controller);
    ~ImagePreview() = default;

    // Non-copyable
    ImagePreview(const ImagePreview&) = delete;
    ImagePreview& operator=(const ImagePreview&) = delete;

    /**
     * Render the preview
     * Call within ImGui context
     */
    void render();

private:
    AppController& m_controller;

    // Cached transform for screen <-> image coordinate conversion
    float m_final_scale{1.0f};
    ImVec2 m_image_screen_pos{0, 0};

    void render_image();
    void render_placeholder();
    void handle_input(const ImVec2& viewport_size, float content_w, float content_h);

    // Custom watermark rect interaction
    void handle_custom_rect_interaction();
    void draw_custom_rect_with_anchors(ImDrawList* draw_list);

    // Coordinate conversion helpers
    ImVec2 image_to_screen(float ix, float iy) const;
    ImVec2 screen_to_image(float sx, float sy) const;
    AnchorPoint hit_test_anchor(const ImVec2& mouse_pos, const cv::Rect& rect) const;
};

}  // namespace gwt::gui
