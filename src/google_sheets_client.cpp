#include "gsheetpp/google_sheets_client.hpp"

#include "google_sheets_client_detail.hpp"
#include "http_client.hpp"

#include <glaze/glaze.hpp>
#include <jwt-cpp/traits/nlohmann-json/defaults.h>

#include <cctype>
#include <format>
#include <tuple>

/**
 * @file google_sheets_client.cpp
 * @brief Google Sheets クライアントの内部処理を実装します。
 */

namespace gsheetpp {

/**
 * @brief addOverGridImage 用の内部リクエスト構造体です。
 */
struct AddOverGridImagePayload {
  struct Image {
    struct Location {
      struct OverlayPosition {
        struct AnchorCell {
          int sheetId{};
          int rowIndex{};
          int columnIndex{};
        } anchorCell{};
        int offsetXPixels{};
        int offsetYPixels{};
        int widthPixels{};
        int heightPixels{};
      } overlayPosition{};
    } location{};
    std::string sourceUri{};
  } image{};
};

/**
 * @brief batchUpdate 全体のリクエスト構造体（addOverGridImage 用）です。
 */
struct AddOverGridImageRequest {
  struct Request {
    AddOverGridImagePayload addOverGridImage{};
  };
  std::vector<Request> requests{};
};

/**
 * @brief values.get 応答 JSON を受ける一時構造体です。
 */
struct ReadValuesPayload {
  std::string                           range{};
  std::string                           majorDimension{};
  std::vector<std::vector<std::string>> values{};
};

/**
 * @brief values.update 応答 JSON を受ける一時構造体です。
 */
struct WriteValuesPayload {
  std::string spreadsheetId{};
  std::string updatedRange{};
  long        updatedRows{};
  long        updatedColumns{};
  long        updatedCells{};
};

/**
 * @brief spreadsheets.get 応答 JSON を受ける一時構造体です。
 */
struct SpreadsheetPayload {
  std::string spreadsheetId{};
  struct SheetPayload {
    SheetMetadata properties{};
  };
  std::vector<SheetPayload> sheets{};
};

/**
 * @brief batchUpdate 応答を受け取る一時構造体です。
 */
struct BatchUpdatePayload {
  struct ReplyPayload {
    struct AddSheetReplyPayload {
      SheetMetadata properties{};
    };
    std::optional<AddSheetReplyPayload> addSheet{};
  };
  std::vector<ReplyPayload> replies{};
};

/**
 * @brief Drive API files.list 応答を受け取る一時構造体です。
 */
struct DriveFilesPayload {
  std::vector<DriveFile> files{};
};

/**
 * @brief セル書式設定用の内部構造体です。
 */
struct ColorPayload {
  float red{};
  float green{};
  float blue{};
};

struct TextFormatPayload {
  std::optional<ColorPayload> foregroundColor{};
  std::optional<bool>         bold{};
};

struct UserEnteredFormatPayload {
  std::optional<ColorPayload>      backgroundColor{};
  std::optional<TextFormatPayload> textFormat{};
};

struct CellPayload {
  std::optional<UserEnteredFormatPayload> userEnteredFormat{};
};

struct GridRangePayload {
  int                sheetId{};
  std::optional<int> startRowIndex{};
  std::optional<int> endRowIndex{};
  std::optional<int> startColumnIndex{};
  std::optional<int> endColumnIndex{};
};

struct RepeatCellPayload {
  GridRangePayload range{};
  CellPayload      cell{};
  std::string      fields{};
};

/**
 * @brief シートプロパティ更新用の内部構造体です。
 */
struct UpdateSheetPropertiesPayload {
  struct Properties {
    int sheetId{};
    struct GridProperties {
      std::optional<int> frozenRowCount{};
      std::optional<int> frozenColumnCount{};
    } gridProperties{};
  } properties{};
  std::string fields{};
};

/**
 * @brief batchUpdate 全体のリクエスト構造体（汎用）です。
 */
struct BatchUpdateRequestsPayload {
  struct Request {
    std::optional<RepeatCellPayload>            repeatCell{};
    std::optional<UpdateSheetPropertiesPayload> updateSheetProperties{};
  };
  std::vector<Request> requests{};
};

/**
 * @brief Google API 標準エラーの内部 `error` オブジェクトです。
 */
struct GoogleApiErrorStatus {
  long        code{};
  std::string message{};
  std::string status{};
};

/**
 * @brief Google API 標準エラー応答全体です。
 */
struct GoogleApiErrorPayload {
  GoogleApiErrorStatus error{};
};

}  // namespace gsheetpp

namespace glz {

template <>
struct meta<gsheetpp::TokenInfo> {
  using T = gsheetpp::TokenInfo;

  static constexpr auto value = object(
      "access_token", &T::access_token,
      "token_type", &T::token_type,
      "expires_in", &T::expires_in_seconds);
};

template <>
struct meta<gsheetpp::detail::OAuthTokenResponse> {
  using T = gsheetpp::detail::OAuthTokenResponse;

  static constexpr auto read_access_token = [](T& value, std::string const& input) {
    value.token.access_token = input;
  };
  static constexpr auto write_access_token = [](T const& value) -> std::string const& {
    return value.token.access_token;
  };
  static constexpr auto read_token_type = [](T& value, std::string const& input) {
    value.token.token_type = input;
  };
  static constexpr auto write_token_type = [](T const& value) -> std::string const& {
    return value.token.token_type;
  };
  static constexpr auto read_expires_in = [](T& value, long input) {
    value.token.expires_in_seconds = input;
  };
  static constexpr auto write_expires_in = [](T const& value) -> long {
    return value.token.expires_in_seconds;
  };

  static constexpr auto value = object(
      "access_token", custom<read_access_token, write_access_token>,
      "token_type", custom<read_token_type, write_token_type>,
      "expires_in", custom<read_expires_in, write_expires_in>,
      "refresh_token", &T::refresh_token);
};

template <>
struct meta<gsheetpp::SheetMetadata> {
  using T = gsheetpp::SheetMetadata;

