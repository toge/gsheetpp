#pragma once

/**
 * @file google_sheets_client.hpp
 * @brief Google Sheets API v4 向けクライアントの公開 API を定義します。
 *
 * このヘッダは「クライアント API」と「公開モデル型」のみを宣言し、
 * 内部実装 (transport 関数・共有状態・RAII ガードなど) は gsheetpp/detail/ 以下に
 * 分離しています。利用者が gsheetpp を使う際に追加で include する必要はなく、
 * このヘッダ 1 つで完全なクライアント API が得られます。
 *
 * std::expected について:
 *   C++23 以降は <expected> 標準ヘッダを利用します。標準が利用できない場合
 *   (C++20 ビルド時) は std::expected が定義されたヘッダが必要です。
 *   現在の実装は C++20/23/26 すべてで同じヘッダを include します。
 */

#include "gsheetpp/detail/internal_types.hpp"
#include "gsheetpp/detail/oauth_token_response.hpp"
#include "gsheetpp/detail/refresh_in_progress_guard.hpp"

#include <concepts>
#include <expected>
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

/**
 * @brief API Key 認証の設定です。
 */
struct ApiKeyAuth {
  std::string api_key{};  ///< Google Sheets API に付与する API キーです。
};

/**
 * @brief サービスアカウント認証に必要な設定です。
 */
struct ServiceAccountConfig {
  std::string client_email{};  ///< サービスアカウントのメールアドレスです。
  std::string private_key{};   ///< JWT 署名に使う PEM 形式秘密鍵です。
  std::string token_uri{};     ///< アクセストークン取得先のエンドポイントです。
  std::string project_id{};    ///< 付随情報として保持する GCP プロジェクト ID です。
};

using ServiceAccountAuth = ServiceAccountConfig;

/**
 * @brief ユーザー委譲 OAuth2 認証に必要な設定です。
 *
 * refresh token がある場合はそれを優先して使用し、ない場合のみ
 * authorization code 交換に進みます。
 */
struct UserOAuth2Auth {
  std::string                         client_id{};                                          ///< OAuth2 クライアント ID です。
  std::string                         client_secret{};                                      ///< OAuth2 クライアントシークレットです。
  std::string                         token_uri{"https://oauth2.googleapis.com/token"};     ///< トークンエンドポイントです。
  std::string                         redirect_uri{};                                       ///< authorization code 交換に必要な redirect URI です。
  /// @note このフィールドはライブラリ内部では使用されません。
  ///       認可コードの取得（ブラウザリダイレクト）はこのライブラリのスコープ外であり、
  ///       スコープの指定は認可 URL 構築時にアプリケーション側で行う必要があります。
  std::vector<std::string>            scopes{};                                             ///< 認可時に要求したスコープ一覧です。
  std::string                         authorization_code{};                                 ///< 初回トークン交換に使う認可コードです。
  std::string                         refresh_token{};                                      ///< 再認可なしで再取得するための refresh token です。
  mutable std::shared_ptr<std::mutex> refresh_token_mutex{std::make_shared<std::mutex>()};  ///< refresh token 参照を直列化する mutex です。

  /**
   * @brief 現在保持している refresh token を取得します。
   * @return 現在の refresh token 文字列です。
   */
  auto current_refresh_token() const -> std::string {
    auto _ = std::scoped_lock{ensure_refresh_token_mutex()};
    return refresh_token;
  }

  /**
   * @brief refresh token を更新します。
   * @param value 新しく保持する refresh token です。
   */
  auto set_refresh_token(std::string value) -> void {
    auto _        = std::scoped_lock{ensure_refresh_token_mutex()};
    refresh_token = std::move(value);
  }

  private:
  /**
   * @brief move 後でも安全に使えるよう refresh token 用 mutex を用意します。
   * @return refresh token 保護に使う mutex 参照です。
   */
  auto ensure_refresh_token_mutex() const -> std::mutex& {
    if (!refresh_token_mutex) {
      refresh_token_mutex = std::make_shared<std::mutex>();
    }
    return *refresh_token_mutex;
  }
};

