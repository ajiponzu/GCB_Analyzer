#include "../ApiServer.hpp"

#include <chrono>
using namespace std::chrono_literals;

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <unordered_map>

#include <cstdio>
#include <cstdlib>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static std::shared_ptr<GCB::BeaconAnalyzer> gptr_beacon_analyzer = nullptr;

struct ProcessState
{
  double m_progression = 0.0;
  uint64_t m_frameCount = 0;
  bool m_isCompleted = false;
};

static std::unordered_set<::pid_t> g_video_request_id_set; // key: client_id (base-> analyze_video's process id)

/// @brief create file path
/// @param base_path Api's directory and base_name
/// @param pid Process id
/// @param extension File's extension
/// @return File_path
static std::string create_process_file_path(
    const std::string &base_path, const ::pid_t &pid, const std::string &extension)
{
  std::stringstream file_path_stream;
  file_path_stream << base_path << pid << extension;

  return file_path_stream.str();
}

/// @brief create mapped memory for connecting another process
/// @param mapped_file_path Memory mapped file path
/// @return Reference (pointer) to mapped memory
static char *create_mapped_memory(const std::string &mapped_file_path, const int32_t &read_write_op)
{
  auto mmap_fd = ::open(mapped_file_path.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  ::lseek(mmap_fd, sizeof(ProcessState) + 1U, SEEK_SET);
  (void)::write(mmap_fd, "", 1);
  ::lseek(mmap_fd, 0, SEEK_SET);

  char *const file_mapped_memory =
      // reinterpret_cast<char *>(::mmap(0, sizeof(ProcessState), PROT_READ | PROT_WRITE, MAP_SHARED, mmap_fd, 0));
      reinterpret_cast<char *>(::mmap(0, sizeof(ProcessState), read_write_op, MAP_SHARED, mmap_fd, 0));
  ::close(mmap_fd);

  return file_mapped_memory;
}

/// @brief return device_detection_result_list (from json_string)
/// @param json_string Json format string
/// @return Vector of device_detection
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

/// @brief analyze beacon on picture (using GCB module)
/// @param picture It contains beacon device
/// @param detection_result_list Vector of GCB::DetecionResult
/// @return Json String as analyzed picture
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

/// @brief analyze beacon on video (using GCB module)
/// @param video_file_path Analyzed video path
/// @param detection_result_list Vector of GCB::DetecionResult
static void analyze_video(const std::string &video_file_path, const std::vector<GCB::DetectionResult> &detection_result_list)
{
  const auto pid = ::getpid();
  const std::string mmap_file_path = create_process_file_path("../data/memory_map/analyze", pid, ".dat");
  const auto file_mapped_memory = create_mapped_memory(mmap_file_path, PROT_WRITE);

  cv::VideoCapture video_cap(video_file_path);
  const auto video_frame_number = video_cap.get(cv::VideoCaptureProperties::CAP_PROP_FRAME_COUNT);

  GCB::AnalyzationResultWriter analyzation_result_writer;
  uint64_t frame_count = 0;
  while (true)
  {
    cv::Mat frame;
    if (!video_cap.read(frame))
      break;

    for (const auto &detection_result : detection_result_list)
    {
      const auto analyzation_result = gptr_beacon_analyzer->analyzePicture(frame, detection_result);
      analyzation_result_writer.writeAnalyzedLedPattern(analyzation_result, frame_count);
    }

    ProcessState analyzation_state;
    analyzation_state.m_progression = static_cast<double>(frame_count) / video_frame_number;
    analyzation_state.m_frameCount = frame_count;
    std::memcpy(file_mapped_memory, reinterpret_cast<char *>(&analyzation_state), sizeof(ProcessState));

    frame_count++;
  }

  analyzation_result_writer.outputJson(
      create_process_file_path("../data/analyze/result_", pid, ".json"), frame_count);

  ProcessState analyzation_state;
  analyzation_state.m_isCompleted = true;
  analyzation_state.m_progression = 1.0;
  analyzation_state.m_frameCount = frame_count;
  std::memcpy(file_mapped_memory, &analyzation_state, sizeof(ProcessState));
  ::munmap(file_mapped_memory, sizeof(ProcessState));
}

/// @brief create visualization video of analyzation_result
/// @param analyzation_json_path Video analyzation result json file path
/// @param video_file_path Analyzed Video file path
static void visualize_analyzation_result(const std::string &analyzation_json_path, const std::string &video_file_path)
{
  const auto pid = ::getpid();
  const std::string mmap_file_path = create_process_file_path("../data/memory_map/visualize", pid, ".dat");
  const auto file_mapped_memory = create_mapped_memory(mmap_file_path, PROT_WRITE);

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
  video_writer.open(
      create_process_file_path("../data/visualize/result_", pid, ".mp4"),
      video_fourcc, video_fps, cv::Size(1500, 1000));

  uint64_t frame_count = 0;
  while (true)
  {
    if (frame_count >= frame_num)
      break;

    cv::Mat frame, visualized_img;
    if (!video_cap.read(frame))
      break;

    const auto frame_obj = json_obj["Frame" + std::to_string(frame_count)];
    std::map<std::string, std::string> device_keys_map = frame_obj["device_keys"];
    std::vector<std::string> device_keys;
    for (const auto &[device_key, _] : device_keys_map)
      device_keys.push_back(device_key);
    std::sort(device_keys.begin(), device_keys.end());

    for (const auto &device_key : device_keys)
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

    ProcessState visualization_state;
    visualization_state.m_progression = static_cast<double>(frame_count) / static_cast<double>(frame_num);
    visualization_state.m_frameCount = frame_count;
    std::memcpy(file_mapped_memory, reinterpret_cast<char *>(&visualization_state), sizeof(ProcessState));

    frame_count++;
  }
  video_writer.release();

  ProcessState visualization_state;
  visualization_state.m_isCompleted = true;
  visualization_state.m_progression = 1.0;
  visualization_state.m_frameCount = frame_count;
  std::memcpy(file_mapped_memory, &visualization_state, sizeof(ProcessState));
  ::munmap(file_mapped_memory, sizeof(ProcessState));
}

/// @brief regist api server's event handler (url method)
static void regist_request_handler()
{
  // analyze picture and send analyzation result Json
  drogon::app().registerHandler("/analyze_picture",
                                [](const drogon::HttpRequestPtr &request,
                                   std::function<void(const drogon::HttpResponsePtr &)> &&callback)
                                {
                                  drogon::MultiPartParser uploaded_file;
                                  uploaded_file.parse(request);

                                  const auto &image_binary_file = uploaded_file.getFilesMap().at("image_file");
                                  std::vector<char> binary_list(image_binary_file.fileLength());
                                  std::memcpy(binary_list.data(), image_binary_file.fileContent().data(), image_binary_file.fileLength());
                                  const auto analyzed_picture = cv::imdecode(binary_list, cv::IMREAD_COLOR);

                                  // .fileContent() ignores tail of file added packet (not contained in original data).
                                  const auto request_json_file_string =
                                      std::string(uploaded_file.getFilesMap().at("request_json").fileContent());
                                  const auto detection_result_list = get_device_detection_list_from_json(request_json_file_string);

                                  const auto result_json_string = analyze_picture(analyzed_picture, detection_result_list);
                                  auto response = drogon::HttpResponse::newHttpResponse();
                                  response->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
                                  response->setBody(result_json_string);
                                  callback(response);
                                },
                                {drogon::Post});

  drogon::app().registerHandler("/analyze_video/{video-path}",
                                [](const drogon::HttpRequestPtr &request,
                                   std::function<void(const drogon::HttpResponsePtr &)> &&callback, const std::string &video_path)
                                {
                                  drogon::MultiPartParser uploaded_file;
                                  uploaded_file.parse(request);
                                  const auto &video_binary_file = uploaded_file.getFilesMap().at("video");

                                  ::pid_t analyze_process_id;
                                  if (video_path != "")
                                  {
                                    video_binary_file.saveAs(video_path);

                                    const auto request_json_file_string = std::string(uploaded_file.getFilesMap().at("request_json").fileContent());
                                    const auto detection_result_list = get_device_detection_list_from_json(request_json_file_string);

                                    if ((analyze_process_id = ::fork()) == 0)
                                    {
                                      analyze_video("../uploads/" + video_path, detection_result_list);
                                      ::_exit(EXIT_SUCCESS);
                                    }
                                  }

                                  g_video_request_id_set.insert(analyze_process_id);

                                  auto response = drogon::HttpResponse::newHttpResponse();
                                  response->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
                                  if (video_path != "")
                                  {
                                    nlohmann::json json_obj;
                                    json_obj["access_id"] = analyze_process_id;
                                    response->setBody(json_obj.dump());
                                  }
                                  else
                                    response->setBody(R"({ "error": "filePath is not found" })");

                                  callback(response);
                                },
                                {drogon::Post});

  drogon::app().registerHandler("/analyzation_result/{access-id}",
                                [](const drogon::HttpRequestPtr &,
                                   std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                                   const ::pid_t &access_id)
                                {
                                  auto response = drogon::HttpResponse::newHttpResponse();
                                  response->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);

                                  if (g_video_request_id_set.find(access_id) == g_video_request_id_set.end())
                                  {
                                    response->setBody(R"({ "error": "'access-id' is not valid" })");
                                    callback(response);
                                    return;
                                  }

                                  const std::string mmap_file_path = create_process_file_path("../data/memory_map/analyze", access_id, ".dat");
                                  const auto file_mapped_memory = create_mapped_memory(mmap_file_path, PROT_READ | PROT_WRITE);

                                  ProcessState analyzation_state;
                                  std::memcpy(reinterpret_cast<char *>(&analyzation_state), file_mapped_memory, sizeof(ProcessState));
                                  ::munmap(file_mapped_memory, sizeof(ProcessState));

                                  if (analyzation_state.m_isCompleted)
                                  {
                                    std::ifstream json_ifs(create_process_file_path("../data/analyze/result_", access_id, ".json"));
                                    std::string file_content, line;
                                    while (std::getline(json_ifs, line))
                                      file_content.append(line);

                                    ::remove(mmap_file_path.c_str());
                                    response->setBody(file_content);
                                  }
                                  else
                                  {
                                    const auto progression = analyzation_state.m_progression;
                                    nlohmann::json json_obj;
                                    json_obj["progression"] = progression;

                                    response->setBody(json_obj.dump());
                                  }

                                  callback(response);
                                },
                                {drogon::Get});

  drogon::app().registerHandler("/visualize_analyzation_result/{access-id}/{}",
                                [](const drogon::HttpRequestPtr &,
                                   std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                                   const pid_t &access_id, const std::string &video_path)
                                {
                                  ::pid_t visualize_process_id;
                                  if (g_video_request_id_set.find(access_id) != g_video_request_id_set.end())
                                  {
                                    if ((visualize_process_id = ::fork()) == 0)
                                    {
                                      visualize_analyzation_result(
                                          create_process_file_path("../data/analyze/result_", access_id, ".json"),
                                          "../uploads/" + video_path);
                                      ::_exit(EXIT_SUCCESS);
                                    }
                                  }

                                  g_video_request_id_set.insert(visualize_process_id);

                                  auto response = drogon::HttpResponse::newHttpResponse();
                                  response->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
                                  if (g_video_request_id_set.find(access_id) != g_video_request_id_set.end())
                                  {
                                    nlohmann::json json_obj;
                                    json_obj["access_id"] = visualize_process_id;
                                    response->setBody(json_obj.dump());
                                  }
                                  else
                                    response->setBody(R"({ "error": "'access-id' is not valid" })");

                                  callback(response);
                                },
                                {drogon::Post});

  drogon::app().registerHandler("/visualization_result/{access-id}",
                                [](const drogon::HttpRequestPtr &,
                                   std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                                   const ::pid_t &access_id)
                                {
                                  if (g_video_request_id_set.find(access_id) == g_video_request_id_set.end())
                                  {
                                    auto response = drogon::HttpResponse::newHttpResponse();
                                    response->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
                                    response->setBody(R"({ "error": "'access-id' is not valid" })");
                                    callback(response);
                                    return;
                                  }

                                  const std::string mmap_file_path = create_process_file_path("../data/memory_map/visualize", access_id, ".dat");
                                  const auto file_mapped_memory = create_mapped_memory(mmap_file_path, PROT_READ | PROT_WRITE);

                                  ProcessState visualizaion_state;
                                  std::memcpy(reinterpret_cast<char *>(&visualizaion_state), file_mapped_memory, sizeof(ProcessState));
                                  ::munmap(file_mapped_memory, sizeof(ProcessState));

                                  auto response = drogon::HttpResponse::newHttpResponse();
                                  if (visualizaion_state.m_isCompleted)
                                  {
                                    std::ifstream video_ifs(
                                        create_process_file_path("../data/visualize/result_", access_id, ".mp4"),
                                        std::ios::binary);

                                    video_ifs.seekg(0, std::ios::end);
                                    const size_t file_size = video_ifs.tellg();
                                    video_ifs.seekg(0, std::ios::beg);

                                    std::string file_content;
                                    file_content.resize(file_size);
                                    video_ifs.read(file_content.data(), file_size);

                                    ::remove(mmap_file_path.c_str());
                                    g_video_request_id_set.erase(access_id);

                                    response->setContentTypeCode(drogon::ContentType::CT_TEXT_PLAIN);
                                    response->setBody(file_content);
                                  }
                                  else
                                  {
                                    const auto progression = visualizaion_state.m_progression;
                                    nlohmann::json json_obj;
                                    json_obj["progression"] = progression;

                                    response->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
                                    response->setBody(json_obj.dump());
                                  }

                                  callback(response);
                                },
                                {drogon::Get});
}

