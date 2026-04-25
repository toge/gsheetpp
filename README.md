# gsheetpp

gsheetpp は、サービスアカウントを使用した JWT 認証をサポートする、モダンな C++ 用の Google Sheets API クライアントライブラリです。

## 特徴

- **モダン C++**: C++23 (`std::expected`, `std::future`, `std::span` 等) を活用したクリーンな API。
- **非同期 API**: ネットワーク通信を非同期で行い、`std::future` を通じて結果を返します。
- **JWT 認証**: Google Cloud サービスアカウントの JSON 鍵ファイルを使用した、安全なサーバー間認証。
- **型安全なエラーハンドリング**: `std::expected` を使用した、詳細なエラー情報。
- **最小限の依存関係**: `libcurl`, `glaze`, `jwt-cpp` を使用。

## 依存ライブラリ

このライブラリは以下のパッケージに依存しています（vcpkg 経由での利用を推奨）:

- `curl`: HTTP 通信
- `glaze`: JSON シリアライズ/デシリアライズ
- `jwt-cpp`: JWT の作成と署名
- `nlohmann-json`: jwt-cpp の内部使用
- `openssl`: 暗号化処理

## セットアップ

### 1. Google Cloud プロジェクトの設定

1. [Google Cloud Console](https://console.cloud.google.com/) でプロジェクトを作成します。
2. "Google Sheets API" を有効にします。
3. "認証情報" -> "認証情報を作成" -> "サービスアカウント" を選択し、サービスアカウントを作成します。
4. 作成したサービスアカウントの "キー" タブで、"鍵を追加" -> "新しい鍵を作成" (JSON) を選択し、ファイルをダウンロードします。
5. 操作したいスプレッドシートを、作成したサービスアカウントのメールアドレスと共有します（閲覧/編集権限を付与）。

### 2. ビルド方法

CMake と vcpkg を使用したビルド例：

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[vcpkg_path]/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

## 使用方法

```cpp
#include "gsheetpp/google_sheets_client.hpp"
#include <iostream>

int main() {
  // 1. サービスアカウントの設定を読み込む
  std::string json_content = /* 鍵ファイルの内容を読み込む */;
  auto config = gsheetpp::parse_service_account_config(json_content);
  if (!config) {
    std::cerr << "設定のパースに失敗: " << config.error().message << std::endl;
    return 1;
  }

  // 2. クライアントの作成
  gsheetpp::GoogleSheetsClient client{*config};

  // 3. 値の読み取り (非同期)
  auto spreadsheet_id = "your_spreadsheet_id_here";
  auto read_future = client.read_values_async(spreadsheet_id, "Sheet1!A1:B10");
  
  auto result = read_future.get(); // 非同期完了を待機
  if (result) {
    for (const auto& row : result->values) {
      for (const auto& cell : row) {
        std::cout << cell << ", ";
      }
      std::cout << std::endl;
    }
  } else {
    std::cerr << "読み取りエラー: " << result.error().message << std::endl;
  }

  // 4. 値の書き込み (非同期)
  std::vector<std::vector<std::string>> data = {
    {"Name", "Age"},
    {"Alice", "30"},
    {"Bob", "25"}
  };
  auto write_future = client.write_values_async(spreadsheet_id, "Sheet1!A1", data);
  
  auto write_result = write_future.get();
  if (write_result) {
    std::cout << "更新成功: " << write_result->updated_cells << " セルが更新されました。" << std::endl;
  } else {
    std::cerr << "書き込みエラー: " << write_result.error().message << std::endl;
  }

  return 0;
}
```

## ライセンス

このプロジェクトは MIT ライセンスの下で公開されています。