/**
 * @brief 認証サーバーから取得したアクセストークン情報です。
 *
 * TokenInfo は gsheetpp/detail/internal_types.hpp で定義済みのため、
 * ここでは再宣言しません。
 */;

/**
 * @brief スプレッドシート内の各シート（タブ）の情報を保持します。
 */
struct SheetMetadata {
  std::string title{};     ///< シートのタイトル（タブ名）です。
  int         sheet_id{};  ///< シートの一意な識別子です。
};

/**
 * @brief セルの色（0.0 - 1.0）を表します。
 */
struct Color {
  float red{0.0f};
  float green{0.0f};
  float blue{0.0f};
};

/**
 * @brief セル内テキストの水平配置です。
 */
enum class HorizontalAlign {
  left,
  center,
  right,
};

/**
 * @brief セル内テキストの垂直配置です。
 */
enum class VerticalAlign {
  top,
  middle,
  bottom,
};

/**
 * @brief セルの結合方式です。
 */
enum class MergeType {
  merge_all,      ///< 範囲内の全セルを1つに結合します (MERGE_ALL)。
  merge_rows,     ///< 各行ごとにセルを結合します (MERGE_ROWS)。
  merge_columns,  ///< 各列ごとにセルを結合します (MERGE_COLUMNS)。
};

/**
 * @brief セルの書式設定を表します。
 */
struct CellFormat {
  std::optional<Color> background_color{};  ///< 背景色です。
  std::optional<Color> foreground_color{};  ///< 文字色です。
  std::optional<bool>  bold{};              ///< 太字にするかどうかです。
};

/**
 * @brief セル内テキストのスタイル設定を表します。
 */
struct TextStyle {
  std::optional<std::string> font_family{};    ///< フォントファミリーです。
  std::optional<int>         font_size{};      ///< フォントサイズです。
  std::optional<bool>        bold{};           ///< 太字にするかどうかです。
  std::optional<bool>        italic{};         ///< 斜体にするかどうかです。
  std::optional<bool>        strikethrough{};  ///< 取り消し線を付けるかどうかです。
};

/**
 * @brief 処理対象のセル範囲をインデックスで指定します。
 */
struct GridRange {
  int                sheet_id{};      ///< シート ID です。
  std::optional<int> start_row{};     ///< 開始行（0開始、 inclusive）です。
  std::optional<int> end_row{};       ///< 終了行（0開始、 exclusive）です。
  std::optional<int> start_column{};  ///< 開始列（0開始、 inclusive）です。
  std::optional<int> end_column{};    ///< 終了列（0開始、 exclusive）です。
};

/**
 * @brief 画像やチャートの配置位置（オーバーレイ）を指定します。
 */
struct OverlayPosition {
  int sheet_id{};           ///< シート ID です。
  int row_index{};          ///< アンカーセルの行インデックス（0開始）です。
  int column_index{};       ///< アンカーセルの列インデックス（0開始）です。
  int offset_x_pixels{0};   ///< 横方向のオフセット（ピクセル）です。
  int offset_y_pixels{0};   ///< 縦方向のオフセット（ピクセル）です。
  int width_pixels{600};    ///< 画像の幅（ピクセル）です。
  int height_pixels{400};   ///< 画像の高さ（ピクセル）です。
};

/**
 * @brief オーバーレイ画像の情報を保持します。
 */
struct OverGridImage {
  std::string     source_uri{};  ///< 画像の URL です。
  OverlayPosition position{};    ///< 挿入位置とサイズ設定です。
};

/**
 * @brief Google Drive 上のファイル情報を保持します。
 */
struct DriveFile {
  std::string name{};  ///< ファイル名です。
  std::string id{};    ///< ファイル ID です。
};

/**
 * @brief values.get 応答を保持します。
 */
struct ReadValuesResult {
  std::string                           range{};            ///< サーバーが解決したレンジです。
  std::string                           major_dimension{};  ///< ROWS / COLUMNS などの主次元です。
  std::vector<std::vector<std::string>> values{};           ///< 取得したセル値です。
};

/**
 * @brief values.update 応答を保持します。
 */
