# gsheetpp

gsheetpp は、Google Sheets API v4 を扱うモダンな C++ 用クライアントライブラリです。
`ApiKeyAuth`、`ServiceAccountAuth`、`UserOAuth2Auth` の 3 認証方式を 1 つの API で扱えます。

## 特徴

- **非同期 API**: すべての通信を `std::future` 経由で実行 (内部で `std::launch::async` 固定)
- **型安全なエラー処理**: `std::expected` で成功/失敗を明示
- **認証方式の切り替え**: `set_authenticator(...)` で別の認証モデルへ再束縛
- **JWT サービスアカウント認証**: `jwt-cpp` と OpenSSL による RS256 署名
- **OAuth2 自動更新**: User OAuth2 は 401 応答時に 1 回だけ自動 refresh & retry
- **PIMPL 化されたテンプレート API**: 内部実装変更時のクライアント再コンパイルを最小化

## ビルド要件

- C++20/23/26 いずれかのコンパイラ (CMake が自動で最上位の標準を使用)
- `std::expected` を提供する標準ライブラリ
  - **C++23 以降**: `<expected>` 標準ヘッダを利用
  - **C++20 でビルドする場合**: GCC 12+ / Clang 16+ の `<expected>` 実装、または
    `tl::expected` などの互換ヘッダへの切替が必要。本リポジトリは標準
    `<expected>` を直接 include するため、MSVC や古い libstdc++ を使う場合は
    C++23 以降の利用を推奨
- vcpkg もしくは conan (依存: `catch2`, `curl`, `glaze`, `jwt-cpp`, `nlohmann-json`, `openssl`)

## スレッド安全性

| 操作                                                                   | スレッドセーフか | 補足                                                                                                                |
| ---------------------------------------------------------------------- | ---------------- | ------------------------------------------------------------------------------------------------------------------- |
| 同じクライアントから `read/write/*_async` を複数スレッドで同時呼び出し | ✓               | `ClientSharedState` (mutex + cv) でアクセストークン更新が直列化されます                                             |
| `UserOAuth2Auth` の 401 refresh & retry                                | ✓               | 並行リクエスト間で refresh が 1 本化されます (詳細は `concurrent oauth retries share one refresh after 401` テスト) |
| `BasicGoogleSheetsClient` のコピー                                     | ✓               | `auth` / `transport` / `clock` / `shared_state` が共有されます (token キャッシュも一貫)                             |
| `set_authenticator` の同時呼び出し                                     | ✗               | 内部状態を変更するため、呼び出し側で mutex などで直列化してください                                                 |
| `authenticator()` による `Auth` 設定の書き換え                         | ✗               | 他の API 呼び出しと同時に行うと未定義動作になります                                                                 |

ライブラリ内部の libcurl ハンドルはリクエストごとに新規確保・解放されるため、
複数スレッドから transport を呼び出しても race を起こしません。

## スプレッドシートの新規作成

`create_new_spreadsheet_async` を使用して、指定したタイトルで新しいスプレッドシートを作成できます。

```cpp
auto result = client.create_new_spreadsheet_async("New Project Sheet").get();
if (result) {
  std::string new_id = *result;
  std::cout << "Created new spreadsheet ID: " << new_id << "\n";

  // 作成した ID を使用してデータを書き込む
  client.write_values_async(new_id, "Sheet1!A1", {{"Hello", "World"}}).get();
}
```

## スプレッドシートの一覧取得

`fetch_root_spreadsheets_async` を使用して、マイドライブのルートディレクトリにあるスプレッドシートを一覧取得できます。

```cpp
auto result = client.fetch_root_spreadsheets_async().get();
if (result) {
  for (auto const& file : *result) {
    std::cout << "Name: " << file.name << ", ID: " << file.id << "\n";
  }
}
```

## シート管理


- `curl`
- `glaze`
- `jwt-cpp`
- `nlohmann-json`
- `openssl`

