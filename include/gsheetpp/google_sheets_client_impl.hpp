#pragma once

/**
 * @file google_sheets_client_impl.hpp
 * @brief BasicGoogleSheetsClient のテンプレート実装を定義します。
 *
 * このヘッダは google_sheets_client.hpp の末尾から #include されます。
 * 利用者が直接 include する必要はありません。
 */

#include "gsheetpp/google_sheets_client.hpp"

#include <future>
#include <type_traits>
#include <utility>

namespace gsheetpp {

namespace detail {

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
   * @brief batchUpdate 応答から追加されたシートの情報を抽出します。
   * @param json Google Sheets API 応答 JSON です。
   * @return 成功時は SheetMetadata、失敗時は GoogleSheetsError です。
   */
  auto parse_add_sheet_response(std::string_view json) -> std::expected<SheetMetadata, GoogleSheetsError>;

  /**
   * @brief spreadsheets.get 応答を SheetMetadata のリストに変換します。
   * @param json Google Sheets API 応答 JSON です。
   * @return 成功時は SheetMetadata のリスト、失敗時は GoogleSheetsError です。
   */
  auto parse_get_sheets_response(std::string_view json) -> std::expected<std::vector<SheetMetadata>, GoogleSheetsError>;

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
   * @return 成功時は JSON 文字列、失敗時は GoogleSheetsError です。
   */
  auto build_write_values_request_body(std::span<std::vector<std::string> const> values) -> std::expected<std::string, GoogleSheetsError>;

  /**
   * @brief addSheet 用の JSON 本文を組み立てます。
   * @param title 追加するシートのタイトルです。
   * @return 成功時は JSON 文字列、失敗時は GoogleSheetsError です。
   */
  auto build_add_sheet_request_body(std::string_view title) -> std::expected<std::string, GoogleSheetsError>;

  /**
   * @brief renameSheet 用の JSON 本文を組み立てます。
   * @param sheet_id 変更対象のシート ID です。
   * @param new_title 新しいタイトルです。
   * @return 成功時は JSON 文字列、失敗時は GoogleSheetsError です。
   */
  auto build_rename_sheet_request_body(int sheet_id, std::string_view new_title) -> std::expected<std::string, GoogleSheetsError>;

  /**
   * @brief deleteSheet 用の JSON 本文を組み立てます。
   * @param sheet_id 削除対象のシート ID です。
   * @return 成功時は JSON 文字列、失敗時は GoogleSheetsError です。
   */
  auto build_delete_sheet_request_body(int sheet_id) -> std::expected<std::string, GoogleSheetsError>;

  /**
   * @brief reorderSheet 用の JSON 本文を組み立てます。
   * @param sheet_id 変更対象のシート ID です。
   * @param new_index 新しいインデックスです。
   * @return 成功時は JSON 文字列、失敗時は GoogleSheetsError です。
   */
  auto build_reorder_sheet_request_body(int sheet_id, int new_index) -> std::expected<std::string, GoogleSheetsError>;

  /**
   * @brief repeatCell 用の JSON 本文を組み立ててセル書式を更新します。
   * @param range 対象のセル範囲です。
   * @param format 適用する書式設定です。
   * @return 成功時は JSON 文字列、失敗時は GoogleSheetsError です。
   */
  auto build_update_cell_format_request_body(GridRange const& range, CellFormat const& format) -> std::expected<std::string, GoogleSheetsError>;

  /**
   * @brief repeatCell 用の JSON 本文を組み立てて文字スタイルを更新します。
   * @param range 対象のセル範囲です。
   * @param style 適用する文字スタイル設定です。
   * @return 成功時は JSON 文字列、失敗時は GoogleSheetsError です。
   */
  auto build_set_text_style_request_body(GridRange const& range, TextStyle const& style) -> std::expected<std::string, GoogleSheetsError>;

  /**
   * @brief repeatCell 用の JSON 本文を組み立ててセル配置を更新します。
   * @param range 対象のセル範囲です。
   * @param horizontal 水平配置です。
   * @param vertical 垂直配置です。
   * @return 成功時は JSON 文字列、失敗時は GoogleSheetsError です。
   */
  auto build_set_cell_alignment_request_body(GridRange const& range, HorizontalAlign horizontal, VerticalAlign vertical) -> std::expected<std::string, GoogleSheetsError>;

  /**
   * @brief repeatCell 用の JSON 本文を組み立ててセル配置を更新します。
   * @param range 対象のセル範囲です。
   * @param horizontal 水平配置文字列です。
   * @param vertical 垂直配置文字列です。
   * @return 成功時は JSON 文字列、失敗時は GoogleSheetsError です。
   */
  auto build_set_cell_alignment_request_body(GridRange const& range, std::string_view horizontal, std::string_view vertical)
      -> std::expected<std::string, GoogleSheetsError>;

