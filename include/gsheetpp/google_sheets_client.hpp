#pragma once

#include <chrono>
#include <concepts>
#include <condition_variable>
#include <expected>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace gsheetpp {

enum class GoogleSheetsErrorKind {
  configuration,
  serialization,
  jwt_signing,
  network,
  http,
  authentication,
  api_response,
};

struct GoogleSheetsError {
  GoogleSheetsErrorKind kind{GoogleSheetsErrorKind::configuration};
  std::string message{};
  std::optional<long> http_status{};
  std::string response_body{};
};

struct ApiKeyAuth {
  std::string api_key{};
};

struct ServiceAccountConfig {
  std::string client_email{};
  std::string private_key{};
  std::string token_uri{};
  std::string project_id{};
};

using ServiceAccountAuth = ServiceAccountConfig;

struct UserOAuth2Auth {
  std::string client_id{};
  std::string client_secret{};
  std::string token_uri{"https://oauth2.googleapis.com/token"};
  std::string redirect_uri{};
  std::vector<std::string> scopes{};
  std::string authorization_code{};
  std::string refresh_token{};
  mutable std::shared_ptr<std::mutex> refresh_token_mutex{std::make_shared<std::mutex>()};

  auto current_refresh_token() const -> std::string {
    auto _ = std::scoped_lock{ensure_refresh_token_mutex()};
    return refresh_token;
  }

  auto set_refresh_token(std::string value) -> void {
    auto _ = std::scoped_lock{ensure_refresh_token_mutex()};
    refresh_token = std::move(value);
  }

 private:
  auto ensure_refresh_token_mutex() const -> std::mutex& {
    if (!refresh_token_mutex) {
      refresh_token_mutex = std::make_shared<std::mutex>();
    }
    return *refresh_token_mutex;
  }
};

struct TokenInfo {
  std::string access_token{};
  std::string token_type{};
  long expires_in_seconds{};
};

struct ReadValuesResult {
  std::string range{};
  std::string major_dimension{};
  std::vector<std::vector<std::string>> values{};
};

struct WriteValuesResult {
  std::string spreadsheet_id{};
  std::string updated_range{};
  long updated_rows{};
  long updated_columns{};
  long updated_cells{};
};

namespace detail {

struct HttpRequest {
  std::string method{};
  std::string url{};
  std::vector<std::string> headers{};
  std::string body{};
};

struct HttpResponse {
  long status_code{};
  std::string body{};
};

using TransportFunction =
    std::function<std::expected<HttpResponse, GoogleSheetsError>(HttpRequest const&)>;

using ClockFunction = std::function<std::chrono::system_clock::time_point()>;

struct ClientSharedState {
  std::mutex mutex{};
  std::condition_variable cv{};
  bool refresh_in_progress{};
  std::optional<TokenInfo> token{};
  std::optional<std::string> refresh_token{};
  std::chrono::system_clock::time_point expires_at{};
};

}  // namespace detail

template <class T>
concept Authenticator = std::same_as<std::remove_cvref_t<T>, ApiKeyAuth> ||
                        std::same_as<std::remove_cvref_t<T>, ServiceAccountConfig> ||
                        std::same_as<std::remove_cvref_t<T>, UserOAuth2Auth>;

template <Authenticator Auth>
class BasicGoogleSheetsClient;

template <Authenticator Auth>
class BasicGoogleSheetsClient {
 public:
  explicit BasicGoogleSheetsClient(Auth auth);
  BasicGoogleSheetsClient(
      Auth auth,
      detail::TransportFunction transport,
      detail::ClockFunction clock = {});

  template <Authenticator OtherAuth>
  auto set_authenticator(OtherAuth auth) const -> BasicGoogleSheetsClient<OtherAuth>;

  auto authenticate_async() -> std::future<std::expected<TokenInfo, GoogleSheetsError>>;
  auto read_values_async(std::string_view spreadsheet_id, std::string_view range)
      -> std::future<std::expected<ReadValuesResult, GoogleSheetsError>>;
  auto write_values_async(
      std::string_view spreadsheet_id,
      std::string_view range,
      std::span<std::vector<std::string> const> data)
      -> std::future<std::expected<WriteValuesResult, GoogleSheetsError>>;

  auto authenticator() -> Auth& {
    return *auth_;
  }

  auto authenticator() const -> Auth const& {
    return *auth_;
  }

 private:
  auto prepare_request(detail::HttpRequest& request, bool is_write)
      -> std::expected<void, GoogleSheetsError>;
  auto execute_request(detail::HttpRequest request, bool retry_on_unauthorized)
      -> std::expected<detail::HttpResponse, GoogleSheetsError>;

  std::shared_ptr<Auth> auth_{};
  detail::TransportFunction transport_{};
  detail::ClockFunction clock_{};
  std::shared_ptr<detail::ClientSharedState> shared_state_{};
};

using GoogleSheetsClient = BasicGoogleSheetsClient<ServiceAccountConfig>;

template <class Auth>
BasicGoogleSheetsClient(Auth) -> BasicGoogleSheetsClient<Auth>;

template <class Auth, class Transport>
BasicGoogleSheetsClient(Auth, Transport&&, detail::ClockFunction = {})
    -> BasicGoogleSheetsClient<Auth>;

auto parse_service_account_config(std::string_view json)
    -> std::expected<ServiceAccountConfig, GoogleSheetsError>;

}  // namespace gsheetpp

#include "gsheetpp/google_sheets_client_impl.hpp"
