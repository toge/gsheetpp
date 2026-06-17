#pragma once

/**
 * @file refresh_in_progress_guard.hpp
 * @brief token refresh 中フラグを例外安全に取り扱う RAII ガードを宣言します。
 */

#include <condition_variable>
#include <mutex>

namespace gsheetpp::detail {

/**
 * @brief refresh 中フラグを例外安全に解除する RAII ガードです。
 *
 * token 取得処理の途中で失敗しても待機中スレッドが永続停止しないようにします。
 * このクラスは内部実装の詳細であり、利用者が直接生成する必要はありません。
 */
class RefreshInProgressGuard {
public:
  /**
   * @brief ガード対象を初期化します。
   * @param mutex refresh 状態を保護する mutex です。
   * @param cv refresh 完了通知用の condition_variable です。
   * @param refresh_in_progress refresh 中フラグへの参照です。
   */
  RefreshInProgressGuard(std::mutex& mutex, std::condition_variable& cv, bool& refresh_in_progress) noexcept;

  RefreshInProgressGuard(RefreshInProgressGuard const&)                    = delete;
  auto operator=(RefreshInProgressGuard const&) -> RefreshInProgressGuard& = delete;
  RefreshInProgressGuard(RefreshInProgressGuard&&)                         = delete;
  auto operator=(RefreshInProgressGuard&&) -> RefreshInProgressGuard&      = delete;

  /**
   * @brief refresh 中フラグを解除し、待機中スレッドへ通知します。
   */
  ~RefreshInProgressGuard() noexcept;

private:
  std::mutex&              mutex_;                ///< 状態更新を直列化する mutex です。
  std::condition_variable& cv_;                   ///< 待機解除通知に使う condition_variable です。
  bool&                    refresh_in_progress_;  ///< 解除対象の refresh 中フラグです。
};

}  // namespace gsheetpp::detail
