#include "http_client.hpp"

#include <curl/curl.h>

#include <mutex>
#include <tuple>

/**
 * @file http_client.cpp
 * @brief libcurl を使って detail::HttpRequest を送信する実装です。
 *
 * 設計ノート:
 *   - libcurl の curl_easy_* 関数群はハンドル単位で非スレッドセーフです。
 *     そのため本実装ではリクエストごとに新しい easy handle を確保し、
 *     使い終わったら必ず cleanup します。これにより複数スレッドから同時に
 *     transport を呼び出しても race を起こしません。
 *   - ボディは CURLOPT_COPYPOSTFIELDS を使って libcurl 側に複製させています。
 *     これによりリクエスト送信が完了するまで呼び出し側の body 文字列を
 *     生存させておく必要がなく、401 retry 時のように元の body 文字列を
 *     別用途に move してしまっても安全に再送できます。
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

  /**
   * @brief 1 回のリクエストで必要となる libcurl リソースを RAII で管理します。
   *
   * curl_easy_init の戻り値と curl_slist を対にして保持し、デストラクタで
   * 漏れなく解放します。例外安全のために用意しています。
   */
  class EasyRequestHandle {
public:
    EasyRequestHandle() = default;

    EasyRequestHandle(EasyRequestHandle const&)                    = delete;
    auto operator=(EasyRequestHandle const&) -> EasyRequestHandle& = delete;
    EasyRequestHandle(EasyRequestHandle&&)                         = delete;
    auto operator=(EasyRequestHandle&&) -> EasyRequestHandle&      = delete;

    ~EasyRequestHandle() {
      if (header_list_ != nullptr) {
        curl_slist_free_all(header_list_);
      }
      if (curl_ != nullptr) {
        curl_easy_cleanup(curl_);
      }
    }

    auto init() -> bool {
      curl_ = curl_easy_init();
      return curl_ != nullptr;
    }

    auto curl() -> CURL* { return curl_; }
    auto append_header(char const* header_line) -> void {
      header_list_ = curl_slist_append(header_list_, header_line);
    }
    auto headers() -> curl_slist* { return header_list_; }

private:
    CURL*      curl_{nullptr};
    curl_slist* header_list_{nullptr};
  };

}  // namespace

/**
 * @brief 単一の HTTP リクエストを libcurl で実行します。
 * @param request 実行する HTTP リクエストです。
 * @return 成功時は HttpResponse、失敗時は GoogleSheetsError です。
 */
auto perform_http_request(detail::HttpRequest const& request) -> std::expected<detail::HttpResponse, GoogleSheetsError> {
  ensure_curl_initialized();

  auto handle = EasyRequestHandle{};
  if (!handle.init()) {
    return std::unexpected{GoogleSheetsError{
        .kind    = GoogleSheetsErrorKind::network,
        .message = "failed to initialize curl",
    }};
  }
  auto* curl = handle.curl();

  auto response = detail::HttpResponse{};

  // 呼び出し側が組み立てたヘッダー列を、そのまま libcurl の slist に移します。
  for (auto const& header : request.headers) {
    handle.append_header(header.c_str());
  }

  auto constexpr CONNECT_TIMEOUT_SECONDS = 10L;
  auto constexpr TOTAL_TIMEOUT_SECONDS   = 30L;

  curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, CONNECT_TIMEOUT_SECONDS);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, TOTAL_TIMEOUT_SECONDS);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, handle.headers());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  if (request.method == "POST") {
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
  } else if (request.method != "GET") {
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request.method.c_str());
  }

  if (!request.body.empty()) {
    // COPYPOSTFIELDS を使うことで、libcurl が必要なサイズを確定する前に
    // 呼び出し側 body 文字列が破棄されても安全です。
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
    curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, request.body.size());
  }

  // 通信エラーと HTTP エラーステータスは分けて扱いたいため、
  // ここでは curl 自体の失敗だけを network として返します。
  auto const result = curl_easy_perform(curl);
  if (result != CURLE_OK) {
    auto const error = GoogleSheetsError{
        .kind    = GoogleSheetsErrorKind::network,
        .message = curl_easy_strerror(result),
    };
    return std::unexpected{error};
  }

  std::ignore = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
  return response;
}

}  // namespace gsheetpp::http
