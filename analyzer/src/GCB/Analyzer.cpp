#include "../GCB.hpp"

#include <array>
#include <cmath>
#include <fstream>
#include <tuple>
#include <vector>

#include "ImgFunc.hpp"
#include "GCB.hpp"

using namespace GCB;
using namespace Inside;

/// @brief output content of LedData object
/// @param led_label Led_data' ID
/// @param led_data Browsed data
static void dump_led_data(const std::string &led_label, const LedData &led_data)
{
  std::cout << led_label
            << " center: " << led_data.m_position
            << ", radius: " << led_data.m_radius
            << ", color: " + led_data.m_color
            << std::endl;
}

/// @brief output content of DeviceDefinition object
/// @param definition Browsed data
static void dump_device_definition(const DeviceDefinition &definition)
{
  std::cout << "*[" + definition.m_deviceName + "]*" << std::endl;
  for (const auto &[key, led_data] : definition.m_markerHash)
    dump_led_data("marker_" + key, led_data);
  for (const auto &[key, led_data] : definition.m_beaconHash)
    dump_led_data("beacon_" + key, led_data);
  std::cout << std::endl;
}

/// @brief read file content
/// @param file_path Loaded file's path
/// @return File content string
static std::string read_file(const std::string &file_path)
{
  std::ifstream ifs(file_path);
  if (ifs.fail())
  {
    std::cout << "File Open Error" << std::endl;
    return "";
  }

  std::string file_content, line;
  while (std::getline(ifs, line))
    file_content.append(line);

  return file_content;
}

/// @brief get LedData from JSON
/// @param led_json JSON object
/// @param device_template_size Size of device template image
/// @return LedData object
static LedData get_led_data_from_json(
    const nlohmann::json &led_json,
    const cv::Size &device_template_size)
{
  LedData led_data;
  led_data.m_position.x = led_json["center_x"];
  led_data.m_position.y = led_json["center_y"];
  led_data.m_radius = led_json["radius"];
  led_data.m_color = led_json["color"];

  /* decide calculated area */
  const auto led_mask_rect_tl = led_data.m_position - cv::Point2f(led_data.m_radius, led_data.m_radius);
  const auto led_mask_rect_br = led_data.m_position + cv::Point2f(led_data.m_radius, led_data.m_radius);
  led_data.m_boundingRect = cv::Rect(led_mask_rect_tl, led_mask_rect_br);

  cv::Mat led_mask = cv::Mat::zeros(device_template_size, CV_8UC1);
  cv::circle(led_mask, led_data.m_position, static_cast<int32_t>(led_data.m_radius), cv::Scalar(255, 0, 0), -1);
  led_data.m_ledMask = ImgSize::get_img_roi(led_mask, led_data.m_boundingRect).clone();
  /* end: decide calculated area */

  return led_data;
}

/// @brief detect markers using circularity and color and lightness
/// @param contours Detected contours
/// @param value_calculate_callback function (ImgProc::sum or ImgProc::mean)
/// @param lightness_img Lightness channel image
/// @param single_color_img Single channel image
/// @return Pair of ixel value and center position of marker list
static std::vector<std::pair<int32_t, cv::Point2f>> extract_marker_list(
    const std::vector<std::vector<cv::Point>> &contours,
    const std::function<double(const cv::Mat &, const cv::Mat &)> value_calculate_callback,
    const cv::Mat &lightness_img, const cv::Mat &single_color_img,
    const std::vector<cv::Point2f> &except_points = std::vector<cv::Point2f>())
{
  std::vector<std::pair<int32_t, cv::Point2f>> marker_list;
  for (const auto &contour : contours)
  {
    // erase tiny-area contour
    if (cv::contourArea(contour) < lightness_img.size().area() * 0.001)
      continue;

    /* create contour_mask for calculating pixel value in only region of contour */
    cv::Point2f contour_center;
    float_t contour_radius;
    cv::minEnclosingCircle(contour, contour_center, contour_radius);

    if (std::find(except_points.begin(), except_points.end(), contour_center) != except_points.end())
      continue;

    cv::Mat circle_mask = cv::Mat::zeros(lightness_img.size(), CV_8UC1);
    cv::fillConvexPoly(circle_mask, contour, cv::Scalar(255, 0, 0));

    const auto bounding_rect = cv::boundingRect(contour);
    circle_mask = ImgSize::get_img_roi(circle_mask, bounding_rect).clone();
    const auto single_color_img_roi = ImgSize::get_img_roi(single_color_img, bounding_rect);
    /* end: create contour_mask for calculating pixel value in only region of contour */

    marker_list.push_back({value_calculate_callback(single_color_img_roi, circle_mask), contour_center});
  }

  // gather markers in head of list by using greater-sort algorithmn */
  std::sort(marker_list.begin(), marker_list.end(),
            [](const auto &a, const auto &b)
            { return a.first > b.first; });

  return marker_list;
}

