#pragma once

namespace gsheetpp {

namespace detail {

struct OAuthTokenResponse {
  TokenInfo token{};
  std::optional<std::string> refresh_token{};
};

class RefreshInProgressGuard {
 public:
  RefreshInProgressGuard(
      std::mutex& mutex,
      std::condition_variable& cv,
      bool& refresh_in_progress) noexcept;

  RefreshInProgressGuard(RefreshInProgressGuard const&) = delete;
  auto operator=(RefreshInProgressGuard const&) -> RefreshInProgressGuard& = delete;
  RefreshInProgressGuard(RefreshInProgressGuard&&) = delete;
  auto operator=(RefreshInProgressGuard&&) -> RefreshInProgressGuard& = delete;

  ~RefreshInProgressGuard() noexcept;

 private:
  std::mutex& mutex_;
  std::condition_variable& cv_;
  bool& refresh_in_progress_;
};

auto default_transport() -> TransportFunction;
auto parse_token_response(std::string_view json)
    -> std::expected<TokenInfo, GoogleSheetsError>;
auto parse_oauth_token_response(std::string_view json)
    -> std::expected<OAuthTokenResponse, GoogleSheetsError>;
auto parse_token_error_response(std::string_view json, long http_status)
    -> std::expected<void, GoogleSheetsError>;
auto parse_read_values_response(std::string_view json)
    -> std::expected<ReadValuesResult, GoogleSheetsError>;
auto parse_write_values_response(std::string_view json)
    -> std::expected<WriteValuesResult, GoogleSheetsError>;
auto parse_api_error_response(std::string_view json)
    -> std::expected<std::string, GoogleSheetsError>;
auto build_jwt_assertion(
    ServiceAccountConfig const& config,
    std::chrono::system_clock::time_point issued_at)
    -> std::expected<std::string, GoogleSheetsError>;
auto build_token_request_body(std::string_view assertion) -> std::string;
auto build_oauth_authorization_code_request_body(UserOAuth2Auth const& auth) -> std::string;
auto build_oauth_refresh_request_body(UserOAuth2Auth const& auth, std::string_view refresh_token)
    -> std::string;
auto build_write_values_request_body(std::vector<std::vector<std::string>> const& values)
    -> std::string;
auto build_values_url(
    std::string_view spreadsheet_id,
    std::string_view range,
    std::string_view query = {}) -> std::string;
auto append_query_parameter(std::string url, std::string_view key, std::string_view value)
    -> std::string;
auto get_valid_token(
    ServiceAccountConfig const& auth,
    std::shared_ptr<ClientSharedState> const& shared_state,
    TransportFunction const& transport,
    ClockFunction const& clock) -> std::expected<TokenInfo, GoogleSheetsError>;
auto get_valid_token(
    UserOAuth2Auth& auth,
    std::shared_ptr<ClientSharedState> const& shared_state,
    TransportFunction const& transport,
    ClockFunction const& clock) -> std::expected<TokenInfo, GoogleSheetsError>;
auto force_refresh_token(
    UserOAuth2Auth& auth,
    std::shared_ptr<ClientSharedState> const& shared_state,
    TransportFunction const& transport,
    ClockFunction const& clock,
    std::string_view failed_access_token) -> std::expected<TokenInfo, GoogleSheetsError>;

}  // namespace detail

template <Authenticator Auth>
BasicGoogleSheetsClient<Auth>::BasicGoogleSheetsClient(Auth auth)
    : BasicGoogleSheetsClient(std::move(auth), detail::default_transport(), {}) {}

template <Authenticator Auth>
BasicGoogleSheetsClient<Auth>::BasicGoogleSheetsClient(
    Auth auth,
    detail::TransportFunction transport,
    detail::ClockFunction clock)
    : auth_{std::make_shared<Auth>(std::move(auth))},
      transport_{std::move(transport)},
      clock_{std::move(clock)},
      shared_state_{std::make_shared<detail::ClientSharedState>()} {}

template <Authenticator Auth>
template <Authenticator OtherAuth>
auto BasicGoogleSheetsClient<Auth>::set_authenticator(OtherAuth auth) const
    -> BasicGoogleSheetsClient<OtherAuth> {
  return BasicGoogleSheetsClient<OtherAuth>{
      std::move(auth),
      transport_,
      clock_,
  };
}

template <Authenticator Auth>
auto BasicGoogleSheetsClient<Auth>::authenticate_async()
    -> std::future<std::expected<TokenInfo, GoogleSheetsError>> {
  return std::async(std::launch::async, [client = *this]() mutable
                                           -> std::expected<TokenInfo, GoogleSheetsError> {
    if constexpr (std::same_as<Auth, ApiKeyAuth>) {
      return TokenInfo{
          .access_token = "",
          .token_type = "ApiKey",
          .expires_in_seconds = 0,
      };
    }
    else if constexpr (std::same_as<Auth, ServiceAccountConfig>) {
      return detail::get_valid_token(*client.auth_, client.shared_state_, client.transport_, client.clock_);
    }
    else {
      return detail::get_valid_token(*client.auth_, client.shared_state_, client.transport_, client.clock_);
    }
  });
}

template <Authenticator Auth>
auto BasicGoogleSheetsClient<Auth>::read_values_async(
    std::string_view spreadsheet_id,
    std::string_view range) -> std::future<std::expected<ReadValuesResult, GoogleSheetsError>> {
  auto const spreadsheet = std::string{spreadsheet_id};
  auto const read_range = std::string{range};
  return std::async(
      std::launch::async,
      [client = *this, spreadsheet, read_range]() mutable
          -> std::expected<ReadValuesResult, GoogleSheetsError> {
        auto request = detail::HttpRequest{
            .method = "GET",
            .url = detail::build_values_url(spreadsheet, read_range),
        };
        auto prepared = client.prepare_request(request, false);
        if (!prepared) {
          return std::unexpected{prepared.error()};
        }

        auto response = client.execute_request(std::move(request), true);
        if (!response) {
          return std::unexpected{response.error()};
        }
        if (response->status_code >= 400) {
          auto api_error = detail::parse_api_error_response(response->body);
          if (!api_error) {
            return std::unexpected{GoogleSheetsError{
                .kind = GoogleSheetsErrorKind::http,
                .message = "google sheets request failed",
                .http_status = response->status_code,
                .response_body = response->body,
            }};
          }
          return std::unexpected{GoogleSheetsError{
              .kind = GoogleSheetsErrorKind::api_response,
              .message = api_error->empty() ? "google api request failed" : *api_error,
              .http_status = response->status_code,
              .response_body = response->body,
          }};
        }

        return detail::parse_read_values_response(response->body);
      });
}

template <Authenticator Auth>
auto BasicGoogleSheetsClient<Auth>::write_values_async(
    std::string_view spreadsheet_id,
    std::string_view range,
    std::span<std::vector<std::string> const> data)
    -> std::future<std::expected<WriteValuesResult, GoogleSheetsError>> {
  auto const spreadsheet = std::string{spreadsheet_id};
  auto const write_range = std::string{range};
  auto const values = std::vector<std::vector<std::string>>{data.begin(), data.end()};
  return std::async(
      std::launch::async,
      [client = *this, spreadsheet, write_range, values]() mutable
          -> std::expected<WriteValuesResult, GoogleSheetsError> {
        auto request = detail::HttpRequest{
            .method = "PUT",
            .url = detail::build_values_url(spreadsheet, write_range, "valueInputOption=RAW"),
            .headers = {"Content-Type: application/json"},
            .body = detail::build_write_values_request_body(values),
        };
        auto prepared = client.prepare_request(request, true);
        if (!prepared) {
          return std::unexpected{prepared.error()};
        }

        auto response = client.execute_request(std::move(request), true);
        if (!response) {
          return std::unexpected{response.error()};
        }
        if (response->status_code >= 400) {
          auto api_error = detail::parse_api_error_response(response->body);
          if (!api_error) {
            return std::unexpected{GoogleSheetsError{
                .kind = GoogleSheetsErrorKind::http,
                .message = "google sheets request failed",
                .http_status = response->status_code,
                .response_body = response->body,
            }};
          }
          return std::unexpected{GoogleSheetsError{
              .kind = GoogleSheetsErrorKind::api_response,
              .message = api_error->empty() ? "google api request failed" : *api_error,
              .http_status = response->status_code,
              .response_body = response->body,
          }};
        }

        return detail::parse_write_values_response(response->body);
      });
}

template <Authenticator Auth>
auto BasicGoogleSheetsClient<Auth>::prepare_request(detail::HttpRequest& request, bool is_write)
    -> std::expected<void, GoogleSheetsError> {
  if constexpr (std::same_as<Auth, ApiKeyAuth>) {
    if (auth_->api_key.empty()) {
      return std::unexpected{GoogleSheetsError{
          .kind = GoogleSheetsErrorKind::configuration,
          .message = "api_key is required",
      }};
    }
    if (is_write) {
      return std::unexpected{GoogleSheetsError{
          .kind = GoogleSheetsErrorKind::configuration,
          .message = "api key authentication is read-only",
      }};
    }
    request.url = detail::append_query_parameter(std::move(request.url), "key", auth_->api_key);
    return {};
  }
  else if constexpr (std::same_as<Auth, ServiceAccountConfig>) {
    auto token = detail::get_valid_token(*auth_, shared_state_, transport_, clock_);
    if (!token) {
      return std::unexpected{token.error()};
    }
    request.headers.push_back("Authorization: Bearer " + token->access_token);
    return {};
  }
  else {
    auto token = detail::get_valid_token(*auth_, shared_state_, transport_, clock_);
    if (!token) {
      return std::unexpected{token.error()};
    }
    request.headers.push_back("Authorization: Bearer " + token->access_token);
    return {};
  }
}

template <Authenticator Auth>
auto BasicGoogleSheetsClient<Auth>::execute_request(
    detail::HttpRequest request,
    bool retry_on_unauthorized) -> std::expected<detail::HttpResponse, GoogleSheetsError> {
  auto send_request = [this](detail::HttpRequest const& current_request)
      -> std::expected<detail::HttpResponse, GoogleSheetsError> {
    if (!transport_) {
      return std::unexpected{GoogleSheetsError{
          .kind = GoogleSheetsErrorKind::network,
          .message = "transport is not configured",
      }};
    }
    return transport_(current_request);
  };

  auto response = send_request(request);
  if (!response) {
    return std::unexpected{response.error()};
  }

  if constexpr (std::same_as<Auth, UserOAuth2Auth>) {
    if (retry_on_unauthorized && response->status_code == 401) {
      auto failed_access_token = std::string_view{};
      for (auto const& header : request.headers) {
        if (header.starts_with("Authorization: Bearer ")) {
          failed_access_token =
              std::string_view{header}.substr(std::string_view{"Authorization: Bearer "}.size());
          break;
        }
      }

      auto refreshed =
          detail::force_refresh_token(*auth_, shared_state_, transport_, clock_, failed_access_token);
      if (!refreshed) {
        return std::unexpected{refreshed.error()};
      }

      auto retry_request = std::move(request);
      auto const replacement = std::string{"Authorization: Bearer " + refreshed->access_token};
      auto replaced = false;
      for (auto& header : retry_request.headers) {
        if (header.starts_with("Authorization: Bearer ")) {
          header = replacement;
          replaced = true;
          break;
        }
      }
      if (!replaced) {
        retry_request.headers.push_back(replacement);
      }

      auto retry_response = send_request(retry_request);
      if (!retry_response) {
        return std::unexpected{retry_response.error()};
      }
      return retry_response;
    }
  }

  return response;
}

}  // namespace gsheetpp
