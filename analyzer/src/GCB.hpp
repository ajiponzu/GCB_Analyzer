#pragma once

#include <iostream>
#include <string>
#include <unordered_map>

#include <opencv2/opencv.hpp>

#define JSON_HAS_CPP_17 1 // nlohmann-json magic code
#include "GCB/json.hpp"

// API providing for detection or analyzation of GCB device
namespace GCB
{
	// Outsider need not to access these information
	namespace Inside
	{
		// Information of LED on beacon device
		struct LedData
		{
			cv::Rect m_boundingRect;
			cv::Point2f m_position;
			float_t m_radius;
			std::string m_color;
			cv::Mat m_ledMask;
		};

		// Information of beacon device
		struct DeviceDefinition
		{
			std::string m_deviceName;
			cv::Size m_deviceTemplateSize;
			std::unordered_map<std::string, LedData> m_markerHash;
			std::unordered_map<std::string, LedData> m_beaconHash;
		};
	}

	// Device type and position of a detected beacon device
	struct DetectionResult
	{
		cv::Rect2f m_positionRect;
		std::string m_deviceName;
		uint64_t m_deviceId;
	};

	// Analysis result of LED lighting patterns by BeaconAnalyzer class
	struct AnalyzationResult
	{
		std::string m_deviceName;
		uint64_t m_deviceId; // Use only when GCB::AnalyzationResultWriter::getJsonString called
		cv::Rect m_devicePositionRect;
		std::unordered_map<std::string, uint8_t> m_ledPatternHash;
		cv::Mat m_analyzedPictureResult;
	};

	// Analyzer of LED beacon patterns on device
	class BeaconAnalyzer
	{
	private:
		std::vector<std::string> m_deviceNames;
		std::unordered_map<std::string, Inside::DeviceDefinition> m_deviceDefinitions;

	public:
		/// @brief default constructor
		BeaconAnalyzer() = default;

		/// @brief constructor
		/// @param definition_file_path File path of JSON about beacon device information
		BeaconAnalyzer(const std::string &definition_file_path);

		/// @brief destructor (non action)
		~BeaconAnalyzer() {}

		/// @brief output device_definitions
		void dump_device_definitions() const;

		/// @brief analyze LED lighting patterns on beacon device located in picture
		/// @param picture Picture used for detection and analyzed
		/// @param detection_result Device type and position of a detected beacon device (in the "picture")
		/// @return Analysis result of LED lighting patterns
		AnalyzationResult analyzePicture(const cv::Mat &picture, const DetectionResult &detection_result) const;

		const std::vector<std::string> &getDeviceNames() const { return m_deviceNames; }

		/// @brief getter deviceDefinitions
		const std::unordered_map<std::string, Inside::DeviceDefinition> &getDeviceDefinitions() const { return m_deviceDefinitions; }
	};

	class AnalyzationResultWriter
	{
	private:
		nlohmann::json m_jsonData;

	public:
		AnalyzationResultWriter() = default;

		/// @brief destructor (non action)
		~AnalyzationResultWriter() {}

		/* forbid copy action */
		AnalyzationResultWriter(const AnalyzationResultWriter &other) = delete;
		AnalyzationResultWriter(const AnalyzationResultWriter &&other) = delete;
		AnalyzationResultWriter operator=(const AnalyzationResultWriter other) const = delete;
		AnalyzationResultWriter operator=(const AnalyzationResultWriter &other) const = delete;
		AnalyzationResultWriter operator=(const AnalyzationResultWriter &&other) const = delete;
		/* end: forbid copy action */

		/// @brief insert analyzed LED_pattern into json tree
		/// @param detection_result Analyzation result
		/// @param frame_count Video_frame_count
		void writeAnalyzedLedPattern(
				const AnalyzationResult &analyzation_result,
				const uint64_t &frame_count = 0);

		/// @brief output json file
		/// @param output_json_path Json file about beacon analyzation result
		/// @param frame_count Video_frame_count
		void outputJson(const std::string &output_json_path, const uint64_t &frame_count = 0);

		/// @brief get m_jsonData string
		/// @param frame_count Video_frame_count
		/// @return String (Json content)
		std::string getJsonString(const uint64_t &frame_count = 0);
	};
};