/// @brief arrange and get markers so that blue marker is the head: direction -> clockwise
/// @param rotate_base_point Rotate base point
/// @param blue_marker_center Blue marker's center position
/// @param green_marker_centers Green markers' center position
/// @return Arranged marker's center position list, head is blue marker
static std::vector<cv::Point2f> get_clockwise_direction_markers(
    const cv::Point2f &rotate_base_point,
    const std::vector<std::pair<int32_t, cv::Point2f>> &blue_marker_centers,
    const std::vector<std::pair<int32_t, cv::Point2f>> &green_marker_centers)
{
  const auto &blue_marker_center = blue_marker_centers.at(0).second;
  std::vector<std::pair<int32_t, size_t>> angle_to_idx_conversion_list;

  for (size_t marker_idx = 0; marker_idx < green_marker_centers.size(); marker_idx++)
  {
    const auto &green_marker_center = green_marker_centers.at(marker_idx).second;

    // clockwise angle
    auto delta_angle =
        ImgProc::calc_angle_degree_formed_by_vectors(blue_marker_center,
                                                     green_marker_center,
                                                     rotate_base_point);
    // clockwise negative angle (-180 < theta < 0) -> positive angle (180 < theta' < 360)
    delta_angle = static_cast<int32_t>(std::floor((delta_angle >= 0) ? delta_angle : (360.0f + delta_angle)));

    angle_to_idx_conversion_list.push_back({delta_angle, marker_idx});
  }
  std::sort(angle_to_idx_conversion_list.begin(), angle_to_idx_conversion_list.end(),
            [](const auto &a, const auto &b)
            { return a.first < b.first; });

  std::vector<cv::Point2f> detected_marker_points{blue_marker_center};
  for (const auto &angle_to_idx_conversion : angle_to_idx_conversion_list)
  {
    const auto &green_marker_idx = angle_to_idx_conversion.second;
    const auto &green_marker_center = green_marker_centers.at(green_marker_idx).second;
    detected_marker_points.push_back(green_marker_center);
  }

  return detected_marker_points;
}