  static constexpr auto value = object(
      "title", &T::title,
      "sheetId", &T::sheet_id);
};

template <>
struct meta<gsheetpp::BatchUpdatePayload::ReplyPayload::AddSheetReplyPayload> {
  using T = gsheetpp::BatchUpdatePayload::ReplyPayload::AddSheetReplyPayload;
  static constexpr auto value = object("properties", &T::properties);
};

template <>
struct meta<gsheetpp::BatchUpdatePayload::ReplyPayload> {
  using T = gsheetpp::BatchUpdatePayload::ReplyPayload;
  static constexpr auto value = object("addSheet", &T::addSheet);
};

template <>
struct meta<gsheetpp::AddOverGridImagePayload::Image::Location::OverlayPosition::AnchorCell> {
  using T = gsheetpp::AddOverGridImagePayload::Image::Location::OverlayPosition::AnchorCell;
  static constexpr auto value = object(
      "sheetId", &T::sheetId,
      "rowIndex", &T::rowIndex,
      "columnIndex", &T::columnIndex);
};

template <>
struct meta<gsheetpp::AddOverGridImagePayload::Image::Location::OverlayPosition> {
  using T = gsheetpp::AddOverGridImagePayload::Image::Location::OverlayPosition;
  static constexpr auto value = object(
      "anchorCell", &T::anchorCell,
      "offsetXPixels", &T::offsetXPixels,
      "offsetYPixels", &T::offsetYPixels,
      "widthPixels", &T::widthPixels,
      "heightPixels", &T::heightPixels);
};

template <>
struct meta<gsheetpp::AddOverGridImagePayload::Image::Location> {
  using T = gsheetpp::AddOverGridImagePayload::Image::Location;
  static constexpr auto value = object("overlayPosition", &T::overlayPosition);
};

template <>
struct meta<gsheetpp::AddOverGridImagePayload::Image> {
  using T = gsheetpp::AddOverGridImagePayload::Image;
  static constexpr auto value = object(
      "location", &T::location,
      "sourceUri", &T::sourceUri);
};

template <>
struct meta<gsheetpp::AddOverGridImagePayload> {
  using T = gsheetpp::AddOverGridImagePayload;
  static constexpr auto value = object("image", &T::image);
};

template <>
struct meta<gsheetpp::AddOverGridImageRequest::Request> {
  using T = gsheetpp::AddOverGridImageRequest::Request;
  static constexpr auto value = object("addOverGridImage", &T::addOverGridImage);
};

template <>
struct meta<gsheetpp::AddOverGridImageRequest> {
  using T = gsheetpp::AddOverGridImageRequest;
  static constexpr auto value = object("requests", &T::requests);
};

template <>
struct meta<gsheetpp::DriveFile> {
  using T = gsheetpp::DriveFile;
  static constexpr auto value = object(
      "id", &T::id,
      "name", &T::name);
};

template <>
struct meta<gsheetpp::DriveFilesPayload> {
  using T = gsheetpp::DriveFilesPayload;
  static constexpr auto value = object("files", &T::files);
};

template <>
struct meta<gsheetpp::SpreadsheetPayload::SheetPayload> {
  using T = gsheetpp::SpreadsheetPayload::SheetPayload;
  static constexpr auto value = object("properties", &T::properties);
};

template <>
struct meta<gsheetpp::SpreadsheetPayload> {
  using T = gsheetpp::SpreadsheetPayload;
  static constexpr auto value = object(
      "spreadsheetId", &T::spreadsheetId,
      "sheets", &T::sheets);
};

template <>
struct meta<gsheetpp::BatchUpdatePayload> {
  using T = gsheetpp::BatchUpdatePayload;
  static constexpr auto value = object("replies", &T::replies);
};

template <>
struct meta<gsheetpp::ColorPayload> {
  using T = gsheetpp::ColorPayload;
  static constexpr auto value = object(
      "red", &T::red,
      "green", &T::green,
      "blue", &T::blue);
};

template <>
struct meta<gsheetpp::TextFormatPayload> {
  using T = gsheetpp::TextFormatPayload;
  static constexpr auto value = object(
      "foregroundColor", &T::foregroundColor,
      "bold", &T::bold);
};

template <>
struct meta<gsheetpp::UserEnteredFormatPayload> {
  using T = gsheetpp::UserEnteredFormatPayload;
  static constexpr auto value = object(
      "backgroundColor", &T::backgroundColor,
      "textFormat", &T::textFormat);
};

template <>
struct meta<gsheetpp::CellPayload> {
  using T = gsheetpp::CellPayload;
  static constexpr auto value = object("userEnteredFormat", &T::userEnteredFormat);
};

template <>
struct meta<gsheetpp::GridRangePayload> {
  using T = gsheetpp::GridRangePayload;
  static constexpr auto value = object(
      "sheetId", &T::sheetId,
      "startRowIndex", &T::startRowIndex,
      "endRowIndex", &T::endRowIndex,
      "startColumnIndex", &T::startColumnIndex,
      "endColumnIndex", &T::endColumnIndex);
};

template <>
struct meta<gsheetpp::RepeatCellPayload> {
  using T = gsheetpp::RepeatCellPayload;
  static constexpr auto value = object(
      "range", &T::range,
      "cell", &T::cell,
      "fields", &T::fields);
};

template <>
struct meta<gsheetpp::UpdateSheetPropertiesPayload::Properties::GridProperties> {
  using T = gsheetpp::UpdateSheetPropertiesPayload::Properties::GridProperties;
  static constexpr auto value = object(
      "frozenRowCount", &T::frozenRowCount,
      "frozenColumnCount", &T::frozenColumnCount);
};

template <>
struct meta<gsheetpp::UpdateSheetPropertiesPayload::Properties> {
  using T = gsheetpp::UpdateSheetPropertiesPayload::Properties;
  static constexpr auto value = object(
      "sheetId", &T::sheetId,
      "gridProperties", &T::gridProperties);
};

template <>
struct meta<gsheetpp::UpdateSheetPropertiesPayload> {
  using T = gsheetpp::UpdateSheetPropertiesPayload;
  static constexpr auto value = object(
      "properties", &T::properties,
      "fields", &T::fields);
};

template <>
struct meta<gsheetpp::BatchUpdateRequestsPayload::Request> {
  using T = gsheetpp::BatchUpdateRequestsPayload::Request;
  static constexpr auto value = object(
      "repeatCell", &T::repeatCell,
      "updateSheetProperties", &T::updateSheetProperties);
};

template <>
struct meta<gsheetpp::BatchUpdateRequestsPayload> {
  using T = gsheetpp::BatchUpdateRequestsPayload;
  static constexpr auto value = object("requests", &T::requests);
};

}  // namespace glz