struct WriteValuesResult {
  std::string spreadsheet_id{};   ///< 更新対象のスプレッドシート ID です。
  std::string updated_range{};    ///< 実際に更新されたレンジです。
  long        updated_rows{};     ///< 更新された行数です。
  long        updated_columns{};  ///< 更新された列数です。
  long        updated_cells{};    ///< 更新されたセル数です。
};

template <class T>
concept Authenticator = std::same_as<std::remove_cvref_t<T>, ApiKeyAuth> || std::same_as<std::remove_cvref_t<T>, ServiceAccountConfig> || std::same_as<std::remove_cvref_t<T>, UserOAuth2Auth>;

template <Authenticator Auth>
class BasicGoogleSheetsClient;

/**
 * @brief 認証方式を型で切り替えられる Google Sheets クライアントです。
 *
 * スレッド安全性:
 *   - 同一インスタンスの *非 mutating* メソッド (read / write / *_async の各 GET) は
 *     複数スレッドから同時に呼び出せます。内部で ClientSharedState (mutex + cv) を
 *     通してアクセストークン更新が直列化されます。
 *   - set_authenticator / authenticator() は内部状態を変更するため、
 *     他のメソッド実行中に行ってはいけません。複数スレッドから構築・切替を行う場合は
 *     呼び出し側で mutex 等で直列化してください。
 *   - ユーザー委譲 OAuth2 (UserOAuth2Auth) では 401 応答を受けた場合に
 *     最大 1 回だけアクセストークンを更新してリトライします。並行リクエスト間で
 *     refresh は 1 本化されます。
 * @tparam Auth 利用する認証設定型です。
 */
template <Authenticator Auth>
class BasicGoogleSheetsClient {
  public:
  /**
   * @brief デフォルト transport を使ってクライアントを構築します。
   * @param auth 利用する認証設定です。
   */
  explicit BasicGoogleSheetsClient(Auth auth);

  /**
   * @brief transport と clock を差し替えてクライアントを構築します。
   * @param auth 利用する認証設定です。
   * @param transport HTTP 実行関数です。
   * @param clock 現在時刻取得関数です。未指定時は system_clock を使います。
   */
  BasicGoogleSheetsClient(Auth auth, detail::TransportFunction transport, detail::ClockFunction clock = {});

  /**
   * @brief コピーコンストラクタです。内部実装を共有して複製します。
   */
  BasicGoogleSheetsClient(BasicGoogleSheetsClient const& other);

  /**
   * @brief ムーブコンストラクタです。
   */
  BasicGoogleSheetsClient(BasicGoogleSheetsClient&& other) noexcept;

  /**
   * @brief コピー代入演算子です。
   */
  auto operator=(BasicGoogleSheetsClient const& other) -> BasicGoogleSheetsClient&;

  /**
   * @brief ムーブ代入演算子です。
   */
  auto operator=(BasicGoogleSheetsClient&& other) noexcept -> BasicGoogleSheetsClient&;

  /**
   * @brief デストラクタを PIMPL 完全型が見える場所で default 定義します。
   *
   * ヘッダ上で std::unique_ptr<Impl> の所有権解放を成立させるために、
   * デストラクタをここで宣言だけ行い google_sheets_client_impl.hpp 内で
   * 定義します。
   */
  ~BasicGoogleSheetsClient();

  template <Authenticator OtherAuth>
  /**
   * @brief transport / clock を引き継いだまま別認証方式へ再束縛します。
   *
   * 認証キャッシュ (アクセストークン) は新しいクライアントには引き継がれません。
   * @tparam OtherAuth 新しい認証設定型です。
   * @param auth 新しい認証設定です。
   * @return 新しい認証型で構築されたクライアントです。
   */
  auto set_authenticator(OtherAuth auth) const -> BasicGoogleSheetsClient<OtherAuth>;

  /**
   * @brief 明示的に認証を実行してトークンを取得します。
   * @return 成功時は TokenInfo、失敗時は GoogleSheetsError を返す future です。
   */
  auto authenticate_async() -> std::future<std::expected<TokenInfo, GoogleSheetsError>>;

  /**
   * @brief スプレッドシート内のシート（タブ）一覧を非同期で取得します。
   * @param spreadsheet_id 対象スプレッドシート ID です。
   * @return 成功時は SheetMetadata のリスト、失敗時は GoogleSheetsError を返す future です。
   */
  auto get_sheets_async(std::string_view spreadsheet_id) -> std::future<std::expected<std::vector<SheetMetadata>, GoogleSheetsError>>;