/// @brief detect beacon_device_markers (green or blue LED x 4 <1:3>)
/// @param analyzed_picture Image which have LED markers
/// @return Four center points of detected beacon_device_markers
static std::vector<cv::Point2f> detect_beacon_device_markers(const cv::Mat &analyzed_picture)
{
  /* split color_channels (Lab space, hsv space) */
  cv::Mat analyzed_picture_lab;
  cv::cvtColor(analyzed_picture, analyzed_picture_lab, cv::COLOR_BGR2Lab);
  std::vector<cv::Mat> analyzed_picture_lab_list;
  cv::split(analyzed_picture_lab, analyzed_picture_lab_list);
  auto &analyzed_picture_l = analyzed_picture_lab_list.at(0);
  auto &analyzed_picture_lab_g = analyzed_picture_lab_list.at(1);
  auto &analyzed_picture_lab_b = analyzed_picture_lab_list.at(2);

  cv::Mat analyzed_picture_hsv, beacon_mask_1, beacon_mask_2, beacon_mask;
  cv::cvtColor(analyzed_picture, analyzed_picture_hsv, cv::COLOR_BGR2HSV);
  cv::inRange(analyzed_picture_hsv, cv::Scalar(0, 0, 0), cv::Scalar(40, 255, 255), beacon_mask_1);
  cv::inRange(analyzed_picture_hsv, cv::Scalar(150, 0, 0), cv::Scalar(180, 255, 255), beacon_mask_2);
  cv::bitwise_or(beacon_mask_1, beacon_mask_2, beacon_mask);
  /* end: split color_channels (Lab space, hsv space) */

  /* preprocess */
  cv::Mat analyzed_picture_l_mask;
  cv::threshold(analyzed_picture_l, analyzed_picture_l_mask, 0.0, 255.0, cv::THRESH_OTSU);
  analyzed_picture_l_mask -= beacon_mask;
  cv::normalize(analyzed_picture_lab_b, analyzed_picture_lab_b, 0.0, 255.0, cv::NORM_MINMAX, -1, analyzed_picture_l_mask);
  cv::normalize(analyzed_picture_lab_g, analyzed_picture_lab_g, 0.0, 255.0, cv::NORM_MINMAX, -1, analyzed_picture_l_mask);
  cv::bitwise_not(analyzed_picture_lab_g, analyzed_picture_lab_g);
  cv::bitwise_not(analyzed_picture_lab_b, analyzed_picture_lab_b);
  /* end: preprocess */

  /* find contours */
  std::vector<cv::Vec4i> _hierarchy;
  std::vector<std::vector<cv::Point>> analyzed_picture_contours;
  cv::findContours(analyzed_picture_l_mask, analyzed_picture_contours, _hierarchy, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);
  /* end: find contours */

  auto green_marker_centers =
      extract_marker_list(analyzed_picture_contours, ImgProc::calc_pixel_mean_with_mask,
                          analyzed_picture_l_mask, analyzed_picture_lab_g);

  if (green_marker_centers.size() < 3)
    return std::vector<cv::Point2f>();

  green_marker_centers.erase(green_marker_centers.begin() + 3, green_marker_centers.end());

  std::vector<cv::Point2f> except_points;
  for (const auto &[_, point] : green_marker_centers)
    except_points.push_back(point);

  auto blue_marker_centers =
      extract_marker_list(analyzed_picture_contours, ImgProc::calc_pixel_mean_with_mask,
                          analyzed_picture_l_mask, analyzed_picture_lab_b, except_points);

  if (blue_marker_centers.empty())
    return std::vector<cv::Point2f>();

  blue_marker_centers.erase(blue_marker_centers.begin() + 1, blue_marker_centers.end());

  const auto analyzed_picture_center = static_cast<cv::Point2f>(analyzed_picture.size()) / 2.0f;
  const auto detected_marker_points =
      get_clockwise_direction_markers(analyzed_picture_center, blue_marker_centers, green_marker_centers);

  return detected_marker_points;
}

/// @brief normalize led_value
/// @param led_value Original led value
/// @param normalized_level Normalized_level
/// @param min_value Minimum value of all led value
/// @param max_value Maximum value of all led value
/// @return Normalized value
static uint8_t normalize_led_value(const uint8_t &led_value, const uint8_t &normalize_level,
                                   const uint8_t &min_value, const uint8_t &max_value)
{
  const auto delta = led_value - min_value;
  const auto divider = max_value - min_value;
  return static_cast<uint8_t>((static_cast<double>(delta) / divider) * static_cast<uint32_t>(normalize_level));
}

