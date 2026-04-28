#pragma once

/**
 * @file google_sheets_client_impl.hpp
 * @brief BasicGoogleSheetsClient のテンプレート実装を定義します。
 */

namespace gsheetpp {

namespace detail {

  /**
   * @brief OAuth2 トークン応答のうち refresh token を保持できる形です。
   */
  struct OAuthTokenResponse {
    TokenInfo                  token{};          ///< 取得したアクセストークンです。
    std::optional<std::string> refresh_token{};  ///< 新たに払い出された refresh token です。
  };

  /**
   * @brief refresh 中フラグを例外安全に解除する RAII ガードです。
   *
   * token 取得処理の途中で失敗しても待機中スレッドが永続停止しないようにします。
   */
  class RefreshInProgressGuard {
public:
    /**
     * @brief ガード対象を初期化します。
     * @param mutex refresh 状態を保護する mutex です。
     * @param cv refresh 完了通知用の condition_variable です。
     * @param refresh_in_progress refresh 中フラグへの参照です。
     */
    RefreshInProgressGuard(std::mutex& mutex, std::condition_variable& cv, bool& refresh_in_progress) noexcept;

    RefreshInProgressGuard(RefreshInProgressGuard const&)                    = delete;
    auto operator=(RefreshInProgressGuard const&) -> RefreshInProgressGuard& = delete;
    RefreshInProgressGuard(RefreshInProgressGuard&&)                         = delete;
    auto operator=(RefreshInProgressGuard&&) -> RefreshInProgressGuard&      = delete;

    /**
     * @brief refresh 中フラグを解除し、待機中スレッドへ通知します。
     */
    ~RefreshInProgressGuard() noexcept;

private:
    std::mutex&              mutex_;                ///< 状態更新を直列化する mutex です。
    std::condition_variable& cv_;                   ///< 待機解除通知に使う condition_variable です。
    bool&                    refresh_in_progress_;  ///< 解除対象の refresh 中フラグです。
  };

  /**
   * @brief libcurl ベースのデフォルト transport を返します。
   * @return HTTP 実行関数です。
   */
  auto default_transport() -> TransportFunction;

  /**
   * @brief サービスアカウントのトークン応答を TokenInfo に変換します。
   * @param json トークン応答 JSON です。
   * @return 成功時は TokenInfo、失敗時は GoogleSheetsError です。
   */
  auto parse_token_response(std::string_view json) -> std::expected<TokenInfo, GoogleSheetsError>;

  /**
   * @brief OAuth2 トークン応答を access token と refresh token に分解します。
   * @param json OAuth2 トークン応答 JSON です。
   * @return 成功時は OAuthTokenResponse、失敗時は GoogleSheetsError です。
   */
  auto parse_oauth_token_response(std::string_view json) -> std::expected<OAuthTokenResponse, GoogleSheetsError>;

  /**
   * @brief トークン取得失敗応答を認証エラーへ変換します。
   * @param json エラー応答本文です。
   * @param http_status 失敗時の HTTP ステータスです。
   * @return 常に authentication エラーを返します。
   */
  auto parse_token_error_response(std::string_view json, long http_status) -> std::expected<void, GoogleSheetsError>;

  /**
   * @brief values.get 応答を ReadValuesResult に変換します。
   * @param json Google Sheets API 応答 JSON です。
   * @return 成功時は ReadValuesResult、失敗時は GoogleSheetsError です。
   */
  auto parse_read_values_response(std::string_view json) -> std::expected<ReadValuesResult, GoogleSheetsError>;

  /**
   * @brief values.update 応答を WriteValuesResult に変換します。
   * @param json Google Sheets API 応答 JSON です。
   * @return 成功時は WriteValuesResult、失敗時は GoogleSheetsError です。
   */
  auto parse_write_values_response(std::string_view json) -> std::expected<WriteValuesResult, GoogleSheetsError>;

  /**
   * @brief Google API 標準エラー応答から message を取り出します。
   * @param json エラー応答 JSON です。
   * @return 成功時は message、失敗時は GoogleSheetsError です。
   */
  auto parse_api_error_response(std::string_view json) -> std::expected<std::string, GoogleSheetsError>;

  /**
   * @brief サービスアカウント用 JWT assertion を生成します。
   * @param config サービスアカウント設定です。
   * @param issued_at JWT の iat に使う時刻です。
   * @return 成功時は署名済み JWT、失敗時は GoogleSheetsError です。
   */
  auto build_jwt_assertion(ServiceAccountConfig const& config, std::chrono::system_clock::time_point issued_at) -> std::expected<std::string, GoogleSheetsError>;