  /**
   * @brief 指定レンジの値を非同期で取得します。
   * @param spreadsheet_id 対象スプレッドシート ID です。
   * @param range A1 形式のレンジです。
   * @return 成功時は ReadValuesResult、失敗時は GoogleSheetsError を返す future です。
   */
  auto read_values_async(std::string_view spreadsheet_id, std::string_view range) -> std::future<std::expected<ReadValuesResult, GoogleSheetsError>>;

  /**
   * @brief 指定レンジへ値を非同期で書き込みます。
   * @param spreadsheet_id 対象スプレッドシート ID です。
   * @param range A1 形式のレンジです。
   * @param data 行単位で並んだ書き込みデータです。
   * @return 成功時は WriteValuesResult、失敗時は GoogleSheetsError を返す future です。
   */
  auto write_values_async(std::string_view spreadsheet_id, std::string_view range, std::span<std::vector<std::string> const> data) -> std::future<std::expected<WriteValuesResult, GoogleSheetsError>>;

  /**
   * @brief 新しいシート（タブ）を非同期で追加します。
   * @param spreadsheet_id 対象スプレッドシート ID です。
   * @param title 追加するシートのタイトルです。
   * @return 成功時は追加されたシートの SheetMetadata、失敗時は GoogleSheetsError を返す future です。
   */
  auto add_sheet_async(std::string_view spreadsheet_id, std::string_view title) -> std::future<std::expected<SheetMetadata, GoogleSheetsError>>;

  /**
   * @brief シートの名前を非同期で変更します。
   * @param spreadsheet_id 対象スプレッドシート ID です。
   * @param sheet_id 変更対象のシート ID です。
   * @param new_title 新しいタイトルです。
   * @return 成功時は void、失敗時は GoogleSheetsError を返す future です。
   */
  auto rename_sheet_async(std::string_view spreadsheet_id, int sheet_id, std::string_view new_title) -> std::future<std::expected<void, GoogleSheetsError>>;

  /**
   * @brief シートを非同期で削除します。
   * @param spreadsheet_id 対象スプレッドシート ID です。
   * @param sheet_id 削除対象のシート ID です。
   * @return 成功時は void、失敗時は GoogleSheetsError を返す future です。
   */
  auto delete_sheet_async(std::string_view spreadsheet_id, int sheet_id) -> std::future<std::expected<void, GoogleSheetsError>>;

  /**
   * @brief シートの表示順序を非同期で変更します。
   * @param spreadsheet_id 対象スプレッドシート ID です。
   * @param sheet_id 変更対象のシート ID です。
   * @param new_index 新しいインデックス（0開始）です。
   * @return 成功時は void、失敗時は GoogleSheetsError を返す future です。
   */
  auto reorder_sheet_async(std::string_view spreadsheet_id, int sheet_id, int new_index) -> std::future<std::expected<void, GoogleSheetsError>>;

  /**
   * @brief 指定範囲のセル書式を非同期で一括更新します。
   * @param spreadsheet_id 対象スプレッドシート ID です。
   * @param range 対象のセル範囲（インデックス指定）です。
   * @param format 適用する書式設定です。背景色、文字色、太字を指定可能です。
   * @return 成功時は void、失敗時は GoogleSheetsError を返す future です。
   */
  auto update_cell_format_async(std::string_view spreadsheet_id, GridRange const& range, CellFormat const& format) -> std::future<std::expected<void, GoogleSheetsError>>;

  /**
   * @brief 指定範囲の文字スタイルを非同期で一括更新します。
   * @param spreadsheet_id 対象スプレッドシート ID です。
   * @param range 対象のセル範囲（インデックス指定）です。
   * @param style 適用する文字スタイル設定です。
   * @return 成功時は void、失敗時は GoogleSheetsError を返す future です。
   */
  auto set_text_style_async(std::string_view spreadsheet_id, GridRange const& range, TextStyle const& style) -> std::future<std::expected<void, GoogleSheetsError>>;