## ビルド

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[vcpkg_path]/scripts/buildsystems/vcpkg.cmake
cmake --build build
cd build && ctest -V
```

## 認証方式

| 認証方式             | 用途                             | 挙動                                                       |
| -------------------- | -------------------------------- | ---------------------------------------------------------- |
| `ApiKeyAuth`         | 公開スプレッドシートの read-only | `key=` クエリを付与。write は即エラー                      |
| `ServiceAccountAuth` | サーバー間通信                   | JWT bearer token を取得して `Authorization` ヘッダーを付与 |
| `UserOAuth2Auth`     | ユーザー委譲アクセス             | アクセストークンを保持し、401 で 1 回だけ refresh & retry  |

## シート管理

ライブラリは以下のシート管理操作を非同期でサポートしています：

- **add_sheet_async**: 新しいシート（タブ）を追加
- **rename_sheet_async**: 既存シートの名前を変更
- **delete_sheet_async**: シートを削除
- **reorder_sheet_async**: シートの表示順序（インデックス）を変更

## セル書式の更新

`update_cell_format_async` を使用して、指定した範囲のセルの背景色、文字色、太字設定を一括で更新できます。

```cpp
auto const range = gsheetpp::GridRange{
  .sheet_id = 0,
  .start_row = 0,
  .end_row = 5,
  .start_column = 0,
  .end_column = 2
};

auto const format = gsheetpp::CellFormat{
  .background_color = gsheetpp::Color{1.0f, 0.8f, 0.8f}, // RGB (0.0 - 1.0)
  .foreground_color = gsheetpp::Color{0.0f, 0.0f, 0.0f},
  .bold = true
};

client.update_cell_format_async("spreadsheet-id", range, format).get();
```

`CellFormat` の各フィールドは `std::optional` なので、更新したい項目だけを指定でき、他の書式を破壊しません。

## 文字スタイルの更新

`set_text_style_async` を使用して、指定した範囲の `fontFamily`、`fontSize`、`bold`、`italic`、`strikethrough` を一括で更新できます。

```cpp
auto const range = gsheetpp::GridRange{
  .sheet_id = 0,
  .start_row = 0,
  .end_row = 3,
  .start_column = 0,
  .end_column = 2
};

auto const style = gsheetpp::TextStyle{
  .font_family = "Arial",
  .font_size = 14,
  .bold = true
};

client.set_text_style_async("spreadsheet-id", range, style).get();
```

`set_text_style_async` は `textFormat` 系の更新専用で、背景色や配置は変更しません。未指定の文字スタイルも field mask に含めないため、そのまま維持されます。

## セル配置の更新

`set_cell_alignment_async` を使用して、指定した範囲の水平配置と垂直配置を一括で更新できます。

```cpp
auto const range = gsheetpp::GridRange{
  .sheet_id = 0,
  .start_row = 0,
  .end_row = 3,
  .start_column = 0,
  .end_column = 2
};

client.set_cell_alignment_async(
  "spreadsheet-id",
  range,
  gsheetpp::HorizontalAlign::center,
  gsheetpp::VerticalAlign::middle).get();

client.set_cell_alignment_async(
  "spreadsheet-id",
  range,
  "right",
  "bottom").get();
```

この API は `repeatCell` の `fields` に `userEnteredFormat.horizontalAlignment,userEnteredFormat.verticalAlignment` だけを指定するため、既存の背景色や枠線には影響しません。

## 行・列の固定

`freeze_panes_async` を使用して、指定したシートの行や列を固定できます。

```cpp
// 1行目と1列目を固定
client.freeze_panes_async("spreadsheet-id", sheet_id, 1, 1).get();

// 行の固定を解除（0を指定）
client.freeze_panes_async("spreadsheet-id", sheet_id, 0, 0).get();
```

## セルの結合と解除

`merge_cells_async` を使用して指定範囲のセルを結合し、`unmerge_cells_async` で結合を解除できます。

```cpp
auto const range = gsheetpp::GridRange{
  .sheet_id = 0,
  .start_row = 0,
  .end_row = 2,
  .start_column = 0,
  .end_column = 2
};

// 全結合 (MERGE_ALL)
client.merge_cells_async("spreadsheet-id", range, gsheetpp::MergeType::merge_all).get();

// 行ごとに結合 (MERGE_ROWS)
client.merge_cells_async("spreadsheet-id", range, gsheetpp::MergeType::merge_rows).get();