namespace gsheetpp {

namespace {

  /**
   * @brief Google の応答 JSON に未知キーが含まれても失敗させない設定です。
   */
  auto constexpr ignore_unknown_keys_opts = glz::opts{
      .error_on_unknown_keys = false,
  };

  /**
   * @brief JSON 解析失敗を表すエラーを組み立てます。
   * @param message 返したい失敗理由です。
   * @return serialization 種別のエラーです。
   */
  auto make_parse_error(std::string_view message) -> GoogleSheetsError {
    return GoogleSheetsError{
        .kind    = GoogleSheetsErrorKind::serialization,
        .message = std::string{message},
    };
  }

  /**
   * @brief 構成値エラーを組み立てます。
   * @param message 返したい失敗理由です。
   * @return configuration 種別のエラーです。
   */
  auto make_configuration_error(std::string_view message) -> GoogleSheetsError {
    return GoogleSheetsError{
        .kind    = GoogleSheetsErrorKind::configuration,
        .message = std::string{message},
    };
  }

  /**
   * @brief HTTP 応答由来のエラーを共通形式で組み立てます。
   * @param kind 返すエラー種別です。
   * @param message 返したい失敗理由です。
   * @param http_status HTTP ステータスです。
   * @param response_body 生レスポンス本文です。
   * @return 指定内容を詰めた GoogleSheetsError です。
   */
  auto make_http_error(GoogleSheetsErrorKind kind, std::string_view message, long http_status, std::string_view response_body) -> GoogleSheetsError {
    return GoogleSheetsError{
        .kind          = kind,
        .message       = std::string{message},
        .http_status   = http_status,
        .response_body = std::string{response_body},
    };
  }

  template <class T>
  /**
   * @brief JSON を指定型へデシリアライズします。
   * @tparam T 変換先型です。
   * @param json 解析対象 JSON 文字列です。
   * @param message 失敗時に返すエラーメッセージです。
   * @return 成功時は T、失敗時は GoogleSheetsError です。
   */
  auto parse_json(std::string_view json, std::string_view message) -> std::expected<T, GoogleSheetsError> {
    auto       value       = T{};
    auto const json_buffer = std::string{json};
    if (auto const ec = glz::read<ignore_unknown_keys_opts>(value, json_buffer)) {
      return std::unexpected{make_parse_error(message)};
    }
    return value;
  }

  /**
   * @brief URL / フォーム本文向けに文字列を percent-encode して既存文字列へ追記します。
   * @param input 変換対象文字列です。
   * @param out 追記先文字列です。
   */
  auto percent_encode(std::string_view input, std::string& out) -> void {
    auto constexpr hex = "0123456789ABCDEF";
    out.reserve(out.size() + input.size() * 3);

    for (auto const ch : input) {
      auto const byte = static_cast<unsigned char>(ch);
      if (std::isalnum(byte) != 0 || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
        out.push_back(static_cast<char>(ch));
        continue;
      }

      out.push_back('%');
      out.push_back(hex[(byte >> 4U) & 0x0FU]);
      out.push_back(hex[byte & 0x0FU]);
    }
  }

  /**
   * @brief URL / フォーム本文向けに文字列を percent-encode します。
   * @param input 変換対象文字列です。
   * @return RFC 3986 互換のエンコード結果です。
   */
  auto percent_encode(std::string_view input) -> std::string {
    auto encoded = std::string{};
    percent_encode(input, encoded);
    return encoded;
  }

  /**
   * @brief clock が注入されていればそれを、なければ system_clock の現在時刻を返します。
   * @param clock 任意の現在時刻関数です。
   * @return 現在時刻です。
   */
  auto now_or_default(detail::ClockFunction const& clock) -> std::chrono::system_clock::time_point {
    if (clock) {
      return clock();
    }
    return std::chrono::system_clock::now();
  }

  /**
   * @brief 共有キャッシュ中のアクセストークンがまだ安全に使えるか判定します。
   * @param shared_state 共有トークン状態です。
   * @param clock 現在時刻関数です。
   * @return true の場合は refresh 不要です。
   */
  auto token_is_valid(std::shared_ptr<detail::ClientSharedState> const& shared_state, detail::ClockFunction const& clock) -> bool {
    auto const now = now_or_default(clock);
    return shared_state->token.has_value() && (shared_state->expires_at - std::chrono::seconds{30} > now);
  }

  /**
   * @brief 取得済みトークンを共有キャッシュへ保存します。
   * @param shared_state 共有トークン状態です。
   * @param token 保存するトークンです。
   * @param issued_at 発行基準時刻です。
   */
  auto store_token(std::shared_ptr<detail::ClientSharedState> const& shared_state, TokenInfo const& token, std::chrono::system_clock::time_point issued_at) -> void {
    auto _                   = std::scoped_lock{shared_state->mutex};
    shared_state->token      = token;
    shared_state->expires_at = issued_at + std::chrono::seconds{token.expires_in_seconds};
  }