  /**
   * @brief 行・列の固定設定用の JSON 本文を組み立てます。
   * @param sheet_id 対象のシート ID です。
   * @param frozen_row_count 固定する行数です。
   * @param frozen_column_count 固定する列数です。
   * @return 成功時は JSON 文字列、失敗時は GoogleSheetsError です。
   */
  auto build_freeze_panes_request_body(int sheet_id, int frozen_row_count, int frozen_column_count) -> std::expected<std::string, GoogleSheetsError>;

  /**
   * @brief addOverGridImage 用の JSON 本文を組み立てます。
   * @param image 追加する画像情報です。
   * @return 成功時は JSON 文字列、失敗時は GoogleSheetsError です。
   */
  auto build_add_over_grid_image_request_body(OverGridImage const& image) -> std::expected<std::string, GoogleSheetsError>;

  /**
   * @brief mergeCells 用の JSON 本文を組み立てます。
   * @param range 結合対象のセル範囲です。
   * @param type 結合方式です。
   * @return 成功時は JSON 文字列、失敗時は GoogleSheetsError です。
   */
  auto build_merge_cells_request_body(GridRange const& range, MergeType type) -> std::expected<std::string, GoogleSheetsError>;

  /**
   * @brief unmergeCells 用の JSON 本文を組み立てます。
   * @param range 結合解除対象のセル範囲です。
   * @return 成功時は JSON 文字列、失敗時は GoogleSheetsError です。
   */
  auto build_unmerge_cells_request_body(GridRange const& range) -> std::expected<std::string, GoogleSheetsError>;

  /**
   * @brief spreadsheets.create 用の JSON 本文を組み立てます。
   * @param title スプレッドシートのタイトルです。
   * @return 成功時は JSON 文字列、失敗時は GoogleSheetsError です。
   */
  auto build_create_spreadsheet_request_body(std::string_view title) -> std::expected<std::string, GoogleSheetsError>;

  /**
   * @brief spreadsheets.create 応答から spreadsheetId を抽出します。
   * @param json 応答 JSON です。
   * @return 成功時は spreadsheetId、失敗時は GoogleSheetsError です。
   */
  auto parse_create_spreadsheet_response(std::string_view json) -> std::expected<std::string, GoogleSheetsError>;

  /**
   * @brief Drive API files.list 応答を DriveFile のリストに変換します。
   * @param json 応答 JSON です。
   * @return 成功時は DriveFile のリスト、失敗時は GoogleSheetsError です。
   */
  auto parse_drive_files_response(std::string_view json) -> std::expected<std::vector<DriveFile>, GoogleSheetsError>;

  /**
   * @brief values API エンドポイント URL を組み立てます。
   * @param spreadsheet_id 対象スプレッドシート ID です。
   * @param range A1 形式のレンジです。
   * @param query 付加するクエリ文字列です。
   * @note query は呼び出し側で percent-encode 済みの完全なクエリ文字列（例: "valueInputOption=RAW"）を
   *       渡すこと。この関数はエンコードを行いません。
   *       キーと値を個別に渡したい場合は append_query_parameter を使用してください。
   * @return 組み立て済み URL です。
   */
  auto build_values_url(std::string_view spreadsheet_id, std::string_view range, std::string_view query = {}) -> std::string;

  /**
   * @brief spreadsheet API エンドポイント URL を組み立てます。
   * @param spreadsheet_id 対象スプレッドシート ID です。
   * @param query 付加するクエリ文字列です。
   * @note query は呼び出し側で percent-encode 済みの完全なクエリ文字列（例: "fields=sheets"）を
   *       渡すこと。この関数はエンコードを行いません。
   *       キーと値を個別に渡したい場合は append_query_parameter を使用してください。
   * @return 組み立て済み URL です。
   */
  auto build_spreadsheet_url(std::string_view spreadsheet_id, std::string_view query = {}) -> std::string;

