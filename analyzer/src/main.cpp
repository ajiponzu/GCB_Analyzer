#include <chrono>
#include <fstream>

#include <drogon/drogon.h>

#include "ApiServer.hpp"

static std::shared_ptr<GCB::BeaconAnalyzer> gptr_beacon_analyzer = nullptr;

static std::vector<GCB::DetectionResult> get_device_detection_list_from_json(const std::string &json_string)
{
  std::vector<GCB::DetectionResult> detection_result_list;

  const auto json_obj = nlohmann::json::parse(json_string);
  const std::vector<std::string> device_key_list = json_obj["device_key"];

  for (const auto &device_key : device_key_list)
  {
    GCB::DetectionResult detection_result;
    detection_result.m_deviceName = json_obj[device_key]["device_name"];
    detection_result.m_deviceId = json_obj[device_key]["device_id"];
    detection_result.m_positionRect.x = json_obj[device_key]["rect_x"];
    detection_result.m_positionRect.y = json_obj[device_key]["rect_y"];
    detection_result.m_positionRect.width = json_obj[device_key]["rect_width"];
    detection_result.m_positionRect.height = json_obj[device_key]["rect_height"];

    detection_result_list.push_back(detection_result);
  }

  return detection_result_list;
}

static std::string analyze_picture(const cv::Mat &picture, const std::vector<GCB::DetectionResult> &detection_result_list)
{
  GCB::AnalyzationResultWriter analyzation_result_writer;

  for (const auto &detection_result : detection_result_list)
  {
    const auto analyzation_result = gptr_beacon_analyzer->analyzePicture(picture, detection_result);
    analyzation_result_writer.writeAnalyzedLedPattern(analyzation_result);
  }

  return analyzation_result_writer.getJsonString();
}

static void analyze_video(const std::string &video_file_path, const std::vector<GCB::DetectionResult> &detection_result_list)
{
  cv::VideoCapture video_cap(video_file_path);
  const auto video_frame_number = video_cap.get(cv::VideoCaptureProperties::CAP_PROP_FRAME_COUNT);

  cv::VideoWriter video_writer;
  const auto video_fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
  const auto video_fps = static_cast<int32_t>(video_cap.get(cv::VideoCaptureProperties::CAP_PROP_FPS));
  video_writer.open("../data/visualize/transform.mp4", video_fourcc, video_fps, cv::Size(1500, 1000));

  GCB::AnalyzationResultWriter analyzation_result_writer;
  uint64_t frame_count = 0;
  while (true)
  {
    cv::Mat frame, visualized_img;
    if (!video_cap.read(frame))
      break;

    for (const auto &detection_result : detection_result_list)
    {
      const auto analyzation_result = gptr_beacon_analyzer->analyzePicture(frame, detection_result);
      analyzation_result_writer.writeAnalyzedLedPattern(analyzation_result, frame_count);
      if (analyzation_result.m_analyzedPictureResult.empty())
        continue;

      cv::Mat view_img = analyzation_result.m_analyzedPictureResult.clone();
      const auto img_high_ratio = 500.0 / view_img.size().height;
      cv::resize(view_img, view_img, cv::Size(), img_high_ratio, img_high_ratio);

      if (visualized_img.empty())
        visualized_img = view_img.clone();
      else
        cv::hconcat(std::vector{visualized_img, view_img}, visualized_img);
    }

    if (!visualized_img.empty())
    {
      const auto img_wid_ratio = static_cast<double>(visualized_img.size().width) / frame.size().width;
      cv::resize(frame, frame, cv::Size(), img_wid_ratio, img_wid_ratio);
      cv::vconcat(std::vector{visualized_img, frame}, visualized_img);

      cv::resize(visualized_img, visualized_img, cv::Size(1500, 1000), 1.0, 1.0);
      video_writer.write(visualized_img);
    }

    frame_count++;
  }

  analyzation_result_writer.outputJson("../data/analyze/result.json", frame_count);
}