// 列ごとに結合 (MERGE_COLUMNS)
client.merge_cells_async("spreadsheet-id", range, gsheetpp::MergeType::merge_columns).get();

// 結合の解除
client.unmerge_cells_async("spreadsheet-id", range).get();
```

## 画像の挿入

`add_over_grid_image_async` を使用して、指定したURLの画像をシート上の指定位置にオーバーレイとして追加できます。

```cpp
auto const image = gsheetpp::OverGridImage{
  .source_uri = "https://example.com/logo.png",
  .position = {
    .sheet_id = 0,
    .row_index = 1,       // 2行目
    .column_index = 1,    // B列
    .offset_x_pixels = 5,
    .offset_y_pixels = 5,
    .width_pixels = 200,
    .height_pixels = 100
  }
};

client.add_over_grid_image_async("spreadsheet-id", image).get();
```

## 使用範囲の取得

`get_used_range_async` を使用して、シート内のデータが存在する全範囲（A1から最終データまで）を取得できます。また、ユーティリティ関数を使用して最終行・最終列を特定できます。

```cpp
auto result = client.get_used_range_async("spreadsheet-id", "Sheet1").get();
if (result) {
  auto const last_row = gsheetpp::get_last_row(result->values);
  auto const last_col = gsheetpp::get_last_column(result->values);
  std::cout << "Last Row: " << last_row << ", Last Col: " << last_col << "\n";
}
```

```cpp
auto add_result = client.add_sheet_async("spreadsheet-id", "New Tab").get();
if (add_result) {
  auto const new_sheet_id = add_result->sheet_id;
  client.rename_sheet_async("spreadsheet-id", new_sheet_id, "Renamed Tab").get();
}
```

### API Key

```cpp
#include "gsheetpp/google_sheets_client.hpp"

auto client = gsheetpp::BasicGoogleSheetsClient<gsheetpp::ApiKeyAuth>{
  gsheetpp::ApiKeyAuth{
    .api_key = "AIza...",
  }
};

auto result = client.read_values_async("spreadsheet-id", "Sheet1!A1:B10").get();
```

API key は公開シートの読み取り専用です。`write_values_async()` は `configuration` エラーを返します。
`authenticate_async()` は no-op success で、実際の検証は `read_values_async()` 側で行われます。

### Service Account

```cpp
#include "gsheetpp/google_sheets_client.hpp"

auto const json = std::string{/* service account json */};
auto config = gsheetpp::parse_service_account_config(json);
if (!config) {
  return 1;
}

auto client = gsheetpp::GoogleSheetsClient{*config};
auto read_result = client.read_values_async("spreadsheet-id", "Sheet1!A1:B10").get();
```

対象シートはサービスアカウントのメールアドレスに共有されている必要があります。

### User OAuth2

```cpp
#include "gsheetpp/google_sheets_client.hpp"

auto client = gsheetpp::BasicGoogleSheetsClient<gsheetpp::UserOAuth2Auth>{
  gsheetpp::UserOAuth2Auth{
    .client_id = "client-id",
    .client_secret = "client-secret",
    .redirect_uri = "http://127.0.0.1/callback",
    .scopes = {"https://www.googleapis.com/auth/spreadsheets"},
    .authorization_code = "authorization-code"
  }
};

auto auth_result = client.authenticate_async().get();
if (!auth_result) {
  return 1;
}

