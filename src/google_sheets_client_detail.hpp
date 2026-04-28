#pragma once

#include "gsheetpp/google_sheets_client.hpp"

#include <condition_variable>
#include <expected>
#include <mutex>
namespace gsheetpp::detail {

inline RefreshInProgressGuard::RefreshInProgressGuard(
    std::mutex& mutex,
    std::condition_variable& cv,
    bool& refresh_in_progress) noexcept
    : mutex_{mutex},
      cv_{cv},
      refresh_in_progress_{refresh_in_progress} {}

inline RefreshInProgressGuard::~RefreshInProgressGuard() noexcept {
  auto _ = std::scoped_lock{mutex_};
  refresh_in_progress_ = false;
  cv_.notify_all();
}

auto make_google_sheets_client_for_test(
    ServiceAccountConfig config,
    TransportFunction transport,
    ClockFunction clock = {}) -> GoogleSheetsClient;

}  // namespace gsheetpp::detail
