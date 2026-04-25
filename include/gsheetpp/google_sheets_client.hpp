#pragma once

#include <chrono>
#include <condition_variable>
#include <expected>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief Google Sheets API クライアントライブラリのメイン名前空間
 */
namespace gsheetpp {

/**
 * @brief クライアント API によって発生するエラーのカテゴリ
 */
enum class GoogleSheetsErrorKind {
  configuration,  ///< 設定ミス（必須フィールドの欠如など）
  serialization,  ///< JSON のシリアライズ/デシリアライズ失敗
  jwt_signing,    ///< JWT の署名失敗
  network,        ///< ネットワーク接続エラー
  http,           ///< HTTP エラー（4xx, 5xx）
  authentication, ///< 認証失敗（トークン取得エラーなど）
  api_response,   ///< Google Sheets API からのエラーレスポンス
};

/**
 * @brief API ヘルパーによって返される構造化されたエラー情報
 */
struct GoogleSheetsError {
  GoogleSheetsErrorKind kind{GoogleSheetsErrorKind::configuration}; ///< エラーの種類
  std::string message{};                                            ///< エラーメッセージ
  std::optional<long> http_status{};                                ///< HTTP ステータスコード（存在する場合）
  std::string response_body{};                                      ///< レスポンスボディ（存在する場合）
};

/**
 * @brief Google Cloud の JSON 認証情報から抽出されたサービスアカウント設定
 */
struct ServiceAccountConfig {
  std::string client_email{}; ///< サービスアカウントのメールアドレス
  std::string private_key{};  ///< RSA 非公開鍵
  std::string token_uri{};    ///< OAuth2 トークンエンドポイント
  std::string project_id{};   ///< プロジェクト ID
};

/**
 * @brief OAuth2 アクセストークンの情報
 */
struct TokenInfo {
  std::string access_token{};  ///< アクセストークン文字列
  std::string token_type{};    ///< トークンの種類（通常は Bearer）
  long expires_in_seconds{};   ///< 有効期限（秒）
};

/**
 * @brief Google Sheets から返される値の範囲の読み取り結果
 */
struct ReadValuesResult {
  std::string range{};                           ///< 読み取った範囲（A1 表記）
  std::string major_dimension{};                 ///< 主要な次元（ROWS または COLUMNS）
  std::vector<std::vector<std::string>> values{}; ///< 読み取った値（2次元配列）
};

/**
 * @brief Google Sheets に値を書き込んだ際の結果概要
 */
struct WriteValuesResult {
  std::string spreadsheet_id{}; ///< 対象のスプレッドシート ID
  std::string updated_range{};  ///< 更新された範囲（A1 表記）
  long updated_rows{};          ///< 更新された行数
  long updated_columns{};       ///< 更新された列数
  long updated_cells{};         ///< 更新されたセル総数
};

namespace detail {

/**
 * @brief 内部で使用される HTTP リクエストの構造
 */
struct HttpRequest {
  std::string method{};              ///< HTTP メソッド (GET, POST, PUT など)
  std::string url{};                 ///< ターゲット URL
  std::vector<std::string> headers{}; ///< HTTP ヘッダーリスト
  std::string body{};                ///< リクエストボディ
};

/**
 * @brief 内部で使用される HTTP レスポンスの構造
 */
struct HttpResponse {
  long status_code{};  ///< HTTP ステータスコード
  std::string body{};  ///< レスポンスボディ
};

/**
 * @brief HTTP トランスポート関数の型定義
 */
using TransportFunction =
    std::function<std::expected<HttpResponse, GoogleSheetsError>(HttpRequest const&)>;

/**
 * @brief 時計関数の型定義（テスト用）
 */
using ClockFunction = std::function<std::chrono::system_clock::time_point()>;

}  // namespace detail

/**
 * @brief サービスアカウント JWT 認証を使用した非同期 Google Sheets API クライアント
 */
class GoogleSheetsClient {
 public:
  /**
   * @brief デフォルトの libcurl トランスポートを使用してクライアントを構築します
   * @param config サービスアカウントの設定情報
   */
  explicit GoogleSheetsClient(ServiceAccountConfig config);

  /**
   * @brief 依存関係を注入してクライアントを構築します（主にテスト用）
   * @param config サービスアカウントの設定情報
   * @param transport HTTP トランスポートのコールバック
   * @param clock 有効期限管理に使用する時計のコールバック
   */
  GoogleSheetsClient(
      ServiceAccountConfig config,
      detail::TransportFunction transport,
      detail::ClockFunction clock = {});

  /**
   * @brief 非同期に認証を行い、OAuth2 アクセストークンを取得・キャッシュします
   * @return TokenInfo または GoogleSheetsError を含む std::future
   */
  auto authenticate_async() -> std::future<std::expected<TokenInfo, GoogleSheetsError>>;

  /**
   * @brief スプレッドシートから値の範囲を非同期に読み取ります
   * @param spreadsheet_id 対象のスプレッドシート ID
   * @param range A1 表記の範囲（例: "Sheet1!A1:B10"）
   * @return ReadValuesResult または GoogleSheetsError を含む std::future
   */
  auto read_values_async(std::string_view spreadsheet_id, std::string_view range)
      -> std::future<std::expected<ReadValuesResult, GoogleSheetsError>>;

  /**
   * @brief スプレッドシートの範囲に値を非同期に書き込みます（RAW モードを使用）
   * @param spreadsheet_id 対象のスプレッドシート ID
   * @param range A1 表記の範囲
   * @param data 書き込むデータ（行優先の2次元配列）
   * @return WriteValuesResult または GoogleSheetsError を含む std::future
   */
  auto write_values_async(
      std::string_view spreadsheet_id,
      std::string_view range,
      std::span<std::vector<std::string> const> data)
      -> std::future<std::expected<WriteValuesResult, GoogleSheetsError>>;

 private:
  struct SharedState;

  auto refresh_token() -> std::expected<TokenInfo, GoogleSheetsError>;
  auto get_valid_token() -> std::expected<TokenInfo, GoogleSheetsError>;

  ServiceAccountConfig config_{};
  detail::TransportFunction transport_{};
  detail::ClockFunction clock_{};
  std::shared_ptr<SharedState> shared_state_{};
};

/**
 * @brief Google サービスアカウントの認証情報 JSON 文字列をパースします
 * @param json サービスアカウント情報を含む UTF-8 JSON 文字列
 * @return パースされた ServiceAccountConfig または GoogleSheetsError
 */
auto parse_service_account_config(std::string_view json)
    -> std::expected<ServiceAccountConfig, GoogleSheetsError>;

}  // namespace gsheetpp
