#include "../ImgFunc.hpp"

cv::Mat ImgSize::get_img_roi(const cv::Mat &img, const cv::Rect &cropped_range)
{
	return img(cropped_range);
}

cv::Mat ImgSize::get_resized_img(
		const cv::Mat &img,
		const cv::Size &new_img_size,
		const cv::Size2d &resize_ratio)
{
	cv::Mat resized_img;
	cv::resize(img, resized_img, new_img_size, resize_ratio.width, resize_ratio.height);

	return resized_img;
}