  /**
   * @brief 指定範囲のセル配置を非同期で一括更新します。
   * @param spreadsheet_id 対象スプレッドシート ID です。
   * @param range 対象のセル範囲（インデックス指定）です。
   * @param horizontal 水平配置です。
   * @param vertical 垂直配置です。
   * @return 成功時は void、失敗時は GoogleSheetsError を返す future です。
   */
  auto set_cell_alignment_async(std::string_view spreadsheet_id, GridRange const& range, HorizontalAlign horizontal, VerticalAlign vertical)
      -> std::future<std::expected<void, GoogleSheetsError>>;

  /**
   * @brief 指定範囲のセル配置を非同期で一括更新します。
   * @param spreadsheet_id 対象スプレッドシート ID です。
   * @param range 対象のセル範囲（インデックス指定）です。
   * @param horizontal 水平配置です。LEFT/CENTER/RIGHT を大文字小文字非依存で受け付けます。
   * @param vertical 垂直配置です。TOP/MIDDLE/BOTTOM を大文字小文字非依存で受け付けます。
   * @return 成功時は void、失敗時は GoogleSheetsError を返す future です。
   */
  auto set_cell_alignment_async(std::string_view spreadsheet_id, GridRange const& range, std::string_view horizontal, std::string_view vertical)
      -> std::future<std::expected<void, GoogleSheetsError>>;

  /**
   * @brief 指定したシートの行・列の固定設定を非同期で更新します。
   * @param spreadsheet_id 対象スプレッドシート ID です。
   * @param sheet_id 固定設定を適用するシート ID です。
   * @param frozen_row_count 固定する行数です。
   * @param frozen_column_count 固定する列数です。
   * @return 成功時は void、失敗時は GoogleSheetsError を返す future です。
   */
  auto freeze_panes_async(std::string_view spreadsheet_id, int sheet_id, int frozen_row_count, int frozen_column_count) -> std::future<std::expected<void, GoogleSheetsError>>;

  /**
   * @brief シート内の全データを非同期で取得します。
   * @param spreadsheet_id 対象スプレッドシート ID です。
   * @param sheet_name 対象のシート名です。
   * @return 成功時は ReadValuesResult、失敗時は GoogleSheetsError を返す future です。
   */
  auto get_used_range_async(std::string_view spreadsheet_id, std::string_view sheet_name) -> std::future<std::expected<ReadValuesResult, GoogleSheetsError>>;

  /**
   * @brief 画像をシート上の指定位置にオーバーレイとして非同期で追加します。
   * @param spreadsheet_id 対象スプレッドシート ID です。
   * @param image 追加する画像の URL と配置情報です。
   * @return 成功時は void、失敗時は GoogleSheetsError を返す future です。
   */
  auto add_over_grid_image_async(std::string_view spreadsheet_id, OverGridImage const& image) -> std::future<std::expected<void, GoogleSheetsError>>;

  /**
   * @brief 新しいスプレッドシートを非同期で作成します。
   * @param title 作成するスプレッドシートのタイトルです。
   * @return 成功時は作成されたスプレッドシートの ID、失敗時は GoogleSheetsError を返す future です。
   */
  auto create_new_spreadsheet_async(std::string_view title) -> std::future<std::expected<std::string, GoogleSheetsError>>;

  /**
   * @brief マイドライブのルート直下にあるスプレッドシートを一覧取得します。
   * @return 成功時は DriveFile のリスト、失敗時は GoogleSheetsError を返す future です。
   */
  auto fetch_root_spreadsheets_async() -> std::future<std::expected<std::vector<DriveFile>, GoogleSheetsError>>;

  /**
   * @brief 指定範囲のセルを非同期で結合します。
   * @param spreadsheet_id 対象スプレッドシート ID です。
   * @param range 結合対象のセル範囲です。
   * @param type 結合方式（全結合、行ごと、列ごと）です。
   * @return 成功時は void、失敗時は GoogleSheetsError を返す future です。
   */
  auto merge_cells_async(std::string_view spreadsheet_id, GridRange const& range, MergeType type) -> std::future<std::expected<void, GoogleSheetsError>>;

