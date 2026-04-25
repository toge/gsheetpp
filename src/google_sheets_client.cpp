#include "gsheetpp/google_sheets_client.hpp"
#include "google_sheets_client_detail.hpp"
#include "http_client.hpp"

#include <glaze/glaze.hpp>
#include <jwt-cpp/traits/nlohmann-json/defaults.h>

#include <cctype>
#include <mutex>
#include <tuple>
#include <utility>

namespace gsheetpp {

/**
 * @brief クライアント内で共有される状態管理構造体
 */
struct GoogleSheetsClient::SharedState {
  std::mutex mutex{};
  std::condition_variable cv{};
  bool refresh_in_progress{};            ///< トークン更新が進行中かどうか
  std::optional<TokenInfo> token{};       ///< キャッシュされたトークン
  std::chrono::system_clock::time_point expires_at{}; ///< トークンの有効期限
};

namespace {

struct TokenResponsePayload {
  std::string access_token{};
  std::string token_type{};
  long expires_in{};
};

struct ReadValuesPayload {
  std::string range{};
  std::string majorDimension{};
  std::vector<std::vector<std::string>> values{};
};

struct WriteValuesPayload {
  std::string spreadsheetId{};
  std::string updatedRange{};
  long updatedRows{};
  long updatedColumns{};
  long updatedCells{};
};

struct GoogleApiErrorStatus {
  long code{};
  std::string message{};
  std::string status{};
};

struct GoogleApiErrorPayload {
  GoogleApiErrorStatus error{};
};

struct WriteValuesRequestPayload {
  std::string majorDimension{"ROWS"};
  std::vector<std::vector<std::string>> values{};
};

auto make_parse_error(std::string_view message) -> GoogleSheetsError {
  return GoogleSheetsError{
      .kind = GoogleSheetsErrorKind::serialization,
      .message = std::string{message},
  };
}

auto make_configuration_error(std::string_view message) -> GoogleSheetsError {
  return GoogleSheetsError{
      .kind = GoogleSheetsErrorKind::configuration,
      .message = std::string{message},
  };
}

auto make_http_error(
    GoogleSheetsErrorKind kind,
    std::string_view message,
    long http_status,
    std::string_view response_body) -> GoogleSheetsError {
  return GoogleSheetsError{
      .kind = kind,
      .message = std::string{message},
      .http_status = http_status,
      .response_body = std::string{response_body},
  };
}

template <class T>
auto parse_json(std::string_view json, std::string_view message)
    -> std::expected<T, GoogleSheetsError> {
  auto value = T{};
  auto const json_buffer = std::string{json};
  if (auto const ec = glz::read_json(value, json_buffer)) {
    return std::unexpected{make_parse_error(message)};
  }
  return value;
}

auto percent_encode(std::string_view input) -> std::string {
  auto encoded = std::string{};
  auto constexpr hex = "0123456789ABCDEF";
  encoded.reserve(input.size() * 3);

  for (auto const ch : input) {
    auto const byte = static_cast<unsigned char>(ch);
    if (std::isalnum(byte) != 0 || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
      encoded.push_back(static_cast<char>(ch));
      continue;
    }

    encoded.push_back('%');
    encoded.push_back(hex[(byte >> 4U) & 0x0FU]);
    encoded.push_back(hex[byte & 0x0FU]);
  }

  return encoded;
}

auto now_or_default(detail::ClockFunction const& clock)
    -> std::chrono::system_clock::time_point {
  if (clock) {
    return clock();
  }
  return std::chrono::system_clock::now();
}

auto build_values_url(
    std::string_view spreadsheet_id,
    std::string_view range,
    std::string_view query = {}) -> std::string {
  auto url = std::string{
      "https://sheets.googleapis.com/v4/spreadsheets/" + percent_encode(spreadsheet_id) +
      "/values/" + percent_encode(range)};
  if (!query.empty()) {
    url += '?';
    url += query;
  }
  return url;
}

auto perform_authenticated_request(
    detail::TransportFunction const& transport,
    detail::HttpRequest request) -> std::expected<detail::HttpResponse, GoogleSheetsError> {
  if (!transport) {
    return std::unexpected{GoogleSheetsError{
        .kind = GoogleSheetsErrorKind::network,
        .message = "transport is not configured",
    }};
  }
  return transport(request);
}

auto handle_sheet_error(std::string_view body, long status)
    -> std::unexpected<GoogleSheetsError> {
  auto api_error = detail::parse_api_error_response(body, status);
  if (!api_error.has_value()) {
    return std::unexpected{api_error.error()};
  }
  return std::unexpected{make_http_error(
      GoogleSheetsErrorKind::http,
      "google sheets request failed",
      status,
      body)};
}

}  // namespace

GoogleSheetsClient::GoogleSheetsClient(ServiceAccountConfig config)
    : GoogleSheetsClient(
          std::move(config),
          [](detail::HttpRequest const& request) {
            return http::perform_http_request(request);
          },
          {}) {}

GoogleSheetsClient::GoogleSheetsClient(
    ServiceAccountConfig config,
    detail::TransportFunction transport,
    detail::ClockFunction clock)
    : config_(std::move(config)),
      transport_(std::move(transport)),
      clock_(std::move(clock)),
      shared_state_(std::make_shared<SharedState>()) {}

auto parse_service_account_config(std::string_view json)
    -> std::expected<ServiceAccountConfig, GoogleSheetsError> {
  auto config = ServiceAccountConfig{};
  auto const json_buffer = std::string{json};
  if (auto const ec = glz::read_json(config, json_buffer)) {
    return std::unexpected{make_parse_error("failed to parse service account JSON")};
  }

  if (config.client_email.empty()) {
    return std::unexpected{make_configuration_error("client_email is required")};
  }
  if (config.private_key.empty()) {
    return std::unexpected{make_configuration_error("private_key is required")};
  }
  if (config.token_uri.empty()) {
    return std::unexpected{make_configuration_error("token_uri is required")};
  }

  return config;
}

auto GoogleSheetsClient::authenticate_async()
    -> std::future<std::expected<TokenInfo, GoogleSheetsError>> {
  return std::async(std::launch::async, [this] { return refresh_token(); });
}

auto GoogleSheetsClient::read_values_async(std::string_view spreadsheet_id, std::string_view range)
    -> std::future<std::expected<ReadValuesResult, GoogleSheetsError>> {
  auto const spreadsheet = std::string{spreadsheet_id};
  auto const read_range = std::string{range};
  return std::async(std::launch::async, [this, spreadsheet, read_range]
                                               -> std::expected<ReadValuesResult, GoogleSheetsError> {
    auto token = get_valid_token();
    if (!token) {
      return std::unexpected{token.error()};
    }

    auto response = perform_authenticated_request(
        transport_,
        detail::HttpRequest{
            .method = "GET",
            .url = build_values_url(spreadsheet, read_range),
            .headers = {"Authorization: Bearer " + token->access_token},
        });
    if (!response) {
      return std::unexpected{response.error()};
    }
    if (response->status_code >= 400) {
      return std::unexpected{handle_sheet_error(response->body, response->status_code).error()};
    }

    return detail::parse_read_values_response(response->body);
  });
}

auto GoogleSheetsClient::write_values_async(
    std::string_view spreadsheet_id,
    std::string_view range,
    std::span<std::vector<std::string> const> data)
    -> std::future<std::expected<WriteValuesResult, GoogleSheetsError>> {
  auto const spreadsheet = std::string{spreadsheet_id};
  auto const write_range = std::string{range};
  auto const values = std::vector<std::vector<std::string>>{data.begin(), data.end()};
  return std::async(
      std::launch::async,
      [this, spreadsheet, write_range, values]
          -> std::expected<WriteValuesResult, GoogleSheetsError> {
    auto token = get_valid_token();
    if (!token) {
      return std::unexpected{token.error()};
    }

    auto response = perform_authenticated_request(
        transport_,
        detail::HttpRequest{
            .method = "PUT",
            .url = build_values_url(spreadsheet, write_range, "valueInputOption=RAW"),
            .headers = {
                "Authorization: Bearer " + token->access_token,
                "Content-Type: application/json",
            },
            .body = detail::build_write_values_request_body(values),
        });
    if (!response) {
      return std::unexpected{response.error()};
    }
    if (response->status_code >= 400) {
      return std::unexpected{handle_sheet_error(response->body, response->status_code).error()};
    }

    return detail::parse_write_values_response(response->body);
      });
}

/**
 * @brief トークンを強制的に更新します。
 * 複数のスレッドから同時に呼び出された場合、一つのスレッドのみが更新を行い、他は完了を待ちます。
 */
auto GoogleSheetsClient::refresh_token() -> std::expected<TokenInfo, GoogleSheetsError> {
  {
    auto lock = std::unique_lock{shared_state_->mutex};
    while (shared_state_->refresh_in_progress) {
      shared_state_->cv.wait(lock);
    }
    shared_state_->refresh_in_progress = true;
  }

  auto const issued_at = now_or_default(clock_);
  auto assertion = detail::build_jwt_assertion(config_, issued_at);
  if (!assertion) {
    auto lock = std::scoped_lock{shared_state_->mutex};
    shared_state_->refresh_in_progress = false;
    shared_state_->cv.notify_all();
    return std::unexpected{assertion.error()};
  }

  auto response = perform_authenticated_request(
      transport_,
      detail::HttpRequest{
          .method = "POST",
          .url = config_.token_uri,
          .headers = {"Content-Type: application/x-www-form-urlencoded"},
          .body = detail::build_token_request_body(assertion.value()),
      });
  if (!response) {
    auto lock = std::scoped_lock{shared_state_->mutex};
    shared_state_->refresh_in_progress = false;
    shared_state_->cv.notify_all();
    return std::unexpected{response.error()};
  }
  if (response->status_code >= 400) {
    auto auth_error = detail::parse_token_error_response(response->body, response->status_code);
    auto lock = std::scoped_lock{shared_state_->mutex};
    shared_state_->refresh_in_progress = false;
    shared_state_->cv.notify_all();
    return std::unexpected{auth_error.error()};
  }

  auto token = detail::parse_token_response(response->body);
  auto lock = std::scoped_lock{shared_state_->mutex};
  shared_state_->refresh_in_progress = false;
  if (!token) {
    shared_state_->cv.notify_all();
    return std::unexpected{token.error()};
  }

  shared_state_->token = token.value();
  shared_state_->expires_at = issued_at + std::chrono::seconds{token->expires_in_seconds};
  shared_state_->cv.notify_all();
  return token;
}

/**
 * @brief 有効なトークンを取得します。
 * キャッシュされたトークンが有効であればそれを返し、期限切れが近ければ更新を行います。
 */
auto GoogleSheetsClient::get_valid_token() -> std::expected<TokenInfo, GoogleSheetsError> {
  auto lock = std::unique_lock{shared_state_->mutex};
  while (shared_state_->refresh_in_progress) {
    shared_state_->cv.wait(lock);
  }

  auto now = now_or_default(clock_);
  auto token_is_valid = shared_state_->token.has_value() &&
                        (shared_state_->expires_at - std::chrono::seconds{30} > now);
  if (token_is_valid) {
    return *shared_state_->token;
  }

  shared_state_->refresh_in_progress = true;
  lock.unlock();

  auto const issued_at = now_or_default(clock_);
  auto assertion = detail::build_jwt_assertion(config_, issued_at);
  if (!assertion) {
    auto failure_lock = std::scoped_lock{shared_state_->mutex};
    shared_state_->refresh_in_progress = false;
    shared_state_->cv.notify_all();
    return std::unexpected{assertion.error()};
  }

  auto response = perform_authenticated_request(
      transport_,
      detail::HttpRequest{
          .method = "POST",
          .url = config_.token_uri,
          .headers = {"Content-Type: application/x-www-form-urlencoded"},
          .body = detail::build_token_request_body(assertion.value()),
      });
  if (!response) {
    auto failure_lock = std::scoped_lock{shared_state_->mutex};
    shared_state_->refresh_in_progress = false;
    shared_state_->cv.notify_all();
    return std::unexpected{response.error()};
  }
  if (response->status_code >= 400) {
    auto auth_error = detail::parse_token_error_response(response->body, response->status_code);
    auto failure_lock = std::scoped_lock{shared_state_->mutex};
    shared_state_->refresh_in_progress = false;
    shared_state_->cv.notify_all();
    return std::unexpected{auth_error.error()};
  }

  auto token = detail::parse_token_response(response->body);
  auto success_lock = std::scoped_lock{shared_state_->mutex};
  shared_state_->refresh_in_progress = false;
  if (!token) {
    shared_state_->cv.notify_all();
    return std::unexpected{token.error()};
  }

  shared_state_->token = token.value();
  shared_state_->expires_at = issued_at + std::chrono::seconds{token->expires_in_seconds};
  shared_state_->cv.notify_all();
  return token;
}

namespace detail {

auto parse_token_response(std::string_view json)
    -> std::expected<TokenInfo, GoogleSheetsError> {
  auto payload = parse_json<TokenResponsePayload>(json, "failed to parse token response");
  if (!payload) {
    return std::unexpected{payload.error()};
  }

  return TokenInfo{
      .access_token = std::move(payload->access_token),
      .token_type = std::move(payload->token_type),
      .expires_in_seconds = payload->expires_in,
  };
}

auto parse_token_error_response(std::string_view json, long http_status)
    -> std::expected<void, GoogleSheetsError> {
  return std::unexpected{make_http_error(
      GoogleSheetsErrorKind::authentication,
      "token request failed",
      http_status,
      json)};
}

auto parse_read_values_response(std::string_view json)
    -> std::expected<ReadValuesResult, GoogleSheetsError> {
  auto payload = parse_json<ReadValuesPayload>(json, "failed to parse read values response");
  if (!payload) {
    return std::unexpected{payload.error()};
  }

  return ReadValuesResult{
      .range = std::move(payload->range),
      .major_dimension = std::move(payload->majorDimension),
      .values = std::move(payload->values),
  };
}

auto parse_write_values_response(std::string_view json)
    -> std::expected<WriteValuesResult, GoogleSheetsError> {
  auto payload = parse_json<WriteValuesPayload>(json, "failed to parse write values response");
  if (!payload) {
    return std::unexpected{payload.error()};
  }

  return WriteValuesResult{
      .spreadsheet_id = std::move(payload->spreadsheetId),
      .updated_range = std::move(payload->updatedRange),
      .updated_rows = payload->updatedRows,
      .updated_columns = payload->updatedColumns,
      .updated_cells = payload->updatedCells,
  };
}

auto parse_api_error_response(std::string_view json, long http_status)
    -> std::expected<void, GoogleSheetsError> {
  auto payload = parse_json<GoogleApiErrorPayload>(json, "failed to parse API error response");
  if (!payload) {
    return std::unexpected{payload.error()};
  }

  return std::unexpected{make_http_error(
      GoogleSheetsErrorKind::api_response,
      payload->error.message.empty() ? "google api request failed" : payload->error.message,
      http_status,
      json)};
}

auto build_jwt_assertion(
    ServiceAccountConfig const& config,
    std::chrono::system_clock::time_point issued_at)
    -> std::expected<std::string, GoogleSheetsError> {
  try {
    auto const token = jwt::create()
                           .set_type("JWT")
                           .set_issuer(config.client_email)
                           .set_audience(config.token_uri)
                           .set_issued_at(issued_at)
                           .set_expires_at(issued_at + std::chrono::hours{1})
                           .set_payload_claim(
                               "scope",
                               jwt::claim(std::string{
                                   "https://www.googleapis.com/auth/spreadsheets"}))
                           .sign(jwt::algorithm::rs256("", config.private_key));
    return token;
  }
  catch (std::exception const& ex) {
    return std::unexpected{GoogleSheetsError{
        .kind = GoogleSheetsErrorKind::jwt_signing,
        .message = ex.what(),
    }};
  }
}

auto build_token_request_body(std::string_view assertion) -> std::string {
  return "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion=" +
         percent_encode(assertion);
}

auto build_write_values_request_body(std::vector<std::vector<std::string>> const& values)
    -> std::string {
  auto payload = WriteValuesRequestPayload{
      .majorDimension = "ROWS",
      .values = values,
  };
  auto json = std::string{};
  std::ignore = glz::write_json(payload, json);
  return json;
}

auto make_google_sheets_client_for_test(
    ServiceAccountConfig config,
    TransportFunction transport,
    ClockFunction clock) -> GoogleSheetsClient {
  return GoogleSheetsClient{std::move(config), std::move(transport), std::move(clock)};
}

}  // namespace detail

}  // namespace gsheetpp