auto const refresh_token = client.authenticator().current_refresh_token();
auto read_result = client.read_values_async("spreadsheet-id", "Sheet1!A1:B10").get();
```

`UserOAuth2Auth` は confidential-client 前提です。ブラウザ起動やローカル callback server はこのライブラリの対象外です。
認可コード取得後のトークン交換、refresh token 保持、401 時の 1 回だけの自動再試行を担当します。

## 補足

`set_authenticator(...)` は transport / clock の注入設定を引き継ぎますが、認証キャッシュ状態は新しい client に持ち越しません。

## Example

`examples/main.cpp` は 4 モードを持ちます。

```bash
gsheetpp_example api-key <api-key> <spreadsheet-id> <read-range>
gsheetpp_example service-account <service-account.json> <spreadsheet-id> <read-range> <write-range> <write-values-json>
gsheetpp_example oauth-code <client-id> <client-secret> <redirect-uri> <authorization-code> <spreadsheet-id> <read-range> <write-range> <write-values-json>
gsheetpp_example oauth-refresh <client-id> <client-secret> <refresh-token> <spreadsheet-id> <read-range> <write-range> <write-values-json>
```

## 付録: 公開フィールド名と Google API パラメータの対応

gsheetpp の公開型は designated initializer での記述性を優先して snake_case を採用しています。
Google Sheets API 本体の camelCase / camelCase フィールドとの対応は以下の通りです。

| gsheetpp (公開型)                    | Google API フィールド      | 用途               |
| ------------------------------------ | -------------------------- | ------------------ |
| `ApiKeyAuth::api_key`                | `key` (クエリ)             | API Key 認証       |
| `ServiceAccountConfig::client_email` | `client_email`             | サービスアカウント |
| `ServiceAccountConfig::private_key`  | `private_key`              | サービスアカウント |
| `ServiceAccountConfig::token_uri`    | `token_uri`                | サービスアカウント |
| `ServiceAccountConfig::project_id`   | `project_id`               | サービスアカウント |
| `UserOAuth2Auth::client_id`          | `client_id`                | OAuth2             |
| `UserOAuth2Auth::client_secret`      | `client_secret`            | OAuth2             |
| `UserOAuth2Auth::token_uri`          | `token_uri`                | OAuth2             |
| `UserOAuth2Auth::redirect_uri`       | `redirect_uri`             | OAuth2             |
| `UserOAuth2Auth::authorization_code` | `code` (フォーム)          | OAuth2 初回        |
| `UserOAuth2Auth::refresh_token`      | `refresh_token` (フォーム) | OAuth2 更新        |
| `TokenInfo::access_token`            | `access_token`             | トークン応答       |
| `TokenInfo::token_type`              | `token_type`               | トークン応答       |
| `TokenInfo::expires_in_seconds`      | `expires_in`               | トークン応答       |
| `SheetMetadata::title`               | `title`                    | シートプロパティ   |
| `SheetMetadata::sheet_id`            | `sheetId`                  | シートプロパティ   |
| `Color::red / green / blue`          | `red / green / blue`       | セル色             |
| `GridRange::sheet_id`                | `sheetId`                  | 範囲指定           |
| `GridRange::start_row`               | `startRowIndex`            | 範囲指定           |
| `GridRange::end_row`                 | `endRowIndex`              | 範囲指定           |
| `GridRange::start_column`            | `startColumnIndex`         | 範囲指定           |
| `GridRange::end_column`              | `endColumnIndex`           | 範囲指定           |
| `OverlayPosition::sheet_id`          | `sheetId`                  | 画像/チャート配置  |
| `OverlayPosition::row_index`         | `rowIndex`                 | 画像/チャート配置  |
| `OverlayPosition::column_index`      | `columnIndex`              | 画像/チャート配置  |
| `OverlayPosition::offset_x_pixels`   | `offsetXPixels`            | 画像/チャート配置  |
| `OverlayPosition::offset_y_pixels`   | `offsetYPixels`            | 画像/チャート配置  |
| `OverlayPosition::width_pixels`      | `widthPixels`              | 画像/チャート配置  |
| `OverlayPosition::height_pixels`     | `heightPixels`             | 画像/チャート配置  |
| `OverGridImage::source_uri`          | `sourceUri`                | 画像追加           |
| `ReadValuesResult::range`            | `range`                    | 値取得応答         |
| `ReadValuesResult::major_dimension`  | `majorDimension`           | 値取得応答         |
| `WriteValuesResult::spreadsheet_id`  | `spreadsheetId`            | 値更新応答         |
| `WriteValuesResult::updated_range`   | `updatedRange`             | 値更新応答         |
| `WriteValuesResult::updated_rows`    | `updatedRows`              | 値更新応答         |
| `WriteValuesResult::updated_columns` | `updatedColumns`           | 値更新応答         |
| `WriteValuesResult::updated_cells`   | `updatedCells`             | 値更新応答         |
| `GoogleSheetsError::http_status`     | HTTP ステータス            | エラー応答         |
| `GoogleSheetsError::response_body`   | HTTP レスポンス本文        | エラー応答         |

## ライセンス

MIT

