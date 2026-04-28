#include "gsheetpp/google_sheets_client.hpp"

#include "google_sheets_client_detail.hpp"
#include "http_client.hpp"

#include <glaze/glaze.hpp>
#include <jwt-cpp/traits/nlohmann-json/defaults.h>

#include <cctype>
#include <format>
#include <tuple>

namespace gsheetpp {

namespace {

auto constexpr ignore_unknown_keys_opts = glz::opts{
    .error_on_unknown_keys = false,
};

struct TokenResponsePayload {
  std::string access_token{};
  std::string token_type{};
  long expires_in{};
  std::optional<std::string> refresh_token{};
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
  if (auto const ec = glz::read<ignore_unknown_keys_opts>(value, json_buffer)) {
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

auto token_is_valid(
    std::shared_ptr<detail::ClientSharedState> const& shared_state,
    detail::ClockFunction const& clock) -> bool {
  auto const now = now_or_default(clock);
  return shared_state->token.has_value() &&
         (shared_state->expires_at - std::chrono::seconds{30} > now);
}

auto store_token(
    std::shared_ptr<detail::ClientSharedState> const& shared_state,
    TokenInfo const& token,
    std::chrono::system_clock::time_point issued_at) -> void {
  auto _ = std::scoped_lock{shared_state->mutex};
  shared_state->token = token;
  shared_state->expires_at = issued_at + std::chrono::seconds{token.expires_in_seconds};
}

template <class Fetch>
auto get_or_refresh_token(
    std::shared_ptr<detail::ClientSharedState> const& shared_state,
    detail::ClockFunction const& clock,
    Fetch&& fetch) -> std::expected<TokenInfo, GoogleSheetsError> {
  auto lock = std::unique_lock{shared_state->mutex};
  while (shared_state->refresh_in_progress) {
    shared_state->cv.wait(lock);
  }

  if (token_is_valid(shared_state, clock)) {
    return *shared_state->token;
  }

  shared_state->refresh_in_progress = true;
  lock.unlock();

  auto guard = detail::RefreshInProgressGuard{
      shared_state->mutex,
      shared_state->cv,
      shared_state->refresh_in_progress,
  };
  return fetch();
}

template <class Fetch>
auto force_refresh(
    std::shared_ptr<detail::ClientSharedState> const& shared_state,
    detail::ClockFunction const& clock,
    std::string_view failed_access_token,
    Fetch&& fetch) -> std::expected<TokenInfo, GoogleSheetsError> {
  {
    auto lock = std::unique_lock{shared_state->mutex};
    while (shared_state->refresh_in_progress) {
      shared_state->cv.wait(lock);
    }
    if (token_is_valid(shared_state, clock) && shared_state->token->access_token != failed_access_token) {
      return *shared_state->token;
    }
    shared_state->refresh_in_progress = true;
  }

  auto guard = detail::RefreshInProgressGuard{
      shared_state->mutex,
      shared_state->cv,
      shared_state->refresh_in_progress,
  };
  return fetch();
}

auto perform_request(
    detail::TransportFunction const& transport,
    detail::HttpRequest const& request) -> std::expected<detail::HttpResponse, GoogleSheetsError> {
  if (!transport) {
    return std::unexpected{GoogleSheetsError{
        .kind = GoogleSheetsErrorKind::network,
        .message = "transport is not configured",
    }};
  }
  return transport(request);
}

auto fetch_service_account_token(
    ServiceAccountConfig const& auth,
    std::shared_ptr<detail::ClientSharedState> const& shared_state,
    detail::TransportFunction const& transport,
    detail::ClockFunction const& clock) -> std::expected<TokenInfo, GoogleSheetsError> {
  auto const issued_at = now_or_default(clock);
  auto assertion = detail::build_jwt_assertion(auth, issued_at);
  if (!assertion) {
    return std::unexpected{assertion.error()};
  }

  auto response = perform_request(
      transport,
      detail::HttpRequest{
          .method = "POST",
          .url = auth.token_uri,
          .headers = {"Content-Type: application/x-www-form-urlencoded"},
          .body = detail::build_token_request_body(assertion.value()),
      });
  if (!response) {
    return std::unexpected{response.error()};
  }
  if (response->status_code >= 400) {
    auto auth_error = detail::parse_token_error_response(response->body, response->status_code);
    return std::unexpected{auth_error.error()};
  }

  auto token = detail::parse_token_response(response->body);
  if (!token) {
    return std::unexpected{token.error()};
  }

  store_token(shared_state, *token, issued_at);
  return token;
}

auto validate_user_oauth_config(UserOAuth2Auth const& auth)
    -> std::expected<void, GoogleSheetsError> {
  if (auth.client_id.empty()) {
    return std::unexpected{make_configuration_error("client_id is required")};
  }
  if (auth.client_secret.empty()) {
    return std::unexpected{make_configuration_error("client_secret is required")};
  }
  if (auth.token_uri.empty()) {
    return std::unexpected{make_configuration_error("token_uri is required")};
  }
  if (auth.current_refresh_token().empty() && auth.authorization_code.empty()) {
    return std::unexpected{GoogleSheetsError{
        .kind = GoogleSheetsErrorKind::authentication,
        .message = "user OAuth2 requires a refresh token or authorization code",
    }};
  }
  if (auth.current_refresh_token().empty() && auth.redirect_uri.empty()) {
    return std::unexpected{make_configuration_error("redirect_uri is required for authorization_code exchange")};
  }
  return {};
}

auto fetch_user_oauth_token(
    UserOAuth2Auth& auth,
    std::shared_ptr<detail::ClientSharedState> const& shared_state,
    detail::TransportFunction const& transport,
    detail::ClockFunction const& clock,
    bool force_refresh_only) -> std::expected<TokenInfo, GoogleSheetsError> {
  auto const validated = validate_user_oauth_config(auth);
  if (!validated) {
    return std::unexpected{validated.error()};
  }
  auto used_authorization_code = false;
  auto const auth_refresh_token = auth.current_refresh_token();
  auto refresh_token_snapshot = std::string{};
  {
    auto _ = std::scoped_lock{shared_state->mutex};
    if (shared_state->refresh_token.has_value()) {
      refresh_token_snapshot = *shared_state->refresh_token;
    }
    else if (!auth_refresh_token.empty()) {
      shared_state->refresh_token = auth_refresh_token;
      refresh_token_snapshot = auth_refresh_token;
    }
  }
  if (!refresh_token_snapshot.empty() && refresh_token_snapshot != auth_refresh_token) {
    auth.set_refresh_token(refresh_token_snapshot);
  }

  auto request_body = std::string{};
  if (!refresh_token_snapshot.empty()) {
    request_body = detail::build_oauth_refresh_request_body(auth, refresh_token_snapshot);
  }
  else if (!force_refresh_only && !auth.authorization_code.empty()) {
    used_authorization_code = true;
    request_body = detail::build_oauth_authorization_code_request_body(auth);
  }
  else {
    return std::unexpected{GoogleSheetsError{
        .kind = GoogleSheetsErrorKind::authentication,
        .message = "user OAuth2 requires a refresh token or authorization code",
    }};
  }

  auto response = perform_request(
      transport,
      detail::HttpRequest{
          .method = "POST",
          .url = auth.token_uri,
          .headers = {"Content-Type: application/x-www-form-urlencoded"},
          .body = std::move(request_body),
      });
  if (!response) {
    return std::unexpected{response.error()};
  }
  if (response->status_code >= 400) {
    auto auth_error = detail::parse_token_error_response(response->body, response->status_code);
    return std::unexpected{auth_error.error()};
  }

  auto oauth_response = detail::parse_oauth_token_response(response->body);
  if (!oauth_response) {
    return std::unexpected{oauth_response.error()};
  }

  auto latest_refresh_token = std::optional<std::string>{};
  if (oauth_response->refresh_token.has_value()) {
    auto _ = std::scoped_lock{shared_state->mutex};
    shared_state->refresh_token = *oauth_response->refresh_token;
    latest_refresh_token = oauth_response->refresh_token;
  }
  if (latest_refresh_token.has_value()) {
    auth.set_refresh_token(*latest_refresh_token);
  }
  if (used_authorization_code) {
    auth.authorization_code.clear();
  }

  auto const issued_at = now_or_default(clock);
  store_token(shared_state, oauth_response->token, issued_at);
  return oauth_response->token;
}

}  // namespace

auto parse_service_account_config(std::string_view json)
    -> std::expected<ServiceAccountConfig, GoogleSheetsError> {
  auto config = parse_json<ServiceAccountConfig>(json, "failed to parse service account JSON");
  if (!config) {
    return std::unexpected{config.error()};
  }

  if (config->client_email.empty()) {
    return std::unexpected{make_configuration_error("client_email is required")};
  }
  if (config->private_key.empty()) {
    return std::unexpected{make_configuration_error("private_key is required")};
  }
  if (config->token_uri.empty()) {
    return std::unexpected{make_configuration_error("token_uri is required")};
  }

  return std::move(config.value());
}

namespace detail {

auto default_transport() -> TransportFunction {
  return [](HttpRequest const& request) {
    return http::perform_http_request(request);
  };
}

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

auto parse_oauth_token_response(std::string_view json)
    -> std::expected<OAuthTokenResponse, GoogleSheetsError> {
  auto payload = parse_json<TokenResponsePayload>(json, "failed to parse OAuth token response");
  if (!payload) {
    return std::unexpected{payload.error()};
  }

  return OAuthTokenResponse{
      .token = TokenInfo{
          .access_token = std::move(payload->access_token),
          .token_type = std::move(payload->token_type),
          .expires_in_seconds = payload->expires_in,
      },
      .refresh_token = std::move(payload->refresh_token),
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

auto parse_api_error_response(std::string_view json)
    -> std::expected<std::string, GoogleSheetsError> {
  auto payload = parse_json<GoogleApiErrorPayload>(json, "failed to parse API error response");
  if (!payload) {
    return std::unexpected{payload.error()};
  }

  return std::move(payload->error.message);
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
  return std::format(
      "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion={}",
      percent_encode(assertion));
}

auto build_oauth_authorization_code_request_body(UserOAuth2Auth const& auth) -> std::string {
  return std::format(
      "client_id={}&client_secret={}&redirect_uri={}&code={}&grant_type=authorization_code",
      percent_encode(auth.client_id),
      percent_encode(auth.client_secret),
      percent_encode(auth.redirect_uri),
      percent_encode(auth.authorization_code));
}

auto build_oauth_refresh_request_body(UserOAuth2Auth const& auth, std::string_view refresh_token)
    -> std::string {
  return std::format(
      "client_id={}&client_secret={}&refresh_token={}&grant_type=refresh_token",
      percent_encode(auth.client_id),
      percent_encode(auth.client_secret),
      percent_encode(refresh_token));
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

auto build_values_url(
    std::string_view spreadsheet_id,
    std::string_view range,
    std::string_view query) -> std::string {
  auto url = std::format(
      "https://sheets.googleapis.com/v4/spreadsheets/{}/values/{}",
      percent_encode(spreadsheet_id),
      percent_encode(range));
  if (!query.empty()) {
    url += '?';
    url += query;
  }
  return url;
}

auto append_query_parameter(std::string url, std::string_view key, std::string_view value)
    -> std::string {
  url += (url.find('?') == std::string::npos) ? '?' : '&';
  url += std::format("{}={}", percent_encode(key), percent_encode(value));
  return url;
}

auto get_valid_token(
    ServiceAccountConfig const& auth,
    std::shared_ptr<ClientSharedState> const& shared_state,
    TransportFunction const& transport,
    ClockFunction const& clock) -> std::expected<TokenInfo, GoogleSheetsError> {
  return get_or_refresh_token(shared_state, clock, [&] {
    return fetch_service_account_token(auth, shared_state, transport, clock);
  });
}

auto get_valid_token(
    UserOAuth2Auth& auth,
    std::shared_ptr<ClientSharedState> const& shared_state,
    TransportFunction const& transport,
    ClockFunction const& clock) -> std::expected<TokenInfo, GoogleSheetsError> {
  return get_or_refresh_token(shared_state, clock, [&] {
    return fetch_user_oauth_token(auth, shared_state, transport, clock, false);
  });
}

auto force_refresh_token(
    UserOAuth2Auth& auth,
    std::shared_ptr<ClientSharedState> const& shared_state,
    TransportFunction const& transport,
    ClockFunction const& clock,
    std::string_view failed_access_token) -> std::expected<TokenInfo, GoogleSheetsError> {
  return force_refresh(shared_state, clock, failed_access_token, [&] {
    return fetch_user_oauth_token(auth, shared_state, transport, clock, true);
  });
}

auto make_google_sheets_client_for_test(
    ServiceAccountConfig config,
    TransportFunction transport,
    ClockFunction clock) -> GoogleSheetsClient {
  return GoogleSheetsClient{
      std::move(config),
      std::move(transport),
      std::move(clock),
  };
}

}  // namespace detail

}  // namespace gsheetpp
