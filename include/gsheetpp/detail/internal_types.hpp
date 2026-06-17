#pragma once

/**
 * @file internal_types.hpp
 * @brief 公開 API の引数・戻り値・内部実装で共有する軽量な内部型を集約します。
 *
 * include/gsheetpp/google_sheets_client.hpp から自動的に include されるため、
 * 利用者が直接 include する必要はありません。ライブラリのバージョン間で
 * 互換性のない変更が入る可能性のある領域です。
 */

#include <chrono>
#include <condition_variable>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gsheetpp {

/**
 * @brief Google Sheets クライアントが返すエラーの大分類です。
 */
enum class GoogleSheetsErrorKind {
  configuration,   ///< 構成値不足や不正な認証設定などの事前条件違反です。
  serialization,   ///< JSON などのシリアライズ・デシリアライズ失敗です。
  jwt_signing,     ///< JWT 署名の生成に失敗したことを表します。
  network,         ///< HTTP 送信前後のネットワーク層エラーです。
  http,            ///< HTTP 応答は得られたが期待形式では扱えない失敗です。
  authentication,  ///< トークン取得・更新など認証そのものの失敗です。
  api_response,    ///< Google API が返した業務エラー応答です。
};

/**
 * @brief クライアントが返す失敗情報です。
 */
struct GoogleSheetsError {
  GoogleSheetsErrorKind kind{GoogleSheetsErrorKind::configuration};  ///< エラー種別です。
  std::string           message{};                                   ///< 人間可読な失敗理由です。
  std::optional<long>   http_status{};                               ///< HTTP ステータスです。
  std::string           response_body{};                             ///< 失敗時の生レスポンス本文です。
};

/**
 * @brief 認証サーバーから取得したアクセストークン情報です。
 *
 * ClientSharedState など内部キャッシュでも参照されるため、internal_types に
 * 配置して前方宣言依存を避けます。
 */
struct TokenInfo {
  std::string access_token{};        ///< Bearer などのアクセストークン値です。
  std::string token_type{};          ///< Bearer や ApiKey などのトークン種別です。
  long        expires_in_seconds{};  ///< 発行時点からの有効秒数です。
};

namespace detail {

  /**
   * @brief トランスポート層へ渡す HTTP リクエスト表現です。
   */
  struct HttpRequest {
    std::string              method{};   ///< GET / POST / PUT などの HTTP メソッドです。
    std::string              url{};      ///< 絶対 URL です。
    std::vector<std::string> headers{};  ///< `Key: Value` 形式のヘッダー一覧です。
    std::string              body{};     ///< 送信本文です。
  };

  /**
   * @brief トランスポート層から返る HTTP 応答表現です。
   */
  struct HttpResponse {
    long        status_code{};  ///< HTTP ステータスコードです。
    std::string body{};         ///< 応答本文です。
  };

  /**
   * @brief HTTP リクエスト実行関数の型です。
   */
  using TransportFunction = std::function<std::expected<HttpResponse, GoogleSheetsError>(HttpRequest const&)>;

  /**
   * @brief 現在時刻取得関数の型です。
   *
   * テストでは疑似時刻を注入するために使います。
   */
  using ClockFunction = std::function<std::chrono::system_clock::time_point()>;

  /**
   * @brief クライアント複製間で共有する認証キャッシュ状態です。
   */
  struct ClientSharedState {
    std::mutex                            mutex{};                ///< 共有トークン状態全体を保護します。
    std::condition_variable               cv{};                   ///< refresh 完了待機に使います。
    bool                                  refresh_in_progress{};  ///< いま別スレッドが refresh しているかどうかです。
    std::optional<TokenInfo>              token{};                ///< 最後に取得したアクセストークンです。
    std::optional<std::string>            refresh_token{};        ///< 最新 refresh token の共有コピーです。
    std::chrono::system_clock::time_point expires_at{};           ///< token の失効予定時刻です。
  };

}  // namespace detail

}  // namespace gsheetpp
