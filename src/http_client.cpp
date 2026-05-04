#include "http_client.hpp"

#include <curl/curl.h>

#include <mutex>
#include <tuple>

/**
 * @file http_client.cpp
 * @brief libcurl を使って detail::HttpRequest を送信する実装です。
 */

namespace gsheetpp::http {

namespace {

  /**
   * @brief libcurl の書き込みコールバックです。
   * @param ptr libcurl が渡す受信データ先頭です。
   * @param size 要素サイズです。
   * @param nmemb 要素数です。
   * @param userdata std::string として解釈する蓄積先です。
   * @return 消費したバイト数です。
   */
  auto write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
    auto const total  = size * nmemb;
    auto*      buffer = static_cast<std::string*>(userdata);
    buffer->append(ptr, total);
    return total;
  }

  /**
   * @brief libcurl のグローバル初期化を 1 回だけ実行します。
   */
  auto ensure_curl_initialized() -> void {
    static auto once = std::once_flag{};
    std::call_once(once, [] {
      std::ignore = curl_global_init(CURL_GLOBAL_DEFAULT);
      std::atexit([] { curl_global_cleanup(); });
    });
  }

}  // namespace

/**
 * @brief 単一の HTTP リクエストを libcurl で実行します。
 * @param request 実行する HTTP リクエストです。
 * @return 成功時は HttpResponse、失敗時は GoogleSheetsError です。
 */
auto perform_http_request(detail::HttpRequest const& request) -> std::expected<detail::HttpResponse, GoogleSheetsError> {
  ensure_curl_initialized();

  auto* curl = curl_easy_init();
  if (curl == nullptr) {
    return std::unexpected{GoogleSheetsError{
        .kind    = GoogleSheetsErrorKind::network,
        .message = "failed to initialize curl",
    }};
  }

  auto  response    = detail::HttpResponse{};
  auto* header_list = static_cast<curl_slist*>(nullptr);

  // 呼び出し側が組み立てたヘッダー列を、そのまま libcurl の slist に移します。
  for (auto const& header : request.headers) {
    header_list = curl_slist_append(header_list, header.c_str());
  }

  auto constexpr CONNECT_TIMEOUT_SECONDS = 10L;
  auto constexpr TOTAL_TIMEOUT_SECONDS   = 30L;

  curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, CONNECT_TIMEOUT_SECONDS);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, TOTAL_TIMEOUT_SECONDS);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  if (request.method == "POST") {
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
  } else if (request.method != "GET") {
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request.method.c_str());
  }

  if (!request.body.empty()) {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(request.body.size()));
  }

  // 通信エラーと HTTP エラーステータスは分けて扱いたいため、
  // ここでは curl 自体の失敗だけを network として返します。
  auto const result = curl_easy_perform(curl);
  if (result != CURLE_OK) {
    auto const error = GoogleSheetsError{
        .kind    = GoogleSheetsErrorKind::network,
        .message = curl_easy_strerror(result),
    };
    curl_slist_free_all(header_list);
    curl_easy_cleanup(curl);
    return std::unexpected{error};
  }

  std::ignore = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);

  curl_slist_free_all(header_list);
  curl_easy_cleanup(curl);
  return response;
}

}  // namespace gsheetpp::http