  /**
   * @brief batchUpdate API エンドポイント URL を組み立てます。
   * @param spreadsheet_id 対象スプレッドシート ID です。
   * @return 組み立て済み URL です。
   */
  auto build_batch_update_url(std::string_view spreadsheet_id) -> std::string;

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

/**
 * @brief BasicGoogleSheetsClient の実装詳細を保持する PIMPL 構造体です。
 *
 * 公開ヘッダから内部状態 (TransportFunction / ClockFunction / ClientSharedState
 * など) を追い出し、利用者の翻訳単位に対する ABI 影響を抑えます。
 */
template <Authenticator Auth>
struct BasicGoogleSheetsClient<Auth>::Impl {
  std::shared_ptr<Auth>                      auth{};          ///< 認証設定本体です。
  detail::TransportFunction                  transport{};     ///< 実際の HTTP 実行手段です。
  detail::ClockFunction                      clock{};         ///< 現在時刻供給元です。
  std::shared_ptr<detail::ClientSharedState> shared_state{};  ///< 複製間共有キャッシュです。
};

template <Authenticator Auth>
template <class Fn>
/**
 * @brief std::async の起動ポリシーを async 固定に統一する内部ヘルパです。
 *
 * std::async(std::launch::async | std::launch::deferred, ...) を直接使うと
 * 実装によっては .get() まで実行が遅延する経路が選ばれてしまうため、
 * ライブラリ全体の方針として「即時に別スレッドへ逃がす」挙動を保証します。
 */
auto BasicGoogleSheetsClient<Auth>::run_async(Fn&& fn) -> std::future<std::invoke_result_t<Fn>> {
  return std::async(std::launch::async, std::forward<Fn>(fn));
}

template <Authenticator Auth>
/**
 * @brief コピーコンストラクタです。auth / transport / clock / shared_state を
 *        浅く共有して複製します。共有 token キャッシュはコピー後も一貫しています。
 */
BasicGoogleSheetsClient<Auth>::BasicGoogleSheetsClient(BasicGoogleSheetsClient const& other) : impl_{std::make_unique<Impl>(*other.impl_)} {}

template <Authenticator Auth>
/**
 * @brief ムーブコンストラクタです。
 */
BasicGoogleSheetsClient<Auth>::BasicGoogleSheetsClient(BasicGoogleSheetsClient&& other) noexcept = default;

template <Authenticator Auth>
/**
 * @brief コピー代入演算子です。
 */
auto BasicGoogleSheetsClient<Auth>::operator=(BasicGoogleSheetsClient const& other) -> BasicGoogleSheetsClient& {
  if (this != &other) {
    impl_ = std::make_unique<Impl>(*other.impl_);
  }
  return *this;
}

template <Authenticator Auth>
/**
 * @brief ムーブ代入演算子です。
 */
auto BasicGoogleSheetsClient<Auth>::operator=(BasicGoogleSheetsClient&& other) noexcept -> BasicGoogleSheetsClient& = default;

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
    : impl_{std::make_unique<Impl>()} {
  impl_->auth         = std::make_shared<Auth>(std::move(auth));
  impl_->transport    = std::move(transport);
  impl_->clock        = std::move(clock);
  impl_->shared_state = std::make_shared<detail::ClientSharedState>();
}

/**
 * @brief PIMPL 構造体が見えて初めて default 化できるデストラクタを定義します。
 */
template <Authenticator Auth>
BasicGoogleSheetsClient<Auth>::~BasicGoogleSheetsClient() = default;

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
      impl_->transport,
      impl_->clock,
  };
}

template <Authenticator Auth>
/**
 * @brief 認証方式に応じてアクセストークン取得を非同期実行します。
 * @return 成功時は TokenInfo、失敗時は GoogleSheetsError を返す future です。
 */
auto BasicGoogleSheetsClient<Auth>::authenticate_async() -> std::future<std::expected<TokenInfo, GoogleSheetsError>> {
  return run_async([client = *this]() mutable -> std::expected<TokenInfo, GoogleSheetsError> {
    if constexpr (std::same_as<Auth, ApiKeyAuth>) {
      return TokenInfo{
          .access_token       = "",
          .token_type         = "ApiKey",
          .expires_in_seconds = 0,
      };
    } else if constexpr (std::same_as<Auth, ServiceAccountConfig>) {
      return detail::get_valid_token(*client.impl_->auth, client.impl_->shared_state, client.impl_->transport, client.impl_->clock);
    } else {
      return detail::get_valid_token(*client.impl_->auth, client.impl_->shared_state, client.impl_->transport, client.impl_->clock);
    }
  });
}

template <Authenticator Auth>
/**
 * @brief spreadsheets.get を非同期実行してシート一覧を取得します。
 * @param spreadsheet_id 対象スプレッドシート ID です。
 * @return 成功時は SheetMetadata のリスト、失敗時は GoogleSheetsError を返す future です。
 */
