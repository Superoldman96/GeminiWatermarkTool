/**
 * @file    watermark_detector.hpp
 * @brief   Watermark Region Detection using OpenCV
 * @author  AllenK (Kwyshell)
 * @license MIT
 *
 * @details
 * Detects semi-transparent watermark regions in images using
 * brightness analysis, local contrast comparison, and edge detection.
 *
 * The Gemini watermark is a white semi-transparent overlay that:
 * 1. Increases local brightness compared to surrounding pixels
 * 2. Reduces local contrast (alpha blending with white flattens detail)
 * 3. Has a distinctive diamond/star shape pattern
 * 4. Is typically positioned near the bottom-right corner
 */

#pragma once

#include <opencv2/core.hpp>
#include <optional>
#include <vector>

namespace gwt {

/**
 * Detection result for a candidate watermark region
 */
struct DetectionResult {
    cv::Rect region;            // Detected watermark bounding box
    float confidence;           // Detection confidence [0.0, 1.0]
    std::string method;         // Detection method used
};

/**
 * Detect potential watermark regions in an image
 *
 * Strategy:
 * 1. Focus on bottom-right quadrant (Gemini watermark location bias)
 * 2. Analyze local brightness anomalies (watermark brightens region)
 * 3. Detect contrast reduction (alpha blending with white reduces contrast)
 * 4. Look for semi-transparent overlay patterns
 *
 * @param image      Input image (BGR, 8-bit)
 * @param hint_rect  Optional hint region to search within
 * @return           Detection result, or std::nullopt if nothing found
 */
std::optional<DetectionResult> detect_watermark_region(
    const cv::Mat& image,
    const std::optional<cv::Rect>& hint_rect = std::nullopt
);

/**
 * Get fallback watermark region (96x96 rule)
 * Used when detection fails
 *
 * @param image_width   Image width
 * @param image_height  Image height
 * @return              Default watermark region
 */
cv::Rect get_fallback_watermark_region(int image_width, int image_height);

}  // namespace gwt
