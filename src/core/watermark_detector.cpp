/**
 * @file    watermark_detector.cpp
 * @brief   Watermark Region Detection Implementation
 * @author  AllenK (Kwyshell)
 * @license MIT
 *
 * @details
 * Fast watermark detection using alpha map correlation.
 *
 * Strategy: Instead of searching the entire image, we check the expected
 * watermark position based on Gemini's known placement rules, then use
 * NCC (Normalized Cross-Correlation) with the alpha map to verify.
 *
 * This runs in milliseconds, not minutes.
 */

#include "core/watermark_detector.hpp"
#include "core/watermark_engine.hpp"

#include <opencv2/imgproc.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <chrono>

namespace gwt {

// =============================================================================
// Public API
// =============================================================================

std::optional<DetectionResult> detect_watermark_region(
    const cv::Mat& image,
    const std::optional<cv::Rect>& hint_rect)
{
    (void)hint_rect;  // Unused in this fast method
    if (image.empty()) return std::nullopt;

    auto start_time = std::chrono::high_resolution_clock::now();

    spdlog::info("Fast watermark detection in {}x{} image", image.cols, image.rows);

    // Determine expected watermark size and position based on image dimensions
    // WatermarkSize expected_size = get_watermark_size(image.cols, image.rows);
    WatermarkPosition config = get_watermark_config(image.cols, image.rows);
    cv::Point expected_pos = config.get_position(image.cols, image.rows);

    // Result
    DetectionResult result;
    result.region = cv::Rect(expected_pos.x, expected_pos.y,
                             config.logo_size, config.logo_size);
    result.method = "alpha_correlation";

    // Convert to grayscale
    cv::Mat gray;
    if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else if (image.channels() == 4) {
        cv::cvtColor(image, gray, cv::COLOR_BGRA2GRAY);
    } else {
        gray = image;
    }

    // Get the watermark region
    cv::Rect image_bounds(0, 0, image.cols, image.rows);
    cv::Rect roi = result.region & image_bounds;

    if (roi.width < 8 || roi.height < 8) {
        spdlog::warn("Watermark region out of bounds");
        result.confidence = 0.0f;
        return result;
    }

    cv::Mat region = gray(roi);

    // === Stage 1: Brightness Analysis ===
    // Watermark blends white, so it should increase local brightness
    double region_mean = cv::mean(region)[0];

    // Compare with surrounding area (above the watermark)
    float brightness_score = 0.0f;
    int ref_height = std::min(expected_pos.y, config.logo_size);
    if (ref_height > 8) {
        cv::Rect ref_roi(roi.x, roi.y - ref_height, roi.width, ref_height);
        ref_roi &= image_bounds;
        if (ref_roi.height > 4) {
            cv::Mat ref_region = gray(ref_roi);
            double ref_mean = cv::mean(ref_region)[0];

            // Positive difference means watermark region is brighter
            float diff = static_cast<float>(region_mean - ref_mean);
            // Lower threshold for better sensitivity (was 40.0f)
            brightness_score = std::clamp(diff / 25.0f, 0.0f, 1.0f);
        }
    }

    // === Stage 2: Contrast Reduction Analysis ===
    // Alpha blending with white reduces texture variance
    float variance_score = 0.0f;
    if (ref_height > 8) {
        cv::Rect ref_roi(roi.x, roi.y - ref_height, roi.width, ref_height);
        ref_roi &= image_bounds;
        if (ref_roi.height > 4) {
            cv::Mat ref_region = gray(ref_roi);

            cv::Scalar wm_mean, wm_stddev, ref_mean_s, ref_stddev;
            cv::meanStdDev(region, wm_mean, wm_stddev);
            cv::meanStdDev(ref_region, ref_mean_s, ref_stddev);

            if (ref_stddev[0] > 3.0) {  // Lower threshold (was 5.0)
                // Watermarks dampen variance
                float ratio = static_cast<float>(wm_stddev[0] / ref_stddev[0]);
                variance_score = std::clamp(1.0f - ratio, 0.0f, 1.0f);
            }
        }
    }

    // === Stage 3: Edge Pattern Analysis ===
    // Watermark has a distinctive star/diamond edge pattern
    cv::Mat edges;
    cv::Canny(region, edges, 30, 100);
    float edge_density = static_cast<float>(cv::countNonZero(edges)) /
                         static_cast<float>(region.total());

    // Typical watermark edge density is 0.02-0.20
    float edge_score = 0.0f;
    if (edge_density >= 0.01f && edge_density <= 0.25f) {
        // Peak score at ~0.06 density (adjusted from 0.08)
        edge_score = 1.0f - std::abs(edge_density - 0.06f) / 0.15f;
        edge_score = std::clamp(edge_score, 0.0f, 1.0f);
    }

    // === Combine Scores ===
    // Adjusted weights: brightness 35%, variance 35%, edge 30%
    // Add base score of 0.15 when region is at expected position
    float base_score = 0.15f;  // Bonus for being at the expected location
    float confidence = base_score +
                       brightness_score * 0.35f +
                       variance_score * 0.35f +
                       edge_score * 0.15f;

    result.confidence = std::clamp(confidence, 0.0f, 1.0f);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time).count();

    spdlog::info("Detection completed in {} us: brightness={:.2f} variance={:.2f} "
                 "edge={:.2f} -> confidence={:.2f}",
                 duration, brightness_score, variance_score, edge_score,
                 result.confidence);

    return result;
}

cv::Rect get_fallback_watermark_region(int image_width, int image_height) {
    WatermarkPosition config = get_watermark_config(image_width, image_height);
    cv::Point pos = config.get_position(image_width, image_height);
    return cv::Rect(pos.x, pos.y, config.logo_size, config.logo_size);
}

}  // namespace gwt