/// @brief observe child process (if this func is not running, parent 'drogon-core' process will not be released forever.)
static void mask_signal_child()
{
  ::sigset_t sigset;
  ::sigemptyset(&sigset);
  ::sigaddset(&sigset, SIGCHLD);
  ::pthread_sigmask(SIG_BLOCK, &sigset, NULL);
}

/// @brief stop to observe child process (it is called only when server program exit)
static void unmask_signal_child()
{
  int status;
  ::wait(&status); // wait for all child process stopped
  std::cout << "chiled process kill" << std::endl;

  ::sigset_t sigset;
  ::sigemptyset(&sigset);
  ::sigaddset(&sigset, SIGCHLD);
  ::pthread_sigmask(SIG_UNBLOCK, &sigset, NULL);
}

void ApiServer::bootServer(const std::string &ip_addr_str, const uint16_t &port_num)
{
  std::ios::sync_with_stdio(false);
  std::filesystem::create_directory("../data/memory_map");
  std::filesystem::create_directory("../data/analyze");
  std::filesystem::create_directory("../data/visualize");
  mask_signal_child();

  gptr_beacon_analyzer =
      std::make_shared<GCB::BeaconAnalyzer>(
          std::move(GCB::BeaconAnalyzer("../assets/beacon_device_definition.json")));

  regist_request_handler();

  static constexpr size_t CLIENT_MAX_BODY_SIZE = 30U * 1000U * 1000U * 1000U; // 30 Gib byte

  drogon::app()
      .setClientMaxBodySize(CLIENT_MAX_BODY_SIZE)
      .setThreadNum(0)
      .setUploadPath("../uploads")
      .addListener(ip_addr_str, port_num)
      .run();

  unmask_signal_child();
  std::filesystem::remove_all("../data/memory_map");
  std::filesystem::remove_all("../data/analyze");
  std::filesystem::remove_all("../data/visualize");
  std::filesystem::remove_all("../uploads");
}