  /**
   * @brief 指定範囲のセルの結合を非同期で解除します。
   * @param spreadsheet_id 対象スプレッドシート ID です。
   * @param range 結合解除対象のセル範囲です。
   * @return 成功時は void、失敗時は GoogleSheetsError を返す future です。
   */
  auto unmerge_cells_async(std::string_view spreadsheet_id, GridRange const& range) -> std::future<std::expected<void, GoogleSheetsError>>;

  /**
   * @brief 現在の認証設定を可変参照で返します。
   *
   * @warning この参照を通じて認証設定を書き換えた場合、内部状態との整合性は
   *          呼び出し側の責任で管理してください。複数スレッドから同時に書き換える操作は
   *          サポートされません。
   * @return 内部保持している認証設定です。
   */
  auto authenticator() -> Auth& { return *impl_->auth; }

  /**
   * @brief 現在の認証設定を読み取り専用参照で返します。
   * @return 内部保持している認証設定です。
   */
  auto authenticator() const -> Auth const& { return *impl_->auth; }

  private:
  /**
   * @brief 認証方式ごとにリクエストへ認証情報を付与します。
   * @param request 更新対象の HTTP リクエストです。
   * @param is_write 書き込みリクエストかどうかです。
   * @return 成功時は void、失敗時は GoogleSheetsError です。
   */
  auto prepare_request(detail::HttpRequest& request, bool is_write) -> std::expected<void, GoogleSheetsError>;

  /**
   * @brief HTTP リクエストを実行し、必要に応じて 401 再試行を行います。
   * @param request 実行する HTTP リクエストです。
   * @param retry_on_unauthorized User OAuth2 の 401 再試行を許可するかどうかです。
   * @return 成功時は HttpResponse、失敗時は GoogleSheetsError です。
   */
  auto execute_request(detail::HttpRequest request, bool retry_on_unauthorized) -> std::expected<detail::HttpResponse, GoogleSheetsError>;

  /**
   * @brief 非同期実行を async ポリシーで起動する内部ヘルパです。
   *
   * std::launch::async | std::launch::deferred ではなく、必ず async で
   * スレッドを起動します。これにより .get() まで実行が遅延する deferred 経路や
   * 標準ライブラリ実装依存の挙動を排除します。
   */
  template <class Fn>
  static auto run_async(Fn&& fn) -> std::future<std::invoke_result_t<Fn>>;

  /**
   * @brief 実装詳細を隠蔽する PIMPL 構造体です。
   *
   * 内部実装 (TransportFunction / ClockFunction / ClientSharedState / 認証ポインタ)
   * はこの構造体に閉じ込めており、フィールド追加に対する利用者コードの
   * 再コンパイルを最小化します。
   */
  struct Impl;

  std::unique_ptr<Impl> impl_;  ///< 内部実装ハンドルです。
};

using GoogleSheetsClient = BasicGoogleSheetsClient<ServiceAccountConfig>;

template <class Auth>
BasicGoogleSheetsClient(Auth) -> BasicGoogleSheetsClient<Auth>;

template <class Auth, class Transport>
BasicGoogleSheetsClient(Auth, Transport&&, detail::ClockFunction = {}) -> BasicGoogleSheetsClient<Auth>;

/**
 * @brief 取得データから最終行（1開始）を特定します。
 * @param values 取得したセルデータです。
 * @return データの最終行番号です。データが空の場合は 0 です。
 */
auto get_last_row(std::vector<std::vector<std::string>> const& values) -> int;

/**
 * @brief 取得データから最終列（1開始）を特定します。
 * @param values 取得したセルデータです。
 * @return データの最終列番号です。データが空の場合は 0 です。
 */
auto get_last_column(std::vector<std::vector<std::string>> const& values) -> int;

/**
 * @brief サービスアカウント JSON を構造化設定へ変換します。
 * @param json サービスアカウント JSON 文字列です。
 * @return 成功時は ServiceAccountConfig、失敗時は GoogleSheetsError です。
 */
auto parse_service_account_config(std::string_view json) -> std::expected<ServiceAccountConfig, GoogleSheetsError>;

}  // namespace gsheetpp

#include "gsheetpp/google_sheets_client_impl.hpp"