auto BasicGoogleSheetsClient<Auth>::get_sheets_async(std::string_view spreadsheet_id) -> std::future<std::expected<std::vector<SheetMetadata>, GoogleSheetsError>> {
  auto const spreadsheet = std::string{spreadsheet_id};
  return run_async([client = *this, spreadsheet]() mutable -> std::expected<std::vector<SheetMetadata>, GoogleSheetsError> {
    auto url     = detail::build_spreadsheet_url(spreadsheet);
    url          = detail::append_query_parameter(std::move(url), "fields", "sheets(properties(title,sheetId))");
    auto request = detail::HttpRequest{
        .method = "GET",
        .url    = std::move(url),
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

    return detail::parse_get_sheets_response(response->body);
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
  return run_async([client = *this, spreadsheet, read_range]() mutable -> std::expected<ReadValuesResult, GoogleSheetsError> {
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
  return run_async([client = *this, spreadsheet, write_range, values]() mutable -> std::expected<WriteValuesResult, GoogleSheetsError> {
    auto body = detail::build_write_values_request_body(values);
    if (!body) {
      return std::unexpected{body.error()};
    }
    auto request = detail::HttpRequest{
        .method  = "PUT",
        .url     = detail::build_values_url(spreadsheet, write_range, "valueInputOption=RAW"),
        .headers = {"Content-Type: application/json"},
        .body    = *std::move(body),
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
 * @brief シート内の全データを非同期で取得します。
 * @param spreadsheet_id 対象スプレッドシート ID です。
 * @param sheet_name 対象のシート名です。
 * @return 成功時は ReadValuesResult、失敗時は GoogleSheetsError を返す future です。
 */
auto BasicGoogleSheetsClient<Auth>::get_used_range_async(std::string_view spreadsheet_id, std::string_view sheet_name) -> std::future<std::expected<ReadValuesResult, GoogleSheetsError>> {
  return read_values_async(spreadsheet_id, sheet_name);
}

template <Authenticator Auth>
/**
 * @brief spreadsheets.batchUpdate を非同期実行してシートを追加します。
 * @param spreadsheet_id 対象スプレッドシート ID です。
 * @param title 追加するシートのタイトルです。
 * @return 成功時は追加されたシートの SheetMetadata、失敗時は GoogleSheetsError を返す future です。
 */
auto BasicGoogleSheetsClient<Auth>::add_sheet_async(std::string_view spreadsheet_id, std::string_view title) -> std::future<std::expected<SheetMetadata, GoogleSheetsError>> {
  auto const spreadsheet = std::string{spreadsheet_id};
  auto const sheet_title = std::string{title};
  return run_async([client = *this, spreadsheet, sheet_title]() mutable -> std::expected<SheetMetadata, GoogleSheetsError> {
    auto body = detail::build_add_sheet_request_body(sheet_title);
    if (!body) {
      return std::unexpected{body.error()};
    }
    auto request = detail::HttpRequest{
        .method  = "POST",
        .url     = detail::build_batch_update_url(spreadsheet),
        .headers = {"Content-Type: application/json"},
        .body    = *std::move(body),
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
      return std::unexpected{GoogleSheetsError{
          .kind          = GoogleSheetsErrorKind::api_response,
          .message       = (!api_error || api_error->empty()) ? "google api request failed" : *api_error,
          .http_status   = response->status_code,
          .response_body = response->body,
      }};
    }

    return detail::parse_add_sheet_response(response->body);
  });
}

template <Authenticator Auth>
/**
 * @brief spreadsheets.batchUpdate を非同期実行してシート名を変更します。
 * @param spreadsheet_id 対象スプレッドシート ID です。
 * @param sheet_id 変更対象のシート ID です。
 * @param new_title 新しいタイトルです。
 * @return 成功時は void、失敗時は GoogleSheetsError を返す future です。
 */
auto BasicGoogleSheetsClient<Auth>::rename_sheet_async(std::string_view spreadsheet_id, int sheet_id, std::string_view new_title) -> std::future<std::expected<void, GoogleSheetsError>> {
  auto const spreadsheet = std::string{spreadsheet_id};
  auto const title       = std::string{new_title};
  return run_async([client = *this, spreadsheet, sheet_id, title]() mutable -> std::expected<void, GoogleSheetsError> {
    auto body = detail::build_rename_sheet_request_body(sheet_id, title);
    if (!body) {
      return std::unexpected{body.error()};
    }
    auto request = detail::HttpRequest{
        .method  = "POST",
        .url     = detail::build_batch_update_url(spreadsheet),
        .headers = {"Content-Type: application/json"},
        .body    = *std::move(body),
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
      return std::unexpected{GoogleSheetsError{
          .kind          = GoogleSheetsErrorKind::api_response,
          .message       = (!api_error || api_error->empty()) ? "google api request failed" : *api_error,
          .http_status   = response->status_code,
          .response_body = response->body,
      }};
    }

    return {};
  });
}

template <Authenticator Auth>
/**
 * @brief spreadsheets.batchUpdate を非同期実行してシートを削除します。
 * @param spreadsheet_id 対象スプレッドシート ID です。
 * @param sheet_id 削除対象のシート ID です。
 * @return 成功時は void、失敗時は GoogleSheetsError を返す future です。
 */
auto BasicGoogleSheetsClient<Auth>::delete_sheet_async(std::string_view spreadsheet_id, int sheet_id) -> std::future<std::expected<void, GoogleSheetsError>> {
  auto const spreadsheet = std::string{spreadsheet_id};
  return run_async([client = *this, spreadsheet, sheet_id]() mutable -> std::expected<void, GoogleSheetsError> {
    auto body = detail::build_delete_sheet_request_body(sheet_id);
    if (!body) {
      return std::unexpected{body.error()};
    }
    auto request = detail::HttpRequest{
        .method  = "POST",
        .url     = detail::build_batch_update_url(spreadsheet),
        .headers = {"Content-Type: application/json"},
        .body    = *std::move(body),
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
      return std::unexpected{GoogleSheetsError{
          .kind          = GoogleSheetsErrorKind::api_response,
          .message       = (!api_error || api_error->empty()) ? "google api request failed" : *api_error,
          .http_status   = response->status_code,
          .response_body = response->body,
      }};
    }

    return {};
  });
}

template <Authenticator Auth>
/**
 * @brief spreadsheets.batchUpdate を非同期実行してシート順序を変更します。
 * @param spreadsheet_id 対象スプレッドシート ID です。
 * @param sheet_id 変更対象のシート ID です。
 * @param new_index 新しいインデックスです。
 * @return 成功時は void、失敗時は GoogleSheetsError を返す future です。
 */
auto BasicGoogleSheetsClient<Auth>::reorder_sheet_async(std::string_view spreadsheet_id, int sheet_id, int new_index) -> std::future<std::expected<void, GoogleSheetsError>> {
  auto const spreadsheet = std::string{spreadsheet_id};
  return run_async([client = *this, spreadsheet, sheet_id, new_index]() mutable -> std::expected<void, GoogleSheetsError> {
    auto body = detail::build_reorder_sheet_request_body(sheet_id, new_index);
    if (!body) {
      return std::unexpected{body.error()};
    }
    auto request = detail::HttpRequest{
        .method  = "POST",
        .url     = detail::build_batch_update_url(spreadsheet),
        .headers = {"Content-Type: application/json"},
        .body    = *std::move(body),
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
      return std::unexpected{GoogleSheetsError{
          .kind          = GoogleSheetsErrorKind::api_response,
          .message       = (!api_error || api_error->empty()) ? "google api request failed" : *api_error,
          .http_status   = response->status_code,
          .response_body = response->body,
      }};
    }

    return {};
  });
}

template <Authenticator Auth>
/**
 * @brief spreadsheets.batchUpdate を非同期実行してセル書式を更新します。
 * @param spreadsheet_id 対象スプレッドシート ID です。
 * @param range 対象のセル範囲です。
 * @param format 適用する書式設定です。
 * @return 成功時は void、失敗時は GoogleSheetsError を返す future です。
 */
auto BasicGoogleSheetsClient<Auth>::update_cell_format_async(std::string_view spreadsheet_id, GridRange const& range, CellFormat const& format)
    -> std::future<std::expected<void, GoogleSheetsError>> {
  auto const spreadsheet = std::string{spreadsheet_id};
  return run_async([client = *this, spreadsheet, range, format]() mutable -> std::expected<void, GoogleSheetsError> {
    auto body = detail::build_update_cell_format_request_body(range, format);
    if (!body) {
      return std::unexpected{body.error()};
    }
    auto request = detail::HttpRequest{
        .method  = "POST",
        .url     = detail::build_batch_update_url(spreadsheet),
        .headers = {"Content-Type: application/json"},
        .body    = *std::move(body),
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
      return std::unexpected{GoogleSheetsError{
          .kind          = GoogleSheetsErrorKind::api_response,
          .message       = (!api_error || api_error->empty()) ? "google api request failed" : *api_error,
          .http_status   = response->status_code,
          .response_body = response->body,
      }};
    }

    return {};
  });
}

template <Authenticator Auth>
/**
 * @brief spreadsheets.batchUpdate を非同期実行して文字スタイルを更新します。
 * @param spreadsheet_id 対象スプレッドシート ID です。
 * @param range 対象のセル範囲です。
 * @param style 適用する文字スタイル設定です。
 * @return 成功時は void、失敗時は GoogleSheetsError を返す future です。
 */
auto BasicGoogleSheetsClient<Auth>::set_text_style_async(std::string_view spreadsheet_id, GridRange const& range, TextStyle const& style)
    -> std::future<std::expected<void, GoogleSheetsError>> {
  auto const spreadsheet = std::string{spreadsheet_id};
  return run_async([client = *this, spreadsheet, range, style]() mutable -> std::expected<void, GoogleSheetsError> {
    auto body = detail::build_set_text_style_request_body(range, style);
    if (!body) {
      return std::unexpected{body.error()};
    }
    auto request = detail::HttpRequest{
        .method  = "POST",
        .url     = detail::build_batch_update_url(spreadsheet),
        .headers = {"Content-Type: application/json"},
        .body    = *std::move(body),
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
      return std::unexpected{GoogleSheetsError{
          .kind          = GoogleSheetsErrorKind::api_response,
          .message       = (!api_error || api_error->empty()) ? "google api request failed" : *api_error,
          .http_status   = response->status_code,
          .response_body = response->body,
      }};
    }

    return {};
  });
}

template <Authenticator Auth>
/**
 * @brief spreadsheets.batchUpdate を非同期実行してセル配置を更新します。
 * @param spreadsheet_id 対象スプレッドシート ID です。
 * @param range 対象のセル範囲です。
 * @param horizontal 水平配置です。
 * @param vertical 垂直配置です。
 * @return 成功時は void、失敗時は GoogleSheetsError を返す future です。
 */
auto BasicGoogleSheetsClient<Auth>::set_cell_alignment_async(
    std::string_view spreadsheet_id,
    GridRange const& range,
    HorizontalAlign horizontal,
    VerticalAlign vertical) -> std::future<std::expected<void, GoogleSheetsError>> {
  auto const spreadsheet = std::string{spreadsheet_id};
  return run_async([client = *this, spreadsheet, range, horizontal, vertical]() mutable -> std::expected<void, GoogleSheetsError> {
    auto body = detail::build_set_cell_alignment_request_body(range, horizontal, vertical);
    if (!body) {
      return std::unexpected{body.error()};
    }
    auto request = detail::HttpRequest{
        .method  = "POST",
        .url     = detail::build_batch_update_url(spreadsheet),
        .headers = {"Content-Type: application/json"},
        .body    = *std::move(body),
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
      return std::unexpected{GoogleSheetsError{
          .kind          = GoogleSheetsErrorKind::api_response,
          .message       = (!api_error || api_error->empty()) ? "google api request failed" : *api_error,
          .http_status   = response->status_code,
          .response_body = response->body,
      }};
    }

    return {};
  });
}

template <Authenticator Auth>
/**
 * @brief spreadsheets.batchUpdate を非同期実行してセル配置を更新します。
 * @param spreadsheet_id 対象スプレッドシート ID です。
 * @param range 対象のセル範囲です。
 * @param horizontal 水平配置文字列です。
 * @param vertical 垂直配置文字列です。
 * @return 成功時は void、失敗時は GoogleSheetsError を返す future です。
 */
auto BasicGoogleSheetsClient<Auth>::set_cell_alignment_async(
    std::string_view spreadsheet_id,
    GridRange const& range,
    std::string_view horizontal,
    std::string_view vertical) -> std::future<std::expected<void, GoogleSheetsError>> {
  auto const spreadsheet      = std::string{spreadsheet_id};
  auto const horizontal_value = std::string{horizontal};
  auto const vertical_value   = std::string{vertical};
  return run_async(
      [client = *this, spreadsheet, range, horizontal_value, vertical_value]() mutable -> std::expected<void, GoogleSheetsError> {
        auto body = detail::build_set_cell_alignment_request_body(range, horizontal_value, vertical_value);
        if (!body) {
          return std::unexpected{body.error()};
        }
        auto request = detail::HttpRequest{
            .method  = "POST",
            .url     = detail::build_batch_update_url(spreadsheet),
            .headers = {"Content-Type: application/json"},
            .body    = *std::move(body),
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
          return std::unexpected{GoogleSheetsError{
              .kind          = GoogleSheetsErrorKind::api_response,
              .message       = (!api_error || api_error->empty()) ? "google api request failed" : *api_error,
              .http_status   = response->status_code,
              .response_body = response->body,
          }};
        }

        return {};
      });
}

template <Authenticator Auth>
/**
 * @brief spreadsheets.batchUpdate を非同期実行してシートの固定設定を更新します。
 * @param spreadsheet_id 対象スプレッドシート ID です。
 * @param sheet_id 対象のシート ID です。
 * @param frozen_row_count 固定する行数です。
 * @param frozen_column_count 固定する列数です。
 * @return 成功時は void、失敗時は GoogleSheetsError を返す future です。
 */
auto BasicGoogleSheetsClient<Auth>::freeze_panes_async(std::string_view spreadsheet_id, int sheet_id, int frozen_row_count, int frozen_column_count)
    -> std::future<std::expected<void, GoogleSheetsError>> {
  auto const spreadsheet = std::string{spreadsheet_id};
  return run_async([client = *this, spreadsheet, sheet_id, frozen_row_count, frozen_column_count]() mutable -> std::expected<void, GoogleSheetsError> {
    auto body = detail::build_freeze_panes_request_body(sheet_id, frozen_row_count, frozen_column_count);
    if (!body) {
      return std::unexpected{body.error()};
    }
    auto request = detail::HttpRequest{
        .method  = "POST",
        .url     = detail::build_batch_update_url(spreadsheet),
        .headers = {"Content-Type: application/json"},
        .body    = *std::move(body),
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
      return std::unexpected{GoogleSheetsError{
          .kind          = GoogleSheetsErrorKind::api_response,
          .message       = (!api_error || api_error->empty()) ? "google api request failed" : *api_error,
          .http_status   = response->status_code,
          .response_body = response->body,
      }};
    }

    return {};
  });
}

template <Authenticator Auth>
/**
 * @brief spreadsheets.batchUpdate を非同期実行して画像を追加します。
 * @param spreadsheet_id 対象スプレッドシート ID です。
 * @param image 追加する画像情報です。
 * @return 成功時は void、失敗時は GoogleSheetsError を返す future です。
 */
auto BasicGoogleSheetsClient<Auth>::add_over_grid_image_async(std::string_view spreadsheet_id, OverGridImage const& image)
    -> std::future<std::expected<void, GoogleSheetsError>> {
  auto const spreadsheet = std::string{spreadsheet_id};
  return run_async([client = *this, spreadsheet, image]() mutable -> std::expected<void, GoogleSheetsError> {
    auto body = detail::build_add_over_grid_image_request_body(image);
    if (!body) {
      return std::unexpected{body.error()};
    }
    auto request = detail::HttpRequest{
        .method  = "POST",
        .url     = detail::build_batch_update_url(spreadsheet),
        .headers = {"Content-Type: application/json"},
        .body    = *std::move(body),
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
      return std::unexpected{GoogleSheetsError{
          .kind          = GoogleSheetsErrorKind::api_response,
          .message       = (!api_error || api_error->empty()) ? "google api request failed" : *api_error,
          .http_status   = response->status_code,
          .response_body = response->body,
      }};
    }

    return {};
  });
}

template <Authenticator Auth>
/**
 * @brief spreadsheets.batchUpdate を非同期実行してセルを結合します。
 * @param spreadsheet_id 対象スプレッドシート ID です。
 * @param range 結合対象のセル範囲です。
 * @param type 結合方式です。
 * @return 成功時は void、失敗時は GoogleSheetsError を返す future です。
 */
auto BasicGoogleSheetsClient<Auth>::merge_cells_async(std::string_view spreadsheet_id, GridRange const& range, MergeType type)
    -> std::future<std::expected<void, GoogleSheetsError>> {
  auto const spreadsheet = std::string{spreadsheet_id};
  return run_async([client = *this, spreadsheet, range, type]() mutable -> std::expected<void, GoogleSheetsError> {
    auto body = detail::build_merge_cells_request_body(range, type);
    if (!body) {
      return std::unexpected{body.error()};
    }
    auto request = detail::HttpRequest{
        .method  = "POST",
        .url     = detail::build_batch_update_url(spreadsheet),
        .headers = {"Content-Type: application/json"},
        .body    = *std::move(body),
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
      return std::unexpected{GoogleSheetsError{
          .kind          = GoogleSheetsErrorKind::api_response,
          .message       = (!api_error || api_error->empty()) ? "google api request failed" : *api_error,
          .http_status   = response->status_code,
          .response_body = response->body,
      }};
    }

    return {};
  });
}

template <Authenticator Auth>
/**
 * @brief spreadsheets.batchUpdate を非同期実行してセルの結合を解除します。
 * @param spreadsheet_id 対象スプレッドシート ID です。
 * @param range 結合解除対象のセル範囲です。
 * @return 成功時は void、失敗時は GoogleSheetsError を返す future です。
 */
auto BasicGoogleSheetsClient<Auth>::unmerge_cells_async(std::string_view spreadsheet_id, GridRange const& range)
    -> std::future<std::expected<void, GoogleSheetsError>> {
  auto const spreadsheet = std::string{spreadsheet_id};
  return run_async([client = *this, spreadsheet, range]() mutable -> std::expected<void, GoogleSheetsError> {
    auto body = detail::build_unmerge_cells_request_body(range);
    if (!body) {
      return std::unexpected{body.error()};
    }
    auto request = detail::HttpRequest{
        .method  = "POST",
        .url     = detail::build_batch_update_url(spreadsheet),
        .headers = {"Content-Type: application/json"},
        .body    = *std::move(body),
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
      return std::unexpected{GoogleSheetsError{
          .kind          = GoogleSheetsErrorKind::api_response,
          .message       = (!api_error || api_error->empty()) ? "google api request failed" : *api_error,
          .http_status   = response->status_code,
          .response_body = response->body,
      }};
    }

    return {};
  });
}

template <Authenticator Auth>
/**
 * @brief spreadsheets.create を非同期実行して新しいスプレッドシートを作成します。
 * @param title スプレッドシートのタイトルです。
 * @return 成功時は作成されたスプレッドシートの ID、失敗時は GoogleSheetsError を返す future です。
 */
auto BasicGoogleSheetsClient<Auth>::create_new_spreadsheet_async(std::string_view title) -> std::future<std::expected<std::string, GoogleSheetsError>> {
  auto const spreadsheet_title = std::string{title};
  return run_async([client = *this, spreadsheet_title]() mutable -> std::expected<std::string, GoogleSheetsError> {
    auto body = detail::build_create_spreadsheet_request_body(spreadsheet_title);
    if (!body) {
      return std::unexpected{body.error()};
    }
    auto request = detail::HttpRequest{
        .method  = "POST",
        .url     = "https://sheets.googleapis.com/v4/spreadsheets",
        .headers = {"Content-Type: application/json"},
        .body    = *std::move(body),
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
      return std::unexpected{GoogleSheetsError{
          .kind          = GoogleSheetsErrorKind::api_response,
          .message       = (!api_error || api_error->empty()) ? "google api request failed" : *api_error,
          .http_status   = response->status_code,
          .response_body = response->body,
      }};
    }

    return detail::parse_create_spreadsheet_response(response->body);
  });
}

template <Authenticator Auth>
/**
 * @brief Drive API を非同期実行してルート直下のスプレッドシートを一覧取得します。
 * @return 成功時は DriveFile のリスト、失敗時は GoogleSheetsError を返す future です。
 */
auto BasicGoogleSheetsClient<Auth>::fetch_root_spreadsheets_async() -> std::future<std::expected<std::vector<DriveFile>, GoogleSheetsError>> {
  return run_async([client = *this]() mutable -> std::expected<std::vector<DriveFile>, GoogleSheetsError> {
    auto const q   = "mimeType = 'application/vnd.google-apps.spreadsheet' and 'root' in parents and trashed = false";
    auto       url = std::string{"https://www.googleapis.com/drive/v3/files"};
    url            = detail::append_query_parameter(std::move(url), "q", q);
    url            = detail::append_query_parameter(std::move(url), "fields", "files(id,name)");

    auto request = detail::HttpRequest{
        .method = "GET",
        .url    = std::move(url),
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
      return std::unexpected{GoogleSheetsError{
          .kind          = GoogleSheetsErrorKind::api_response,
          .message       = (!api_error || api_error->empty()) ? "google api request failed" : *api_error,
          .http_status   = response->status_code,
          .response_body = response->body,
      }};
    }

    return detail::parse_drive_files_response(response->body);
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
    if (impl_->auth->api_key.empty()) {
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
    request.url = detail::append_query_parameter(std::move(request.url), "key", impl_->auth->api_key);
    return {};
  } else if constexpr (std::same_as<Auth, ServiceAccountConfig>) {
    auto token = detail::get_valid_token(*impl_->auth, impl_->shared_state, impl_->transport, impl_->clock);
    if (!token) {
      return std::unexpected{token.error()};
    }
    request.headers.push_back("Authorization: Bearer " + token->access_token);
    return {};
  } else {
    auto token = detail::get_valid_token(*impl_->auth, impl_->shared_state, impl_->transport, impl_->clock);
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
    if (!impl_->transport) {
      return std::unexpected{GoogleSheetsError{
          .kind    = GoogleSheetsErrorKind::network,
          .message = "transport is not configured",
      }};
    }
    return impl_->transport(current_request);
  };

  auto response = send_request(request);
  if (!response) {
    return std::unexpected{response.error()};
  }

  if constexpr (std::same_as<Auth, UserOAuth2Auth>) {
    if (retry_on_unauthorized && response->status_code == 401) {
      // 失効したアクセストークンを控えておくことで、他スレッドが先に refresh 済みかどうかを判断できます。
      auto constexpr authorization_bearer_prefix = std::string_view{"Authorization: Bearer "};
      auto           failed_access_token         = std::string_view{};
      for (auto const& header : request.headers) {
        auto const header_view = std::string_view{header};
        if (header_view.starts_with(authorization_bearer_prefix)) {
          failed_access_token = header_view.substr(authorization_bearer_prefix.size());
          break;
        }
      }

      auto refreshed = detail::force_refresh_token(*impl_->auth, impl_->shared_state, impl_->transport, impl_->clock, failed_access_token);
      if (!refreshed) {
        return std::unexpected{refreshed.error()};
      }

      // 元リクエストの Authorization ヘッダーだけ差し替え、同一条件で 1 回だけ再送します。
      auto retry_request = std::move(request);
      auto replaced      = false;
      for (auto& header : retry_request.headers) {
        auto const header_view = std::string_view{header};
        if (header_view.starts_with(authorization_bearer_prefix)) {
          header.resize(authorization_bearer_prefix.size());
          header += refreshed->access_token;
          replaced = true;
          break;
        }
      }
      if (!replaced) {
        auto header = std::string{authorization_bearer_prefix};
        header += refreshed->access_token;
        retry_request.headers.push_back(std::move(header));
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