/// @brief analyze LED_pattern
/// @param analyzed_picture Image which has analyzed LED beacons
/// @param device_definition DeviceDefinition object
/// @return Analyzed LED Pattern (Level of 0~31)
static std::unordered_map<std::string, uint8_t> analyze_led_pattern(
    const cv::Mat &analyzed_picture,
    const DeviceDefinition &device_definition)
{
  /* split color channels (Lab space) */
  cv::Mat analyzed_picture_lab;
  std::vector<cv::Mat> analyzed_picture_lab_list;
  cv::cvtColor(analyzed_picture, analyzed_picture_lab, cv::COLOR_BGR2Lab);
  cv::split(analyzed_picture_lab, analyzed_picture_lab_list);
  const auto &analyzed_picture_b = analyzed_picture_lab_list.at(2);
  /* end: split color channels (Lab space) */

  /* calculate led_value for classification */
  std::unordered_map<std::string, uint8_t> led_pattern_hash;
  for (const auto &[beacon_key, beacon] : device_definition.m_beaconHash)
  {
    // improve software performance using roi
    const auto analyzed_picture_b_roi = ImgSize::get_img_roi(analyzed_picture_b, beacon.m_boundingRect);
    led_pattern_hash[beacon_key] = static_cast<uint8_t>(ImgProc::calc_pixel_mean_with_mask(analyzed_picture_b_roi, beacon.m_ledMask));
  }
  /* end: calculate led_value for classification */

  /* normalize (unordered_map's default sort: by 'key' -> sort by 'value') */
  std::unordered_map<std::string, uint8_t> led_pattern_normalized_hash;
  const auto &[_max_key, led_pattern_max_value] =
      *std::max_element(led_pattern_hash.begin(), led_pattern_hash.end(),
                        [](const auto &a, const auto &b)
                        {
                          return a.second < b.second;
                        });
  const auto &[_min_key, led_pattern_min_value] =
      *std::min_element(led_pattern_hash.begin(), led_pattern_hash.end(),
                        [](const auto &a, const auto &b)
                        {
                          return a.second < b.second;
                        });

  for (const auto &[beacon_key, led_pattern] : led_pattern_hash)
    led_pattern_normalized_hash[beacon_key] =
        normalize_led_value(led_pattern, 31U, led_pattern_min_value, led_pattern_max_value);
  /* end: normalize */

  return led_pattern_normalized_hash;
}

BeaconAnalyzer::BeaconAnalyzer(const std::string &definition_file_path)
{
  const auto definition_json = nlohmann::json::parse(read_file(definition_file_path));
  m_deviceNames = definition_json["device_name"];

  for (const std::string &device_name : m_deviceNames)
  {
    const auto &device_json = definition_json[device_name];
    const auto &marker_json = device_json["marker"];
    const auto &beacon_json = device_json["beacon"];

    const auto device_template_size = cv::Size(device_json["template_width"], device_json["template_height"]);

    std::unordered_map<std::string, LedData> marker_hash;
    const int32_t marker_led_num = marker_json["led_num"];

    for (int32_t led_idx = 1; led_idx <= marker_led_num; led_idx++)
    {
      const auto led_key = "ID" + std::to_string(led_idx);
      const auto &led_json = marker_json[led_key];
      marker_hash[led_key] = get_led_data_from_json(led_json, device_template_size);
    }

    std::unordered_map<std::string, LedData> beacon_hash;
    const int32_t beacon_led_num = beacon_json["led_num"];

    for (int32_t led_idx = 1; led_idx <= beacon_led_num; led_idx++)
    {
      const auto led_key = "ID" + std::to_string(led_idx);
      const auto &led_json = beacon_json[led_key];
      beacon_hash[led_key] = get_led_data_from_json(led_json, device_template_size);
    }

    DeviceDefinition definition;
    definition.m_deviceName = device_name;
    definition.m_deviceTemplateSize = std::move(device_template_size);
    definition.m_markerHash = std::move(marker_hash);
    definition.m_beaconHash = std::move(beacon_hash);

    m_deviceDefinitions[device_name] = std::move(definition);
  }
}

