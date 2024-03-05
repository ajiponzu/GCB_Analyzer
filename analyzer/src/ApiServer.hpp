#pragma once

#include <drogon/drogon.h>
#include "GCB.hpp"

namespace ApiServer
{
  /// @brief boot gcb-analyzer server
  /// @param ip_addr_str Server's ip address (string: "0.0.0.0")
  /// @param string Port number
  void bootServer(const std::string &ip_addr_str, const uint16_t &port_num);
};