  template <class Fetch>
  /**
   * @brief 共有キャッシュを見て必要時のみトークン取得処理を 1 本化します。
   * @tparam Fetch 実際の取得処理を表す callable です。
   * @param shared_state 共有トークン状態です。
   * @param clock 現在時刻関数です。
   * @param fetch 実際に新しい token を取りに行く処理です。
   * @return 成功時は TokenInfo、失敗時は GoogleSheetsError です。
   */
  auto get_or_refresh_token(std::shared_ptr<detail::ClientSharedState> const& shared_state, detail::ClockFunction const& clock, Fetch&& fetch) -> std::expected<TokenInfo, GoogleSheetsError> {
    auto lock = std::unique_lock{shared_state->mutex};
    while (shared_state->refresh_in_progress) {
      shared_state->cv.wait(lock);
    }

    // 先行スレッドの refresh が終わっていれば、その結果をそのまま再利用します。
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
  /**
   * @brief 401 後の強制 refresh を 1 本化します。
   * @tparam Fetch 実際の refresh 処理を表す callable です。
   * @param shared_state 共有トークン状態です。
   * @param clock 現在時刻関数です。
   * @param failed_access_token 失効していたアクセストークンです。
   * @param fetch 実際に新しい token を取りに行く処理です。
   * @return 成功時は TokenInfo、失敗時は GoogleSheetsError です。
   */
  auto force_refresh(std::shared_ptr<detail::ClientSharedState> const& shared_state, detail::ClockFunction const& clock, std::string_view failed_access_token, Fetch&& fetch)
      -> std::expected<TokenInfo, GoogleSheetsError> {
    {
      auto lock = std::unique_lock{shared_state->mutex};
      while (shared_state->refresh_in_progress) {
        shared_state->cv.wait(lock);
      }
      // 他スレッドがすでに別トークンへ更新済みなら、その結果をそのまま使います。
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

  /**
   * @brief transport が設定されていることを確認したうえで HTTP リクエストを投げます。
   * @param transport HTTP 実行関数です。
   * @param request 実行する HTTP リクエストです。
   * @return 成功時は HttpResponse、失敗時は GoogleSheetsError です。
   */
  auto perform_request(detail::TransportFunction const& transport, detail::HttpRequest const& request) -> std::expected<detail::HttpResponse, GoogleSheetsError> {
    if (!transport) {
      return std::unexpected{GoogleSheetsError{
          .kind    = GoogleSheetsErrorKind::network,
          .message = "transport is not configured",
      }};
    }
    return transport(request);
  }

  /**
   * @brief サービスアカウント認証でアクセストークンを取得します。
   * @param auth サービスアカウント設定です。
   * @param shared_state 共有トークン状態です。
   * @param transport HTTP 実行関数です。
   * @param clock 現在時刻関数です。
   * @return 成功時は TokenInfo、失敗時は GoogleSheetsError です。
   */
  auto fetch_service_account_token(ServiceAccountConfig const& auth, std::shared_ptr<detail::ClientSharedState> const& shared_state, detail::TransportFunction const& transport,
                                   detail::ClockFunction const& clock) -> std::expected<TokenInfo, GoogleSheetsError> {
    auto const issued_at = now_or_default(clock);
    auto       assertion = detail::build_jwt_assertion(auth, issued_at);
    if (!assertion) {
      return std::unexpected{assertion.error()};
    }

    // サービスアカウントは JWT bearer grant で毎回アクセストークンを取得します。
    auto response = perform_request(transport, detail::HttpRequest{
                                                   .method  = "POST",
                                                   .url     = auth.token_uri,
                                                   .headers = {"Content-Type: application/x-www-form-urlencoded"},
                                                   .body    = detail::build_token_request_body(assertion.value()),
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

  /**
   * @brief User OAuth2 の構成値が token 取得に足るか検証します。
   * @param auth OAuth2 認証設定です。
   * @return 成功時は void、失敗時は GoogleSheetsError です。
   */
  auto validate_user_oauth_config(UserOAuth2Auth const& auth) -> std::expected<void, GoogleSheetsError> {
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
          .kind    = GoogleSheetsErrorKind::authentication,
          .message = "user OAuth2 requires a refresh token or authorization code",
      }};
    }
    if (auth.current_refresh_token().empty() && auth.redirect_uri.empty()) {
      return std::unexpected{make_configuration_error("redirect_uri is required for authorization_code exchange")};
    }
    return {};
  }

  /**
   * @brief User OAuth2 のアクセストークン取得または更新を行います。
   * @param auth OAuth2 認証設定です。
   * @param shared_state 共有トークン状態です。
   * @param transport HTTP 実行関数です。
   * @param clock 現在時刻関数です。
   * @param force_refresh_only true の場合は authorization code 交換を禁止します。
   * @return 成功時は TokenInfo、失敗時は GoogleSheetsError です。
   */
  auto fetch_user_oauth_token(UserOAuth2Auth& auth, std::shared_ptr<detail::ClientSharedState> const& shared_state, detail::TransportFunction const& transport, detail::ClockFunction const& clock,
                              bool force_refresh_only) -> std::expected<TokenInfo, GoogleSheetsError> {
    auto const validated = validate_user_oauth_config(auth);
    if (!validated) {
      return std::unexpected{validated.error()};
    }
    auto       used_authorization_code = false;
    auto const auth_refresh_token      = auth.current_refresh_token();
    auto       refresh_token_snapshot  = std::string{};
    {
      // refresh token は shared_state 側を正としつつ、初回は authenticator 側から取り込みます。
      auto _ = std::scoped_lock{shared_state->mutex};
      if (shared_state->refresh_token.has_value()) {
        refresh_token_snapshot = *shared_state->refresh_token;
      } else if (!auth_refresh_token.empty()) {
        shared_state->refresh_token = auth_refresh_token;
        refresh_token_snapshot      = auth_refresh_token;
      }
    }
    if (!refresh_token_snapshot.empty() && refresh_token_snapshot != auth_refresh_token) {
      auth.set_refresh_token(refresh_token_snapshot);
    }

    auto request_body = std::string{};
    // まず refresh token を優先し、なければ初回 bootstrap として authorization code を使います。
    if (!refresh_token_snapshot.empty()) {
      request_body = detail::build_oauth_refresh_request_body(auth, refresh_token_snapshot);
    } else if (!force_refresh_only && !auth.authorization_code.empty()) {
      used_authorization_code = true;
      request_body            = detail::build_oauth_authorization_code_request_body(auth);
    } else {
      return std::unexpected{GoogleSheetsError{
          .kind    = GoogleSheetsErrorKind::authentication,
          .message = "user OAuth2 requires a refresh token or authorization code",
      }};
    }

    auto response = perform_request(transport, detail::HttpRequest{
                                                   .method  = "POST",
                                                   .url     = auth.token_uri,
                                                   .headers = {"Content-Type: application/x-www-form-urlencoded"},
                                                   .body    = std::move(request_body),
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
      // Google が refresh token を再発行した場合は共有状態と authenticator の両方へ反映します。
      auto _                      = std::scoped_lock{shared_state->mutex};
      shared_state->refresh_token = *oauth_response->refresh_token;
      latest_refresh_token        = oauth_response->refresh_token;
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

/**
 * @brief 取得データから最終行（1開始）を特定します。
 */
auto get_last_row(std::vector<std::vector<std::string>> const& values) -> int {
  return static_cast<int>(values.size());
}

/**
 * @brief 取得データから最終列（1開始）を特定します。
 */
auto get_last_column(std::vector<std::vector<std::string>> const& values) -> int {
  auto max_col = 0;
  for (auto const& row : values) {
    max_col = std::max(max_col, static_cast<int>(row.size()));
  }
  return max_col;
}

/**
 * @brief サービスアカウント JSON を構造化設定へ変換します。

 * @param json サービスアカウント JSON 文字列です。
 * @return 成功時は ServiceAccountConfig、失敗時は GoogleSheetsError です。
 */
auto parse_service_account_config(std::string_view json) -> std::expected<ServiceAccountConfig, GoogleSheetsError> {
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

  /**
   * @brief デフォルト transport を返します。
   * @return libcurl ベース transport です。
   */
  auto default_transport() -> TransportFunction {
    return [](HttpRequest const& request) { return http::perform_http_request(request); };
  }

  /**
   * @brief サービスアカウント token 応答を TokenInfo に変換します。
   * @param json token 応答 JSON です。
   * @return 成功時は TokenInfo、失敗時は GoogleSheetsError です。
   */
  auto parse_token_response(std::string_view json) -> std::expected<TokenInfo, GoogleSheetsError> {
    return parse_json<TokenInfo>(json, "failed to parse token response");
  }

  /**
   * @brief OAuth2 token 応答を access token と refresh token に変換します。
   * @param json token 応答 JSON です。
   * @return 成功時は OAuthTokenResponse、失敗時は GoogleSheetsError です。
   */
  auto parse_oauth_token_response(std::string_view json) -> std::expected<OAuthTokenResponse, GoogleSheetsError> {
    return parse_json<OAuthTokenResponse>(json, "failed to parse OAuth token response");
  }

  /**
   * @brief token 取得失敗を authentication エラーとして返します。
   * @param json 失敗応答本文です。
   * @param http_status HTTP ステータスです。
   * @return 常に失敗 expected を返します。
   */
  auto parse_token_error_response(std::string_view json, long http_status) -> std::expected<void, GoogleSheetsError> {
    return std::unexpected{make_http_error(GoogleSheetsErrorKind::authentication, "token request failed", http_status, json)};
  }

  /**
   * @brief values.get 応答を ReadValuesResult に変換します。
   * @param json 応答 JSON です。
   * @return 成功時は ReadValuesResult、失敗時は GoogleSheetsError です。
   */
  auto parse_read_values_response(std::string_view json) -> std::expected<ReadValuesResult, GoogleSheetsError> {
    auto payload = parse_json<ReadValuesPayload>(json, "failed to parse read values response");
    if (!payload) {
      return std::unexpected{payload.error()};
    }

    return ReadValuesResult{
        .range           = std::move(payload->range),
        .major_dimension = std::move(payload->majorDimension),
        .values          = std::move(payload->values),
    };
  }

  /**
   * @brief values.update 応答を WriteValuesResult に変換します。
   * @param json 応答 JSON です。
   * @return 成功時は WriteValuesResult、失敗時は GoogleSheetsError です。
   */
  auto parse_write_values_response(std::string_view json) -> std::expected<WriteValuesResult, GoogleSheetsError> {
    auto payload = parse_json<WriteValuesPayload>(json, "failed to parse write values response");
    if (!payload) {
      return std::unexpected{payload.error()};
    }

    return WriteValuesResult{
        .spreadsheet_id  = std::move(payload->spreadsheetId),
        .updated_range   = std::move(payload->updatedRange),
        .updated_rows    = payload->updatedRows,
        .updated_columns = payload->updatedColumns,
        .updated_cells   = payload->updatedCells,
    };
  }

  /**
   * @brief batchUpdate 応答から追加されたシートの情報を抽出します。
   * @param json 応答 JSON です。
   * @return 成功時は SheetMetadata、失敗時は GoogleSheetsError です。
   */
  auto parse_add_sheet_response(std::string_view json) -> std::expected<SheetMetadata, GoogleSheetsError> {
    auto payload = parse_json<BatchUpdatePayload>(json, "failed to parse batch update response");
    if (!payload) {
      return std::unexpected{payload.error()};
    }

    if (payload->replies.empty() || !payload->replies[0].addSheet.has_value()) {
      return std::unexpected{make_parse_error("addSheet reply not found in batch update response")};
    }

    return std::move(payload->replies[0].addSheet->properties);
  }

  /**
   * @brief spreadsheets.create 応答から spreadsheetId を抽出します。
   * @param json 応答 JSON です。
   * @return 成功時は spreadsheetId、失敗時は GoogleSheetsError です。
   */
  auto parse_create_spreadsheet_response(std::string_view json) -> std::expected<std::string, GoogleSheetsError> {
    auto payload = parse_json<SpreadsheetPayload>(json, "failed to parse create spreadsheet response");
    if (!payload) {
      return std::unexpected{payload.error()};
    }
    return std::move(payload->spreadsheetId);
  }

  /**
   * @brief Drive API files.list 応答を DriveFile のリストに変換します。
   * @param json 応答 JSON です。
   * @return 成功時は DriveFile のリスト、失敗時は GoogleSheetsError です。
   */
  auto parse_drive_files_response(std::string_view json) -> std::expected<std::vector<DriveFile>, GoogleSheetsError> {
    auto payload = parse_json<DriveFilesPayload>(json, "failed to parse drive files response");
    if (!payload) {
      return std::unexpected{payload.error()};
    }
    return std::move(payload->files);
  }

  /**
   * @brief spreadsheets.get 応答を SheetMetadata のリストに変換します。
   * @param json 応答 JSON です。
   * @return 成功時は SheetMetadata のリスト、失敗時は GoogleSheetsError です。
   */
  auto parse_get_sheets_response(std::string_view json) -> std::expected<std::vector<SheetMetadata>, GoogleSheetsError> {
    auto payload = parse_json<SpreadsheetPayload>(json, "failed to parse spreadsheet response");
    if (!payload) {
      return std::unexpected{payload.error()};
    }

    auto results = std::vector<SheetMetadata>{};
    results.reserve(payload->sheets.size());
    for (auto& sheet : payload->sheets) {
      results.push_back(std::move(sheet.properties));
    }

    return results;
  }

  /**
   * @brief Google API 標準エラー応答から message を抽出します。
   * @param json エラー応答 JSON です。
   * @return 成功時は message、失敗時は GoogleSheetsError です。
   */
  auto parse_api_error_response(std::string_view json) -> std::expected<std::string, GoogleSheetsError> {
    auto payload = parse_json<GoogleApiErrorPayload>(json, "failed to parse API error response");
    if (!payload) {
      return std::unexpected{payload.error()};
    }

    return std::move(payload->error.message);
  }

  /**
   * @brief サービスアカウント認証用 JWT assertion を生成します。
   * @param config サービスアカウント設定です。
   * @param issued_at iat / exp の基準時刻です。
   * @return 成功時は署名済み JWT、失敗時は GoogleSheetsError です。
   */
  auto build_jwt_assertion(ServiceAccountConfig const& config, std::chrono::system_clock::time_point issued_at) -> std::expected<std::string, GoogleSheetsError> {
    try {
      // Google Sheets API の service account フローに必要な固定 scope を claim に載せます。
      auto const token = jwt::create()
                             .set_type("JWT")
                             .set_issuer(config.client_email)
                             .set_audience(config.token_uri)
                             .set_issued_at(issued_at)
                             .set_expires_at(issued_at + std::chrono::hours{1})
                             .set_payload_claim("scope", jwt::claim(std::string{"https://www.googleapis.com/auth/spreadsheets"}))
                             .sign(jwt::algorithm::rs256("", config.private_key));
      return token;
    } catch (std::exception const& ex) {
      return std::unexpected{GoogleSheetsError{
          .kind    = GoogleSheetsErrorKind::jwt_signing,
          .message = ex.what(),
      }};
    }
  }

  /**
   * @brief JWT bearer grant 用フォーム本文を生成します。
   * @param assertion 署名済み JWT assertion です。
   * @return application/x-www-form-urlencoded 本文です。
   */
  auto build_token_request_body(std::string_view assertion) -> std::string {
    return std::format("grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion={}", percent_encode(assertion));
  }

  /**
   * @brief authorization code 交換用フォーム本文を生成します。
   * @param auth OAuth2 認証設定です。
   * @return application/x-www-form-urlencoded 本文です。
   */
  auto build_oauth_authorization_code_request_body(UserOAuth2Auth const& auth) -> std::string {
    return std::format("client_id={}&client_secret={}&redirect_uri={}&code={}&grant_type=authorization_code", percent_encode(auth.client_id), percent_encode(auth.client_secret),
                       percent_encode(auth.redirect_uri), percent_encode(auth.authorization_code));
  }

  /**
   * @brief refresh token 交換用フォーム本文を生成します。
   * @param auth OAuth2 認証設定です。
   * @param refresh_token 利用する refresh token です。
   * @return application/x-www-form-urlencoded 本文です。
   */
  auto build_oauth_refresh_request_body(UserOAuth2Auth const& auth, std::string_view refresh_token) -> std::string {
    return std::format("client_id={}&client_secret={}&refresh_token={}&grant_type=refresh_token", percent_encode(auth.client_id), percent_encode(auth.client_secret), percent_encode(refresh_token));
  }

  /**
   * @brief values.update 用 JSON 本文を生成します。
   * @param values 書き込むセル値一覧です。
   * @return 成功時は JSON 本文、失敗時は GoogleSheetsError です。
   */
  auto build_write_values_request_body(std::span<std::vector<std::string> const> values) -> std::expected<std::string, GoogleSheetsError> {
    auto json = std::string{};
    if (auto const ec = glz::write_json(glz::obj{
                            "majorDimension", "ROWS",
                            "values", values,
                        },
                        json)) {
      return std::unexpected{make_parse_error("failed to build write values request body")};
    }
    return json;
  }

  /**
   * @brief addSheet 用 JSON 本文を生成します。
   * @param title 追加するシートのタイトルです。
   * @return 成功時は JSON 本文、失敗時は GoogleSheetsError です。
   */
  auto build_add_sheet_request_body(std::string_view title) -> std::expected<std::string, GoogleSheetsError> {
    auto json = std::string{};
    if (auto const ec = glz::write_json(glz::obj{
                            "requests",
                            std::vector{glz::obj{
                                "addSheet",
                                glz::obj{"properties", glz::obj{"title", title}},
                            }},
                        },
                        json)) {
      return std::unexpected{make_parse_error("failed to build add sheet request body")};
    }
    return json;
  }

  /**
   * @brief renameSheet 用 JSON 本文を生成します。
   * @param sheet_id 変更対象のシート ID です。
   * @param new_title 新しいタイトルです。
   * @return 成功時は JSON 本文、失敗時は GoogleSheetsError です。
   */
  auto build_rename_sheet_request_body(int sheet_id, std::string_view new_title) -> std::expected<std::string, GoogleSheetsError> {
    auto json = std::string{};
    if (auto const ec = glz::write_json(glz::obj{
                            "requests",
                            std::vector{glz::obj{
                                "updateSheetProperties",
                                glz::obj{
                                    "properties",
                                    glz::obj{
                                        "sheetId", sheet_id,
                                        "title", new_title,
                                    },
                                    "fields", "title",
                                },
                            }},
                        },
                        json)) {
      return std::unexpected{make_parse_error("failed to build rename sheet request body")};
    }
    return json;
  }

  /**
   * @brief deleteSheet 用 JSON 本文を生成します。
   * @param sheet_id 削除対象のシート ID です。
   * @return 成功時は JSON 本文、失敗時は GoogleSheetsError です。
   */
  auto build_delete_sheet_request_body(int sheet_id) -> std::expected<std::string, GoogleSheetsError> {
    auto json = std::string{};
    if (auto const ec = glz::write_json(glz::obj{
                            "requests",
                            std::vector{glz::obj{
                                "deleteSheet",
                                glz::obj{"sheetId", sheet_id},
                            }},
                        },
                        json)) {
      return std::unexpected{make_parse_error("failed to build delete sheet request body")};
    }
    return json;
  }

  /**
   * @brief reorderSheet 用 JSON 本文を生成します。
   * @param sheet_id 変更対象のシート ID です。
   * @param new_index 新しいインデックスです。
   * @return 成功時は JSON 本文、失敗時は GoogleSheetsError です。
   */
  auto build_reorder_sheet_request_body(int sheet_id, int new_index) -> std::expected<std::string, GoogleSheetsError> {
    auto json = std::string{};
    if (auto const ec = glz::write_json(glz::obj{
                            "requests",
                            std::vector{glz::obj{
                                "updateSheetProperties",
                                glz::obj{
                                    "properties",
                                    glz::obj{
                                        "sheetId", sheet_id,
                                        "index", new_index,
                                    },
                                    "fields", "index",
                                },
                            }},
                        },
                        json)) {
      return std::unexpected{make_parse_error("failed to build reorder sheet request body")};
    }
    return json;
  }

  /**
   * @brief repeatCell 用 JSON 本文を生成してセル書式を更新します。
   * @param range 対象のセル範囲です。
   * @param format 適用する書式設定です。
   * @return 成功時は JSON 本文、失敗時は GoogleSheetsError です。
   */
  auto build_update_cell_format_request_body(GridRange const& range, CellFormat const& format) -> std::expected<std::string, GoogleSheetsError> {
    auto payload = BatchUpdateRequestsPayload{};
    auto repeat  = RepeatCellPayload{
         .range = {
             .sheetId          = range.sheet_id,
             .startRowIndex    = range.start_row,
             .endRowIndex      = range.end_row,
             .startColumnIndex = range.start_column,
             .endColumnIndex   = range.end_column,
        },
    };

    auto fields              = std::vector<std::string>{};
    auto user_entered_format = UserEnteredFormatPayload{};

    if (format.background_color) {
      user_entered_format.backgroundColor = ColorPayload{
          .red   = format.background_color->red,
          .green = format.background_color->green,
          .blue  = format.background_color->blue,
      };
      fields.push_back("userEnteredFormat.backgroundColor");
    }

    if (format.foreground_color || format.bold) {
      auto text_format = TextFormatPayload{};
      if (format.foreground_color) {
        text_format.foregroundColor = ColorPayload{
            .red   = format.foreground_color->red,
            .green = format.foreground_color->green,
            .blue  = format.foreground_color->blue,
        };
        fields.push_back("userEnteredFormat.textFormat.foregroundColor");
      }
      if (format.bold) {
        text_format.bold = *format.bold;
        fields.push_back("userEnteredFormat.textFormat.bold");
      }
      user_entered_format.textFormat = text_format;
    }

    if (user_entered_format.backgroundColor || user_entered_format.textFormat) {
      repeat.cell.userEnteredFormat = user_entered_format;
    }

    auto fields_str = std::string{};
    for (size_t i = 0; i < fields.size(); ++i) {
      fields_str += fields[i];
      if (i < fields.size() - 1) {
        fields_str += ",";
      }
    }
    repeat.fields = fields_str;

    payload.requests.push_back(BatchUpdateRequestsPayload::Request{.repeatCell = std::move(repeat)});

    auto json = std::string{};
    if (auto const ec = glz::write_json(payload, json)) {
      return std::unexpected{make_parse_error("failed to build update cell format request body")};
    }
    return json;
  }

  /**
   * @brief 行・列の固定設定用 JSON 本文を生成します。
   * @param sheet_id 対象のシート ID です。
   * @param frozen_row_count 固定する行数です。
   * @param frozen_column_count 固定する列数です。
   * @return 成功時は JSON 本文、失敗時は GoogleSheetsError です。
   */
  auto build_freeze_panes_request_body(int sheet_id, int frozen_row_count, int frozen_column_count) -> std::expected<std::string, GoogleSheetsError> {
    auto payload = BatchUpdateRequestsPayload{};
    payload.requests.push_back(BatchUpdateRequestsPayload::Request{
        .updateSheetProperties =
            UpdateSheetPropertiesPayload{
                .properties =
                    {
                        .sheetId        = sheet_id,
                        .gridProperties = {.frozenRowCount = frozen_row_count, .frozenColumnCount = frozen_column_count},
                    },
                .fields = "gridProperties.frozenRowCount,gridProperties.frozenColumnCount",
            },
    });

    auto json = std::string{};
    if (auto const ec = glz::write_json(payload, json)) {
      return std::unexpected{make_parse_error("failed to build freeze panes request body")};
    }
    return json;
  }

  /**
   * @brief addOverGridImage 用 JSON 本文を生成します。
   * @param image 追加する画像情報です。
   * @return 成功時は JSON 本文、失敗時は GoogleSheetsError です。
   */
  auto build_add_over_grid_image_request_body(OverGridImage const& image) -> std::expected<std::string, GoogleSheetsError> {
    auto payload = AddOverGridImageRequest{
        .requests = {
            {
                .addOverGridImage = {
                    .image = {
                        .location = {
                            .overlayPosition = {
                                .anchorCell = {
                                    .sheetId     = image.position.sheet_id,
                                    .rowIndex    = image.position.row_index,
                                    .columnIndex = image.position.column_index,
                                },
                                .offsetXPixels = image.position.offset_x_pixels,
                                .offsetYPixels = image.position.offset_y_pixels,
                                .widthPixels   = image.position.width_pixels,
                                .heightPixels  = image.position.height_pixels,
                            },
                        },
                        .sourceUri = image.source_uri,
                    },
                },
            },
        },
    };
    auto json = std::string{};
    if (auto const ec = glz::write_json(payload, json)) {
      return std::unexpected{make_parse_error("failed to build add over grid image request body")};
    }
    return json;
  }

  /**
   * @brief spreadsheets.create 用 JSON 本文を生成します。
   * @param title スプレッドシートのタイトルです。
   * @return 成功時は JSON 本文、失敗時は GoogleSheetsError です。
   */
  auto build_create_spreadsheet_request_body(std::string_view title) -> std::expected<std::string, GoogleSheetsError> {
    auto json = std::string{};
    if (auto const ec = glz::write_json(glz::obj{
                            "properties",
                            glz::obj{"title", title},
                        },
                        json)) {
      return std::unexpected{make_parse_error("failed to build create spreadsheet request body")};
    }
    return json;
  }

  /**
   * @brief Google Sheets values API の URL を生成します。
   * @param spreadsheet_id 対象スプレッドシート ID です。
   * @param range A1 形式のレンジです。
   * @param query 任意の追加クエリです。
   * @return 組み立て済み URL です。
   */
  auto build_values_url(std::string_view spreadsheet_id, std::string_view range, std::string_view query) -> std::string {
    auto url = std::string{"https://sheets.googleapis.com/v4/spreadsheets/"};
    url.reserve(url.size() + (spreadsheet_id.size() * 3) + std::string_view{"/values/"}.size() + (range.size() * 3)
                + (query.empty() ? std::size_t{0} : (std::size_t{1} + query.size())));
    percent_encode(spreadsheet_id, url);
    url += "/values/";
    percent_encode(range, url);
    if (!query.empty()) {
      url += '?';
      url += query;
    }
    return url;
  }

  /**
   * @brief Google Sheets spreadsheet API の URL を生成します。
   * @param spreadsheet_id 対象スプレッドシート ID です。
   * @param query 任意の追加クエリです。
   * @return 組み立て済み URL です。
   */
  auto build_spreadsheet_url(std::string_view spreadsheet_id, std::string_view query) -> std::string {
    auto url = std::string{"https://sheets.googleapis.com/v4/spreadsheets/"};
    url.reserve(url.size() + (spreadsheet_id.size() * 3) + (query.empty() ? std::size_t{0} : (std::size_t{1} + query.size())));
    percent_encode(spreadsheet_id, url);
    if (!query.empty()) {
      url += '?';
      url += query;
    }
    return url;
  }

  /**
   * @brief Google Sheets batchUpdate API の URL を生成します。
   * @param spreadsheet_id 対象スプレッドシート ID です。
   * @return 組み立て済み URL です。
   */
  auto build_batch_update_url(std::string_view spreadsheet_id) -> std::string {
    auto url = std::string{"https://sheets.googleapis.com/v4/spreadsheets/"};
    url.reserve(url.size() + (spreadsheet_id.size() * 3) + std::string_view{":batchUpdate"}.size());
    percent_encode(spreadsheet_id, url);
    url += ":batchUpdate";
    return url;
  }

  /**
   * @brief URL にクエリパラメータを 1 つ追加します。
   * @param url 更新対象 URL です。
   * @param key 追加するキーです。
   * @param value 追加する値です。
   * @return 更新後 URL です。
   */
  auto append_query_parameter(std::string url, std::string_view key, std::string_view value) -> std::string {
    auto const has_query = url.find('?') != std::string::npos;
    url.reserve(url.size() + 1 + (key.size() * 3) + 1 + (value.size() * 3));
    url += has_query ? '&' : '?';
    percent_encode(key, url);
    url += '=';
    percent_encode(value, url);
    return url;
  }

  /**
   * @brief サービスアカウント向けの有効トークンを返します。
   * @param auth サービスアカウント設定です。
   * @param shared_state 共有トークン状態です。
   * @param transport HTTP 実行関数です。
   * @param clock 現在時刻関数です。
   * @return 成功時は TokenInfo、失敗時は GoogleSheetsError です。
   */
  auto get_valid_token(ServiceAccountConfig const& auth, std::shared_ptr<ClientSharedState> const& shared_state, TransportFunction const& transport, ClockFunction const& clock)
      -> std::expected<TokenInfo, GoogleSheetsError> {
    return get_or_refresh_token(shared_state, clock, [&] { return fetch_service_account_token(auth, shared_state, transport, clock); });
  }

  /**
   * @brief User OAuth2 向けの有効トークンを返します。
   * @param auth OAuth2 認証設定です。
   * @param shared_state 共有トークン状態です。
   * @param transport HTTP 実行関数です。
   * @param clock 現在時刻関数です。
   * @return 成功時は TokenInfo、失敗時は GoogleSheetsError です。
   */
  auto get_valid_token(UserOAuth2Auth& auth, std::shared_ptr<ClientSharedState> const& shared_state, TransportFunction const& transport, ClockFunction const& clock)
      -> std::expected<TokenInfo, GoogleSheetsError> {
    return get_or_refresh_token(shared_state, clock, [&] { return fetch_user_oauth_token(auth, shared_state, transport, clock, false); });
  }

  /**
   * @brief 401 後に User OAuth2 の refresh を強制実行します。
   * @param auth OAuth2 認証設定です。
   * @param shared_state 共有トークン状態です。
   * @param transport HTTP 実行関数です。
   * @param clock 現在時刻関数です。
   * @param failed_access_token 失効していたアクセストークンです。
   * @return 成功時は新しい TokenInfo、失敗時は GoogleSheetsError です。
   */
  auto force_refresh_token(UserOAuth2Auth& auth, std::shared_ptr<ClientSharedState> const& shared_state, TransportFunction const& transport, ClockFunction const& clock,
                           std::string_view failed_access_token) -> std::expected<TokenInfo, GoogleSheetsError> {
    return force_refresh(shared_state, clock, failed_access_token, [&] { return fetch_user_oauth_token(auth, shared_state, transport, clock, true); });
  }

  /**
   * @brief テスト用に依存を差し替えたクライアントを構築します。
   * @param config サービスアカウント設定です。
   * @param transport テスト用 transport です。
   * @param clock 任意の疑似時刻関数です。
   * @return テスト向け GoogleSheetsClient です。
   */
  auto make_google_sheets_client_for_test(ServiceAccountConfig config, TransportFunction transport, ClockFunction clock) -> GoogleSheetsClient {
    return GoogleSheetsClient{
        std::move(config),
        std::move(transport),
        std::move(clock),
    };
  }

}  // namespace detail

}  // namespace gsheetpp
