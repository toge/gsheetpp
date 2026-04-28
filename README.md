# gsheetpp

gsheetpp は、Google Sheets API v4 を扱うモダンな C++ 用クライアントライブラリです。  
`ApiKeyAuth`、`ServiceAccountAuth`、`UserOAuth2Auth` の 3 認証方式を 1 つの API で扱えます。

## 特徴

- **非同期 API**: すべての通信を `std::future` 経由で実行
- **型安全なエラー処理**: `std::expected` で成功/失敗を明示
- **認証方式の切り替え**: `set_authenticator(...)` で別の認証モデルへ再束縛
- **JWT サービスアカウント認証**: `jwt-cpp` と OpenSSL による RS256 署名
- **OAuth2 自動更新**: User OAuth2 は 401 応答時に 1 回だけ自動 refresh & retry

## 依存ライブラリ

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

| 認証方式 | 用途 | 挙動 |
| --- | --- | --- |
| `ApiKeyAuth` | 公開スプレッドシートの read-only | `key=` クエリを付与。write は即エラー |
| `ServiceAccountAuth` | サーバー間通信 | JWT bearer token を取得して `Authorization` ヘッダーを付与 |
| `UserOAuth2Auth` | ユーザー委譲アクセス | アクセストークンを保持し、401 で 1 回だけ refresh & retry |

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

## ライセンス

MIT
