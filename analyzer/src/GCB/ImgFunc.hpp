#pragma once

#include <opencv2/opencv.hpp>

// resize, trim
namespace ImgSize
{
	/// @brief get roi of image
	/// @param img Original image
	/// @param cropped_range Roi's range
	/// @return "roi" of img
	cv::Mat get_img_roi(const cv::Mat &img, const cv::Rect &cropped_range);

	/// @brief get resized image
	/// @param img Original image
	/// @param new_img_size New image size. if only using ratio, "resize_ratio" = cv::Size2d().
	/// @param resize_ratio Ratio used by resize. if only using ratio, "new_img_size" = cv::Size().
	/// @return Resized_image
	cv::Mat get_resized_img(const cv::Mat &img, const cv::Size &new_img_size, const cv::Size2d &resize_ratio);
};

// filter, extract_edge, detect_contour
namespace ImgProc
{
	/// @brief get circularity of contour (0.0 ~ 1.0)
	/// @param contour Contour extracted by cv::findContours
	/// @return "contour"'s circularity
	double get_contour_circularity(const std::vector<cv::Point> &contour);

	/// @brief calculate mean of pixel value
	/// @param img Single channel image having calculated pixels
	/// @param mask Specifying calculated range
	/// @return Mean value
	double calc_pixel_mean_with_mask(const cv::Mat &img, const cv::Mat &mask);

	/// @brief calculate sum of pixel value
	/// @param img Single channel image having calculated pixels
	/// @param mask Specifying calculated range
	/// @return Sum value
	double calc_pixel_sum_with_mask(const cv::Mat &img, const cv::Mat &mask);

	/// @brief calculate clock-wise(eye sight, not image position number) angle (-180 ~ 180) formed by two vectors
	/// @param vec1 Base vector
	/// @param vec2 Another vector
	/// @return Calculated degree of angle (-180 ~ 180)
	float_t calc_angle_degree_formed_by_vectors(
			const cv::Point2f &vec1, const cv::Point2f &vec2,
			const cv::Point2f &base_point = cv::Point2f(0.0f, 0.0f));
};