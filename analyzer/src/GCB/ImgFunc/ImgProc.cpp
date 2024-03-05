#include "../ImgFunc.hpp"

double ImgProc::get_contour_circularity(const std::vector<cv::Point> &contour)
{
	double contour_circularity = 0.0;

	const auto contour_area = cv::contourArea(contour);
	const auto contour_length = cv::arcLength(contour, true);

	if (contour_length > 0)
		contour_circularity = 4 * M_PI * contour_area / std::pow(contour_length, 2.0);

	return contour_circularity;
}

double ImgProc::calc_pixel_sum_with_mask(const cv::Mat &img, const cv::Mat &mask)
{
	cv::Mat processed_img;
	cv::bitwise_and(img, mask, processed_img);
	return cv::sum(processed_img)[0];
}

double ImgProc::calc_pixel_mean_with_mask(const cv::Mat &img, const cv::Mat &mask)
{
	return cv::mean(img, mask)[0];
}

float_t ImgProc::calc_angle_degree_formed_by_vectors(
		const cv::Point2f &vec1, const cv::Point2f &vec2, const cv::Point2f &base_point)
{
	const auto vec1_transed = vec1 - base_point;
	const auto vec2_transed = vec2 - base_point;

	const auto vec1_degree = cv::fastAtan2(vec1_transed.y, vec1_transed.x);
	const auto vec2_degree = cv::fastAtan2(vec2_transed.y, vec2_transed.x);

	const auto delta_angle = vec2_degree - vec1_degree;
	const auto delta_angle_abs = std::abs(delta_angle);
	if (delta_angle_abs > 180.0f)
		return -(360.0f - delta_angle_abs) * delta_angle_abs / delta_angle;
	else
		return delta_angle;
}