  /**
   * @brief JWT bearer grant 用のフォーム本文を組み立てます。
   * @param assertion 署名済み JWT assertion です。
   * @return application/x-www-form-urlencoded 形式本文です。
   */
  auto build_token_request_body(std::string_view assertion) -> std::string;

  /**
   * @brief authorization code 交換用のフォーム本文を組み立てます。
   * @param auth OAuth2 認証設定です。
   * @return application/x-www-form-urlencoded 形式本文です。
   */
  auto build_oauth_authorization_code_request_body(UserOAuth2Auth const& auth) -> std::string;

  /**
   * @brief refresh token 交換用のフォーム本文を組み立てます。
   * @param auth OAuth2 認証設定です。
   * @param refresh_token 利用する refresh token です。
   * @return application/x-www-form-urlencoded 形式本文です。
   */
  auto build_oauth_refresh_request_body(UserOAuth2Auth const& auth, std::string_view refresh_token) -> std::string;

  /**
   * @brief values.update 用の JSON 本文を組み立てます。
   * @param values 書き込むセル値一覧です。
   * @return JSON 文字列です。
   */
  auto build_write_values_request_body(std::vector<std::vector<std::string>> const& values) -> std::string;

  /**
   * @brief values API エンドポイント URL を組み立てます。
   * @param spreadsheet_id 対象スプレッドシート ID です。
   * @param range A1 形式のレンジです。
   * @param query 付加したいクエリ文字列です。
   * @return 組み立て済み URL です。
   */
  auto build_values_url(std::string_view spreadsheet_id, std::string_view range, std::string_view query = {}) -> std::string;

  /**
   * @brief URL にクエリパラメータを安全に追加します。
   * @param url 更新対象 URL です。
   * @param key クエリキーです。
   * @param value クエリ値です。
   * @return クエリを追加した URL です。
   */
  auto append_query_parameter(std::string url, std::string_view key, std::string_view value) -> std::string;

  /**
   * @brief サービスアカウント向けの有効トークンを返します。
   * @param auth サービスアカウント設定です。
   * @param shared_state 共有トークン状態です。
   * @param transport HTTP 実行関数です。
   * @param clock 現在時刻取得関数です。
   * @return 成功時は TokenInfo、失敗時は GoogleSheetsError です。
   */
  auto get_valid_token(ServiceAccountConfig const& auth, std::shared_ptr<ClientSharedState> const& shared_state, TransportFunction const& transport, ClockFunction const& clock)
      -> std::expected<TokenInfo, GoogleSheetsError>;

  /**
   * @brief User OAuth2 向けの有効トークンを返します。
   * @param auth OAuth2 認証設定です。
   * @param shared_state 共有トークン状態です。
   * @param transport HTTP 実行関数です。
   * @param clock 現在時刻取得関数です。
   * @return 成功時は TokenInfo、失敗時は GoogleSheetsError です。
   */
  auto get_valid_token(UserOAuth2Auth& auth, std::shared_ptr<ClientSharedState> const& shared_state, TransportFunction const& transport, ClockFunction const& clock)
      -> std::expected<TokenInfo, GoogleSheetsError>;