static void visualize_analyzation_result(const std::string &analyzation_json_path, const std::string &video_file_path)
{
  std::ifstream json_ifs(analyzation_json_path);

  if (json_ifs.fail())
  {
    std::cout << "File Open Error" << std::endl;
    return;
  }

  std::string json_str, line;
  while (std::getline(json_ifs, line))
    json_str.append(line);

  const auto json_obj = nlohmann::json::parse(json_str);

  const auto &device_definitions = gptr_beacon_analyzer->getDeviceDefinitions();
  const uint64_t frame_num = json_obj["frame_num"];

  cv::VideoCapture video_cap(video_file_path);
  const auto video_fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
  const auto video_fps = static_cast<int32_t>(video_cap.get(cv::VideoCaptureProperties::CAP_PROP_FPS));

  cv::VideoWriter video_writer;
  video_writer.open("../data/visualize/result.mp4", video_fourcc, video_fps, cv::Size(1500, 1000));

  uint64_t frame_count = 0;
  while (true)
  {
    if (frame_count >= frame_num)
      break;

    cv::Mat frame, visualized_img;
    if (!video_cap.read(frame))
      break;

    const auto frame_obj = json_obj["Frame" + std::to_string(frame_count)];
    std::map<std::string, std::string> device_keys = frame_obj["device_keys"];

    for (const auto &[device_key, _] : device_keys)
    {
      const auto device_obj = frame_obj[device_key];
      const std::string device_name = device_obj["device_name"];
      const auto &beacon_device = device_definitions.at(device_name);
      cv::Mat view_img = cv::Mat::zeros(beacon_device.m_deviceTemplateSize, CV_8UC3);

      if (view_img.size() == cv::Size())
      {
        std::cout << device_key + "'s result is None" << std::endl;
        continue;
      }

      for (const auto &[_, marker] : beacon_device.m_markerHash)
      {
        cv::Scalar marker_color;

        if (marker.m_color == "blue")
          marker_color[0] = 255;
        else
          marker_color[1] = 255;
        cv::circle(view_img, marker.m_position, static_cast<int32_t>(marker.m_radius), marker_color, -1);
      }

      for (const auto &[beacon_id, beacon] : beacon_device.m_beaconHash)
      {
        const uint8_t beacon_pattern = device_obj["beacon"][beacon_id];

        cv::circle(view_img, beacon.m_position, static_cast<int32_t>(beacon.m_radius),
                   cv::Scalar(0, 0, (255 / 32) * beacon_pattern), -1);
      }

      const auto img_high_ratio = 500.0 / view_img.size().height;
      cv::resize(view_img, view_img, cv::Size(), img_high_ratio, img_high_ratio);

      if (visualized_img.empty())
        visualized_img = view_img.clone();
      else
        cv::hconcat(std::vector{visualized_img, view_img}, visualized_img);
    }

    if (!visualized_img.empty())
    {
      const auto img_wid_ratio = static_cast<double>(visualized_img.size().width) / frame.size().width;
      cv::resize(frame, frame, cv::Size(), img_wid_ratio, img_wid_ratio);
      cv::vconcat(std::vector{visualized_img, frame}, visualized_img);

      cv::resize(visualized_img, visualized_img, cv::Size(1500, 1000), 1.0, 1.0);
      video_writer.write(visualized_img);
    }

    frame_count++;
  }
  video_writer.release();
}

void debug_video()
{
  std::ios::sync_with_stdio(false);
  std::filesystem::create_directory("../data/analyze");
  std::filesystem::create_directory("../data/visualize");

  gptr_beacon_analyzer =
      std::make_shared<GCB::BeaconAnalyzer>(
          std::move(GCB::BeaconAnalyzer("../assets/beacon_device_definition.json")));

  std::ifstream json_ifs("../data/request.json");
  std::string request_json_file_string, line;
  while (std::getline(json_ifs, line))
    request_json_file_string.append(line);

  const auto detection_result_list = get_device_detection_list_from_json(request_json_file_string);
  analyze_video("../data/input.mp4", detection_result_list);
  // analyze_video("../data/input.mov", detection_result_list);
  visualize_analyzation_result("../data/analyze/result.json", "../data/input.mp4");
}

int main()
{
  ApiServer::bootServer("0.0.0.0", 8080);
  // debug_video();

  return 0;
}