void BeaconAnalyzer::dump_device_definitions() const
{
  for (const auto &[_, definition] : m_deviceDefinitions)
    dump_device_definition(definition);
}

AnalyzationResult BeaconAnalyzer::analyzePicture(const cv::Mat &picture, const DetectionResult &detection_result) const
{
  const auto &device_definition = m_deviceDefinitions.at(detection_result.m_deviceName);
  auto analyzed_picture = ImgSize::get_img_roi(picture, detection_result.m_positionRect).clone();

  // perform beacon_device_markers detection, and get four points
  const auto homography_src_points = detect_beacon_device_markers(analyzed_picture);
  if (homography_src_points.size() != 4)
  {
    std::cout << "failed to find markers" << std::endl;
    AnalyzationResult dummy;
    dummy.m_deviceName = detection_result.m_deviceName;
    dummy.m_deviceId = detection_result.m_deviceId;
    dummy.m_devicePositionRect = detection_result.m_positionRect;
    for (const auto &[led_key, _] : device_definition.m_beaconHash)
      dummy.m_ledPatternHash[led_key] = 0;

    return dummy;
  }

  /* create homography dst points from marker points on device_definition_json */
  std::vector<cv::Point2f> homography_dst_points;
  for (uint32_t marker_id = 1; marker_id <= 4; marker_id++)
  {
    const auto &marker = device_definition.m_markerHash.at("ID" + std::to_string(marker_id));
    homography_dst_points.push_back(marker.m_position);
  }
  /* end: create homography dst points from marker points on device_definition_json */

  /* perform image registration and transform */
  const auto homography_mat = cv::getPerspectiveTransform(homography_src_points, homography_dst_points);
  cv::warpPerspective(ImgSize::get_img_roi(picture, detection_result.m_positionRect), analyzed_picture,
                      homography_mat, device_definition.m_deviceTemplateSize);
  /* end: perform image registration and transform */

  AnalyzationResult analyzation_result;
  analyzation_result.m_deviceName = detection_result.m_deviceName;
  analyzation_result.m_deviceId = detection_result.m_deviceId;
  analyzation_result.m_devicePositionRect = detection_result.m_positionRect;
  analyzation_result.m_ledPatternHash = analyze_led_pattern(analyzed_picture, device_definition);
  analyzation_result.m_analyzedPictureResult = std::move(analyzed_picture);

  return analyzation_result;
}

void GCB::AnalyzationResultWriter::writeAnalyzedLedPattern(
    const AnalyzationResult &analyzation_result,
    const uint64_t &frame_count)
{
  const auto &position_rect = analyzation_result.m_devicePositionRect;
  const auto device_key = analyzation_result.m_deviceName + std::to_string(analyzation_result.m_deviceId);

  const auto frame_id = "Frame" + std::to_string(frame_count);

  m_jsonData[frame_id]["device_keys"].emplace(device_key, device_key);

  m_jsonData[frame_id][device_key]["device_name"] = analyzation_result.m_deviceName;

  m_jsonData[frame_id][device_key]["position"] = {
      {"x", position_rect.x},
      {"y", position_rect.y},
      {"width", position_rect.width},
      {"height", position_rect.height}};

  for (const auto &[beacon_id, led_pattern] : analyzation_result.m_ledPatternHash)
    m_jsonData[frame_id][device_key]["beacon"][beacon_id] = led_pattern;
}

void GCB::AnalyzationResultWriter::outputJson(const std::string &json_file_path, const uint64_t &frame_count)
{
  m_jsonData["frame_num"] = frame_count;

  std::ofstream json_ofs(json_file_path);
  json_ofs << m_jsonData.dump();
}

std::string GCB::AnalyzationResultWriter::getJsonString(const uint64_t &frame_count)
{
  m_jsonData["frame_num"] = frame_count;

  return m_jsonData.dump();
}