  /**
   * @brief 失効済みアクセストークンを前提に強制 refresh を行います。
   * @param auth OAuth2 認証設定です。
   * @param shared_state 共有トークン状態です。
   * @param transport HTTP 実行関数です。
   * @param clock 現在時刻取得関数です。
   * @param failed_access_token 401 を返したアクセストークンです。
   * @return 成功時は新しい TokenInfo、失敗時は GoogleSheetsError です。
   */
  auto force_refresh_token(UserOAuth2Auth& auth, std::shared_ptr<ClientSharedState> const& shared_state, TransportFunction const& transport, ClockFunction const& clock,
                           std::string_view failed_access_token) -> std::expected<TokenInfo, GoogleSheetsError>;

}  // namespace detail

template <Authenticator Auth>
/**
 * @brief デフォルト transport を用いてクライアントを初期化します。
 * @param auth 利用する認証設定です。
 */
BasicGoogleSheetsClient<Auth>::BasicGoogleSheetsClient(Auth auth) : BasicGoogleSheetsClient(std::move(auth), detail::default_transport(), {}) {}

template <Authenticator Auth>
/**
 * @brief transport / clock を明示してクライアントを初期化します。
 * @param auth 利用する認証設定です。
 * @param transport HTTP 実行関数です。
 * @param clock 現在時刻取得関数です。
 */
BasicGoogleSheetsClient<Auth>::BasicGoogleSheetsClient(Auth auth, detail::TransportFunction transport, detail::ClockFunction clock)
    : auth_{std::make_shared<Auth>(std::move(auth))}, transport_{std::move(transport)}, clock_{std::move(clock)}, shared_state_{std::make_shared<detail::ClientSharedState>()} {}

template <Authenticator Auth>
template <Authenticator OtherAuth>
/**
 * @brief 既存 transport / clock を引き継いで認証方式を切り替えます。
 * @tparam OtherAuth 切り替え先の認証型です。
 * @param auth 新しい認証設定です。
 * @return 切り替え後のクライアントです。
 */
auto BasicGoogleSheetsClient<Auth>::set_authenticator(OtherAuth auth) const -> BasicGoogleSheetsClient<OtherAuth> {
  return BasicGoogleSheetsClient<OtherAuth>{
      std::move(auth),
      transport_,
      clock_,
  };
}

template <Authenticator Auth>
/**
 * @brief 認証方式に応じてアクセストークン取得を非同期実行します。
 * @return 成功時は TokenInfo、失敗時は GoogleSheetsError を返す future です。
 */
auto BasicGoogleSheetsClient<Auth>::authenticate_async() -> std::future<std::expected<TokenInfo, GoogleSheetsError>> {
  return std::async(std::launch::async, [client = *this]() mutable -> std::expected<TokenInfo, GoogleSheetsError> {
    if constexpr (std::same_as<Auth, ApiKeyAuth>) {
      return TokenInfo{
          .access_token       = "",
          .token_type         = "ApiKey",
          .expires_in_seconds = 0,
      };
    } else if constexpr (std::same_as<Auth, ServiceAccountConfig>) {
      return detail::get_valid_token(*client.auth_, client.shared_state_, client.transport_, client.clock_);
    } else {
      return detail::get_valid_token(*client.auth_, client.shared_state_, client.transport_, client.clock_);
    }
  });
}

template <Authenticator Auth>
/**
 * @brief values.get を非同期実行します。
 * @param spreadsheet_id 対象スプレッドシート ID です。
 * @param range A1 形式のレンジです。
 * @return 成功時は ReadValuesResult、失敗時は GoogleSheetsError を返す future です。
 */
auto BasicGoogleSheetsClient<Auth>::read_values_async(std::string_view spreadsheet_id, std::string_view range) -> std::future<std::expected<ReadValuesResult, GoogleSheetsError>> {
  auto const spreadsheet = std::string{spreadsheet_id};
  auto const read_range  = std::string{range};
  return std::async(std::launch::async, [client = *this, spreadsheet, read_range]() mutable -> std::expected<ReadValuesResult, GoogleSheetsError> {
    auto request = detail::HttpRequest{
        .method = "GET",
        .url    = detail::build_values_url(spreadsheet, read_range),
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
      // Google API 標準エラー形式なら message を抽出し、そうでなければ汎用 HTTP エラーへ落とします。
      auto api_error = detail::parse_api_error_response(response->body);
      if (!api_error) {
        return std::unexpected{GoogleSheetsError{
            .kind          = GoogleSheetsErrorKind::http,
            .message       = "google sheets request failed",
            .http_status   = response->status_code,
            .response_body = response->body,
        }};
      }
      return std::unexpected{GoogleSheetsError{
          .kind          = GoogleSheetsErrorKind::api_response,
          .message       = api_error->empty() ? "google api request failed" : *api_error,
          .http_status   = response->status_code,
          .response_body = response->body,
      }};
    }

    return detail::parse_read_values_response(response->body);
  });
}

template <Authenticator Auth>
/**
 * @brief values.update を非同期実行します。
 * @param spreadsheet_id 対象スプレッドシート ID です。
 * @param range A1 形式のレンジです。
 * @param data 行単位の書き込みデータです。
 * @return 成功時は WriteValuesResult、失敗時は GoogleSheetsError を返す future です。
 */
auto BasicGoogleSheetsClient<Auth>::write_values_async(std::string_view spreadsheet_id, std::string_view range, std::span<std::vector<std::string> const> data)
    -> std::future<std::expected<WriteValuesResult, GoogleSheetsError>> {
  auto const spreadsheet = std::string{spreadsheet_id};
  auto const write_range = std::string{range};
  auto const values      = std::vector<std::vector<std::string>>{data.begin(), data.end()};
  return std::async(std::launch::async, [client = *this, spreadsheet, write_range, values]() mutable -> std::expected<WriteValuesResult, GoogleSheetsError> {
    auto request = detail::HttpRequest{
        .method  = "PUT",
        .url     = detail::build_values_url(spreadsheet, write_range, "valueInputOption=RAW"),
        .headers = {"Content-Type: application/json"},
        .body    = detail::build_write_values_request_body(values),
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
      // read と同じ方針で、Google API 由来の詳細 message が取れる場合は優先します。
      auto api_error = detail::parse_api_error_response(response->body);
      if (!api_error) {
        return std::unexpected{GoogleSheetsError{
            .kind          = GoogleSheetsErrorKind::http,
            .message       = "google sheets request failed",
            .http_status   = response->status_code,
            .response_body = response->body,
        }};
      }
      return std::unexpected{GoogleSheetsError{
          .kind          = GoogleSheetsErrorKind::api_response,
          .message       = api_error->empty() ? "google api request failed" : *api_error,
          .http_status   = response->status_code,
          .response_body = response->body,
      }};
    }

    return detail::parse_write_values_response(response->body);
  });
}

template <Authenticator Auth>
/**
 * @brief 認証方式に応じて HTTP リクエストへ認証情報を付与します。
 * @param request 更新対象の HTTP リクエストです。
 * @param is_write 書き込みリクエストかどうかです。
 * @return 成功時は void、失敗時は GoogleSheetsError です。
 */
auto BasicGoogleSheetsClient<Auth>::prepare_request(detail::HttpRequest& request, bool is_write) -> std::expected<void, GoogleSheetsError> {
  if constexpr (std::same_as<Auth, ApiKeyAuth>) {
    if (auth_->api_key.empty()) {
      return std::unexpected{GoogleSheetsError{
          .kind    = GoogleSheetsErrorKind::configuration,
          .message = "api_key is required",
      }};
    }
    if (is_write) {
      return std::unexpected{GoogleSheetsError{
          .kind    = GoogleSheetsErrorKind::configuration,
          .message = "api key authentication is read-only",
      }};
    }
    request.url = detail::append_query_parameter(std::move(request.url), "key", auth_->api_key);
    return {};
  } else if constexpr (std::same_as<Auth, ServiceAccountConfig>) {
    auto token = detail::get_valid_token(*auth_, shared_state_, transport_, clock_);
    if (!token) {
      return std::unexpected{token.error()};
    }
    request.headers.push_back("Authorization: Bearer " + token->access_token);
    return {};
  } else {
    auto token = detail::get_valid_token(*auth_, shared_state_, transport_, clock_);
    if (!token) {
      return std::unexpected{token.error()};
    }
    request.headers.push_back("Authorization: Bearer " + token->access_token);
    return {};
  }
}

template <Authenticator Auth>
/**
 * @brief HTTP リクエストを実行し、User OAuth2 のみ 401 時に 1 回だけ再試行します。
 * @param request 実行対象の HTTP リクエストです。
 * @param retry_on_unauthorized 401 再試行を許可するかどうかです。
 * @return 成功時は HttpResponse、失敗時は GoogleSheetsError です。
 */
auto BasicGoogleSheetsClient<Auth>::execute_request(detail::HttpRequest request, bool retry_on_unauthorized) -> std::expected<detail::HttpResponse, GoogleSheetsError> {
  auto send_request = [this](detail::HttpRequest const& current_request) -> std::expected<detail::HttpResponse, GoogleSheetsError> {
    if (!transport_) {
      return std::unexpected{GoogleSheetsError{
          .kind    = GoogleSheetsErrorKind::network,
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
      // 失効したアクセストークンを控えておくことで、他スレッドが先に refresh 済みかどうかを判断できます。
      auto failed_access_token = std::string_view{};
      for (auto const& header : request.headers) {
        if (header.starts_with("Authorization: Bearer ")) {
          failed_access_token = std::string_view{header}.substr(std::string_view{"Authorization: Bearer "}.size());
          break;
        }
      }

      auto refreshed = detail::force_refresh_token(*auth_, shared_state_, transport_, clock_, failed_access_token);
      if (!refreshed) {
        return std::unexpected{refreshed.error()};
      }

      // 元リクエストの Authorization ヘッダーだけ差し替え、同一条件で 1 回だけ再送します。
      auto       retry_request = std::move(request);
      auto const replacement   = std::string{"Authorization: Bearer " + refreshed->access_token};
      auto       replaced      = false;
      for (auto& header : retry_request.headers) {
        if (header.starts_with("Authorization: Bearer ")) {
          header   = replacement;
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
