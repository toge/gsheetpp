#pragma once

/**
 * @file google_sheets_client_detail.hpp
 * @brief テストや内部補助実装で共有する小さな定義をまとめます。
 */

#include "gsheetpp/google_sheets_client.hpp"

namespace gsheetpp::detail {

/**
 * @brief RefreshInProgressGuard のインライン定義 (例外安全のためヘッダに置く)。
 * @param mutex refresh 状態を保護する mutex です。
 * @param cv refresh 完了通知用 condition_variable です。
 * @param refresh_in_progress refresh 中フラグへの参照です。
 */
inline RefreshInProgressGuard::RefreshInProgressGuard(std::mutex& mutex, std::condition_variable& cv, bool& refresh_in_progress) noexcept
    : mutex_{mutex}, cv_{cv}, refresh_in_progress_{refresh_in_progress} {}

/**
 * @brief refresh 中フラグを解除し、待機中スレッドへ通知します。
 */
inline RefreshInProgressGuard::~RefreshInProgressGuard() noexcept {
  auto _               = std::scoped_lock{mutex_};
  refresh_in_progress_ = false;
  cv_.notify_all();
}

/**
 * @brief テスト用に transport / clock を差し替えたクライアントを生成します。
 * @param config サービスアカウント設定です。
 * @param transport テスト用 HTTP 実行関数です。
 * @param clock 任意の疑似時刻関数です。
 * @return テスト向けに構成した GoogleSheetsClient です。
 */
auto make_google_sheets_client_for_test(ServiceAccountConfig config, TransportFunction transport, ClockFunction clock = {}) -> GoogleSheetsClient;

}  // namespace gsheetpp::detail
