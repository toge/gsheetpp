#pragma once

/**
 * @file http_client.hpp
 * @brief libcurl ベース HTTP transport の宣言です。
 */

#include "gsheetpp/google_sheets_client.hpp"

#include <expected>

/**
 * @brief 内部的な HTTP クライアント実装用名前空間
 */
namespace gsheetpp::http {

/**
 * @brief HTTP リクエストを実行します。デフォルトでは libcurl を使用します。
 * @param request 実行する HTTP リクエストの詳細
 * @return 成功した場合は HttpResponse、失敗した場合は GoogleSheetsError
 */
auto perform_http_request(detail::HttpRequest const& request) -> std::expected<detail::HttpResponse, GoogleSheetsError>;

}  // namespace gsheetpp::http
