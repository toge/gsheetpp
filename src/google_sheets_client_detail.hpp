#pragma once

#include "gsheetpp/google_sheets_client.hpp"

#include <chrono>
#include <expected>
#include <string_view>

/**
 * @brief 実装の詳細を保持する内部用名前空間
 */
namespace gsheetpp::detail {

/**
 * @brief トークンレスポンスの JSON を TokenInfo にパースします
 * @param json レスポンス JSON
 * @return パース結果
 */
auto parse_token_response(std::string_view json)
    -> std::expected<TokenInfo, GoogleSheetsError>;

/**
 * @brief トークンリクエストのエラーレスポンスをパースします
 * @param json エラー JSON
 * @param http_status HTTP ステータスコード
 * @return 常に std::unexpected(GoogleSheetsError)
 */
auto parse_token_error_response(std::string_view json, long http_status)
    -> std::expected<void, GoogleSheetsError>;

/**
 * @brief 値の読み取りレスポンスの JSON を ReadValuesResult にパースします
 * @param json レスポンス JSON
 * @return パース結果
 */
auto parse_read_values_response(std::string_view json)
    -> std::expected<ReadValuesResult, GoogleSheetsError>;

/**
 * @brief 値の書き込みレスポンスの JSON を WriteValuesResult にパースします
 * @param json レスポンス JSON
 * @return パース結果
 */
auto parse_write_values_response(std::string_view json)
    -> std::expected<WriteValuesResult, GoogleSheetsError>;

/**
 * @brief 一般的な API エラーレスポンスをパースします
 * @param json エラー JSON
 * @param http_status HTTP ステータスコード
 * @return 常に std::unexpected(GoogleSheetsError)
 */
auto parse_api_error_response(std::string_view json, long http_status)
    -> std::expected<void, GoogleSheetsError>;

/**
 * @brief 認証用の JWT アサーションを構築・署名します
 * @param config サービスアカウント設定
 * @param issued_at 発行時刻
 * @return 署名済み JWT 文字列
 */
auto build_jwt_assertion(
    ServiceAccountConfig const& config,
    std::chrono::system_clock::time_point issued_at)
    -> std::expected<std::string, GoogleSheetsError>;

/**
 * @brief トークン取得リクエストのボディを構築します
 * @param assertion JWT アサーション
 * @return x-www-form-urlencoded 形式の文字列
 */
auto build_token_request_body(std::string_view assertion) -> std::string;

/**
 * @brief 値の書き込みリクエストの JSON ボディを構築します
 * @param values 書き込む値のリスト
 * @return JSON 文字列
 */
auto build_write_values_request_body(std::vector<std::vector<std::string>> const& values)
    -> std::string;

/**
 * @brief テスト用のクライアントインスタンスを作成します
 * @param config 設定
 * @param transport カスタムトランスポート
 * @param clock カスタム時計
 * @return クライアントインスタンス
 */
auto make_google_sheets_client_for_test(
    ServiceAccountConfig config,
    TransportFunction transport,
    ClockFunction clock = {}) -> GoogleSheetsClient;

}  // namespace gsheetpp::detail
