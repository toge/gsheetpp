#include "catch2/catch_all.hpp"

#include "google_sheets_client_detail.hpp"
#include "gsheetpp/google_sheets_client.hpp"

#include <jwt-cpp/traits/nlohmann-json/defaults.h>

/**
 * @file test_sheet_management.cpp
 * @brief シート管理機能（追加・名前変更・削除・順序変更）のテストです。
 */

namespace {

auto make_test_private_key() -> std::string {
  return "-----BEGIN PRIVATE KEY-----\n"
         "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCdq5V0dNbfQpFb\n"
         "QWkAHLJyJXDScZulLGzEkR/Yzim7c+RjCNVym/0RplAnDd1mnNbmunXETwv7GJTe\n"
         "OWGkR0qu+DiIROtlX/a0DY3MK8WWJPtpMU2Akg9rgjk0u9Mz7/CUtXun3786LLBg\n"
         "tCD8fX53Z0QiogsPRBLKx4rZhgylwYjT0rC02UhmnEB7IASiZmXAjF7EVfNWDn4t\n"
         "eU189n8JgJOF6SlzU8apN5VHUPOIAS1gWw5BR0YY6tXxYP19OeYFtO+MpyTFKSYM\n"
         "eTR8ThERYQSgBT/BDBPqrABEgJYT7UfO66fAkNoddFzluDt3dU9QmTWao2JzxR/u\n"
         "04C72PTJAgMBAAECggEAObeCZdeRgrfVCvFGVR6sKgHPq5Gf9tZs2IYBgPVzLGYP\n"
         "sDsfG63DdY8Kn0lBh1sZheuyyZJgIeJNOI4L0r2wNp2x4BxOiVUYM7AUfK13Tm+q\n"
         "QsckBlExaNsKQuYQud33FRDcO8c5sruCM1wtCRCNV3tLY1wrLULvmCB0kQ0zmwdK\n"
         "zYXIzCkKgTYk6KdqiNLiEovi3VCcsWXf5Af7pmgGFsSvTWxlFKTTrvcBcN8kDfNT\n"
         "Am0ZPOEBH6jCvoWiJapIhF5HTRzzSysrLYeydd7ZDN/R/iwZHywKLrTnD/0+SRu5\n"
         "gl+cNTfu2n9S1qRHXX586GWXDT70T/lhQ/eYA8G9wwKBgQDVEWQgZ7107q1V/8wE\n"
         "oEpOCsvdX0WR7pTZJM7DM1v1XB/83nf5X9I7MfdQ+FO42JcrZEMAYlxXYz5rTO38\n"
         "IfDYMj09bw4XKAWL6adCVT7KeG6Q4NQAP+uWRkzaaQ968tbLYS6GPC08bqtGzcqq\n"
         "OdkNQYER5iujGBwJSZdEUKQ6jwKBgQC9cKIcgFSoTrBNny8mxIdmc4NK9zvtpXIW\n"
         "KupuIjBaEOLBsElEmI07M5yQ0fUdZIQzK2b2sv0Hv0XwkHrfjwYfzgyGtSSmmITb\n"
         "tN5bM5FoewGFVn/R7W9KofFxYwywoUhQATQyI4375DZ29XcRKQ9QCKISa3vwxpSt\n"
         "hdZzsLPnJwKBgQDCnwMdkoT9BXMxZdketKeKx9PV3st2dD5kZnmy1fv+j+rsGO3v\n"
         "zLYEuixVOq+G3difmxKUjEQa0p1Wd8u+jeYoJSPJNOLjBfyjA4dzUNhtKzqbfbJI\n"
         "lBqGhqp2qpaoUJ8g4SEVHmyXkeNDZoDjorl/oUDbb6qWbFRXq2/Px/VrQwKBgQCo\n"
         "mg4t/6lZAm+3Je35OwCnFjfMCK61m4ImaJus2ZDfGBg4+oS7IGrSPeUinzrgpg1i\n"
         "3UYOWs8IjhvZNz2kqs5wkBpl6eJsw2G7iQY9dW/85T09RvcMB539dpRErjslGOYb\n"
         "Dnc+CJbdkQdIaL9H7ptKR+S3MCJm8NZyeaOb47C6EQKBgHxlzBosJiV1jA1qg8OM\n"
         "lLXWoIz0jeJMFo3BX/ixQD94jbHW6Ku5ansCpEF3I1Qo4u+rAkW5lLgRpiXGV0rN\n"
         "2lYjkCsStnLIoyMHF8sSIFTB2WWJmElTKpLlrZa/9qNDhEKpRpp12FLPQWAF3kZE\n"
         "2WuBJ7t7eINKho30RYppYJJM\n"
         "-----END PRIVATE KEY-----\n";
}

}  // namespace

TEST_CASE("addSheet request body is serialized", "[requests]") {
  auto const body = gsheetpp::detail::build_add_sheet_request_body("NewSheet");
  CHECK(body.find("\"addSheet\"") != std::string::npos);
  CHECK(body.find("\"title\":\"NewSheet\"") != std::string::npos);
}

TEST_CASE("renameSheet request body is serialized", "[requests]") {
  auto const body = gsheetpp::detail::build_rename_sheet_request_body(123, "RenamedSheet");
  CHECK(body.find("\"updateSheetProperties\"") != std::string::npos);
  CHECK(body.find("\"sheetId\":123") != std::string::npos);
  CHECK(body.find("\"title\":\"RenamedSheet\"") != std::string::npos);
  CHECK(body.find("\"fields\":\"title\"") != std::string::npos);
}

TEST_CASE("deleteSheet request body is serialized", "[requests]") {
  auto const body = gsheetpp::detail::build_delete_sheet_request_body(456);
  CHECK(body.find("\"deleteSheet\"") != std::string::npos);
  CHECK(body.find("\"sheetId\":456") != std::string::npos);
}

TEST_CASE("reorderSheet request body is serialized", "[requests]") {
  auto const body = gsheetpp::detail::build_reorder_sheet_request_body(789, 2);
  CHECK(body.find("\"updateSheetProperties\"") != std::string::npos);
  CHECK(body.find("\"sheetId\":789") != std::string::npos);
  CHECK(body.find("\"index\":2") != std::string::npos);
  CHECK(body.find("\"fields\":\"index\"") != std::string::npos);
}

TEST_CASE("parse_add_sheet_response extracts metadata", "[responses]") {
  auto const json = R"({
    "replies": [
      {
        "addSheet": {
          "properties": {
            "sheetId": 12345,
            "title": "NewSheet"
          }
        }
      }
    ]
  })";

  auto parsed = gsheetpp::detail::parse_add_sheet_response(json);
  REQUIRE(parsed.has_value());
  CHECK(parsed->sheet_id == 12345);
  CHECK(parsed->title == "NewSheet");
}

TEST_CASE("add_sheet_async successful execution", "[client]") {
  auto const config = gsheetpp::ServiceAccountConfig{
      .client_email = "svc@example.iam.gserviceaccount.com",
      .private_key  = make_test_private_key(),
      .token_uri    = "https://oauth2.googleapis.com/token",
      .project_id   = "demo-project",
  };

  auto client = gsheetpp::detail::make_google_sheets_client_for_test(
      config, [](gsheetpp::detail::HttpRequest const& request) -> std::expected<gsheetpp::detail::HttpResponse, gsheetpp::GoogleSheetsError> {
        if (request.url == "https://oauth2.googleapis.com/token") {
          return gsheetpp::detail::HttpResponse{
              .status_code = 200,
              .body        = R"({"access_token":"token-value","token_type":"Bearer","expires_in":3600})",
          };
        }

        CHECK(request.method == "POST");
        CHECK(request.url.find(":batchUpdate") != std::string::npos);
        return gsheetpp::detail::HttpResponse{
            .status_code = 200,
            .body        = R"({
              "replies": [
                {
                  "addSheet": {
                    "properties": {
                      "sheetId": 555,
                      "title": "CreatedSheet"
                    }
                  }
                }
              ]
            })",
        };
      });

  auto result = client.add_sheet_async("spreadsheet-id", "CreatedSheet").get();
  REQUIRE(result.has_value());
  CHECK(result->sheet_id == 555);
  CHECK(result->title == "CreatedSheet");
}

TEST_CASE("rename_sheet_async successful execution", "[client]") {
  auto const config = gsheetpp::ServiceAccountConfig{
      .client_email = "svc@example.iam.gserviceaccount.com",
      .private_key  = make_test_private_key(),
      .token_uri    = "https://oauth2.googleapis.com/token",
      .project_id   = "demo-project",
  };

  auto client = gsheetpp::detail::make_google_sheets_client_for_test(
      config, [](gsheetpp::detail::HttpRequest const& request) -> std::expected<gsheetpp::detail::HttpResponse, gsheetpp::GoogleSheetsError> {
        if (request.url == "https://oauth2.googleapis.com/token") {
          return gsheetpp::detail::HttpResponse{
              .status_code = 200,
              .body        = R"({"access_token":"token-value","token_type":"Bearer","expires_in":3600})",
          };
        }

        CHECK(request.method == "POST");
        CHECK(request.body.find("\"updateSheetProperties\"") != std::string::npos);
        return gsheetpp::detail::HttpResponse{
            .status_code = 200,
            .body        = R"({"replies":[{}]})",
        };
      });

  auto result = client.rename_sheet_async("spreadsheet-id", 123, "NewName").get();
  REQUIRE(result.has_value());
}

TEST_CASE("delete_sheet_async successful execution", "[client]") {
  auto const config = gsheetpp::ServiceAccountConfig{
      .client_email = "svc@example.iam.gserviceaccount.com",
      .private_key  = make_test_private_key(),
      .token_uri    = "https://oauth2.googleapis.com/token",
      .project_id   = "demo-project",
  };

  auto client = gsheetpp::detail::make_google_sheets_client_for_test(
      config, [](gsheetpp::detail::HttpRequest const& request) -> std::expected<gsheetpp::detail::HttpResponse, gsheetpp::GoogleSheetsError> {
        if (request.url == "https://oauth2.googleapis.com/token") {
          return gsheetpp::detail::HttpResponse{
              .status_code = 200,
              .body        = R"({"access_token":"token-value","token_type":"Bearer","expires_in":3600})",
          };
        }

        CHECK(request.method == "POST");
        CHECK(request.body.find("\"deleteSheet\"") != std::string::npos);
        return gsheetpp::detail::HttpResponse{
            .status_code = 200,
            .body        = R"({"replies":[{}]})",
        };
      });

  auto result = client.delete_sheet_async("spreadsheet-id", 456).get();
  REQUIRE(result.has_value());
}

TEST_CASE("reorder_sheet_async successful execution", "[client]") {
  auto const config = gsheetpp::ServiceAccountConfig{
      .client_email = "svc@example.iam.gserviceaccount.com",
      .private_key  = make_test_private_key(),
      .token_uri    = "https://oauth2.googleapis.com/token",
      .project_id   = "demo-project",
  };

  auto client = gsheetpp::detail::make_google_sheets_client_for_test(
      config, [](gsheetpp::detail::HttpRequest const& request) -> std::expected<gsheetpp::detail::HttpResponse, gsheetpp::GoogleSheetsError> {
        if (request.url == "https://oauth2.googleapis.com/token") {
          return gsheetpp::detail::HttpResponse{
              .status_code = 200,
              .body        = R"({"access_token":"token-value","token_type":"Bearer","expires_in":3600})",
          };
        }

        CHECK(request.method == "POST");
        CHECK(request.body.find("\"index\"") != std::string::npos);
        return gsheetpp::detail::HttpResponse{
            .status_code = 200,
            .body        = R"({"replies":[{}]})",
        };
      });

  auto result = client.reorder_sheet_async("spreadsheet-id", 789, 0).get();
  REQUIRE(result.has_value());
}

TEST_CASE("update_cell_format_async request body serialization", "[requests]") {
  auto const range = gsheetpp::GridRange{
      .sheet_id     = 0,
      .start_row    = 1,
      .end_row      = 5,
      .start_column = 0,
      .end_column   = 2,
  };
  auto const format = gsheetpp::CellFormat{
      .background_color = gsheetpp::Color{1.0f, 0.0f, 0.0f},
      .foreground_color = gsheetpp::Color{0.0f, 1.0f, 0.0f},
      .bold             = true,
  };

  auto const body = gsheetpp::detail::build_update_cell_format_request_body(range, format);

  CHECK(body.find("\"sheetId\":0") != std::string::npos);
  CHECK(body.find("\"startRowIndex\":1") != std::string::npos);
  CHECK(body.find("\"endRowIndex\":5") != std::string::npos);
  CHECK(body.find("\"backgroundColor\"") != std::string::npos);
  CHECK(body.find("\"foregroundColor\"") != std::string::npos);
  CHECK(body.find("\"bold\":true") != std::string::npos);
  CHECK(body.find("\"fields\":\"userEnteredFormat.backgroundColor,userEnteredFormat.textFormat.foregroundColor,userEnteredFormat.textFormat.bold\"") != std::string::npos);
}

TEST_CASE("update_cell_format_async partial update serialization", "[requests]") {
  auto const range = gsheetpp::GridRange{
      .sheet_id = 123,
  };
  auto const format = gsheetpp::CellFormat{
      .bold = false,
  };

  auto const body = gsheetpp::detail::build_update_cell_format_request_body(range, format);

  CHECK(body.find("\"sheetId\":123") != std::string::npos);
  CHECK(body.find("\"startRowIndex\"") == std::string::npos);
  CHECK(body.find("\"backgroundColor\"") == std::string::npos);
  CHECK(body.find("\"bold\":false") != std::string::npos);
  CHECK(body.find("\"fields\":\"userEnteredFormat.textFormat.bold\"") != std::string::npos);
}

TEST_CASE("update_cell_format_async successful execution", "[client]") {
  auto const config = gsheetpp::ServiceAccountConfig{
      .client_email = "svc@example.iam.gserviceaccount.com",
      .private_key  = make_test_private_key(),
      .token_uri    = "https://oauth2.googleapis.com/token",
      .project_id   = "demo-project",
  };

  auto client = gsheetpp::detail::make_google_sheets_client_for_test(
      config, [](gsheetpp::detail::HttpRequest const& request) -> std::expected<gsheetpp::detail::HttpResponse, gsheetpp::GoogleSheetsError> {
        if (request.url == "https://oauth2.googleapis.com/token") {
          return gsheetpp::detail::HttpResponse{
              .status_code = 200,
              .body        = R"({"access_token":"token-value","token_type":"Bearer","expires_in":3600})",
          };
        }

        CHECK(request.method == "POST");
        CHECK(request.body.find("\"repeatCell\"") != std::string::npos);
        return gsheetpp::detail::HttpResponse{
            .status_code = 200,
            .body        = R"({"replies":[{}]})",
        };
      });

  auto const range = gsheetpp::GridRange{.sheet_id = 0};
  auto const format = gsheetpp::CellFormat{.bold = true};
  auto result = client.update_cell_format_async("spreadsheet-id", range, format).get();
  REQUIRE(result.has_value());
}

TEST_CASE("freeze_panes_async request body serialization", "[requests]") {
  auto const body = gsheetpp::detail::build_freeze_panes_request_body(123, 2, 1);

  CHECK(body.find("\"sheetId\":123") != std::string::npos);
  CHECK(body.find("\"frozenRowCount\":2") != std::string::npos);
  CHECK(body.find("\"frozenColumnCount\":1") != std::string::npos);
  CHECK(body.find("\"fields\":\"gridProperties.frozenRowCount,gridProperties.frozenColumnCount\"") != std::string::npos);
}

TEST_CASE("freeze_panes_async successful execution", "[client]") {
  auto const config = gsheetpp::ServiceAccountConfig{
      .client_email = "svc@example.iam.gserviceaccount.com",
      .private_key  = make_test_private_key(),
      .token_uri    = "https://oauth2.googleapis.com/token",
      .project_id   = "demo-project",
  };

  auto client = gsheetpp::detail::make_google_sheets_client_for_test(
      config, [](gsheetpp::detail::HttpRequest const& request) -> std::expected<gsheetpp::detail::HttpResponse, gsheetpp::GoogleSheetsError> {
        if (request.url == "https://oauth2.googleapis.com/token") {
          return gsheetpp::detail::HttpResponse{
              .status_code = 200,
              .body        = R"({"access_token":"token-value","token_type":"Bearer","expires_in":3600})",
          };
        }

        CHECK(request.method == "POST");
        CHECK(request.body.find("\"frozenRowCount\"") != std::string::npos);
        return gsheetpp::detail::HttpResponse{
            .status_code = 200,
            .body        = R"({"replies":[{}]})",
        };
      });

  auto result = client.freeze_panes_async("spreadsheet-id", 0, 1, 1).get();
  REQUIRE(result.has_value());
}

TEST_CASE("add_over_grid_image_async request body serialization", "[requests]") {
  auto const image = gsheetpp::OverGridImage{
      .source_uri = "https://example.com/image.png",
      .position =
          {
              .sheet_id        = 0,
              .row_index       = 5,
              .column_index    = 2,
              .offset_x_pixels = 10,
              .offset_y_pixels = 20,
              .width_pixels    = 300,
              .height_pixels   = 200,
          },
  };

  auto const body = gsheetpp::detail::build_add_over_grid_image_request_body(image);

  CHECK(body.find("\"sourceUri\":\"https://example.com/image.png\"") != std::string::npos);
  CHECK(body.find("\"sheetId\":0") != std::string::npos);
  CHECK(body.find("\"rowIndex\":5") != std::string::npos);
  CHECK(body.find("\"columnIndex\":2") != std::string::npos);
  CHECK(body.find("\"offsetXPixels\":10") != std::string::npos);
  CHECK(body.find("\"widthPixels\":300") != std::string::npos);
}

TEST_CASE("add_over_grid_image_async successful execution", "[client]") {
  auto const config = gsheetpp::ServiceAccountConfig{
      .client_email = "svc@example.iam.gserviceaccount.com",
      .private_key  = make_test_private_key(),
      .token_uri    = "https://oauth2.googleapis.com/token",
      .project_id   = "demo-project",
  };

  auto client = gsheetpp::detail::make_google_sheets_client_for_test(
      config, [](gsheetpp::detail::HttpRequest const& request) -> std::expected<gsheetpp::detail::HttpResponse, gsheetpp::GoogleSheetsError> {
        if (request.url == "https://oauth2.googleapis.com/token") {
          return gsheetpp::detail::HttpResponse{
              .status_code = 200,
              .body        = R"({"access_token":"token-value","token_type":"Bearer","expires_in":3600})",
          };
        }

        CHECK(request.method == "POST");
        CHECK(request.body.find("\"addOverGridImage\"") != std::string::npos);
        return gsheetpp::detail::HttpResponse{
            .status_code = 200,
            .body        = R"({"replies":[{}]})",
        };
      });

  auto const image = gsheetpp::OverGridImage{.source_uri = "https://example.com/img.jpg"};
  auto result      = client.add_over_grid_image_async("spreadsheet-id", image).get();
  REQUIRE(result.has_value());
}

TEST_CASE("create_new_spreadsheet_async request body serialization", "[requests]") {
  auto const body = gsheetpp::detail::build_create_spreadsheet_request_body("NewSpreadsheet");

  CHECK(body.find("\"properties\"") != std::string::npos);
  CHECK(body.find("\"title\":\"NewSpreadsheet\"") != std::string::npos);
}

TEST_CASE("create_new_spreadsheet_async successful execution", "[client]") {
  auto const config = gsheetpp::ServiceAccountConfig{
      .client_email = "svc@example.iam.gserviceaccount.com",
      .private_key  = make_test_private_key(),
      .token_uri    = "https://oauth2.googleapis.com/token",
      .project_id   = "demo-project",
  };

  auto client = gsheetpp::detail::make_google_sheets_client_for_test(
      config, [](gsheetpp::detail::HttpRequest const& request) -> std::expected<gsheetpp::detail::HttpResponse, gsheetpp::GoogleSheetsError> {
        if (request.url == "https://oauth2.googleapis.com/token") {
          return gsheetpp::detail::HttpResponse{
              .status_code = 200,
              .body        = R"({"access_token":"token-value","token_type":"Bearer","expires_in":3600})",
          };
        }

        CHECK(request.method == "POST");
        CHECK(request.url == "https://sheets.googleapis.com/v4/spreadsheets");
        return gsheetpp::detail::HttpResponse{
            .status_code = 200,
            .body        = R"({
              "spreadsheetId": "newly-created-id",
              "properties": { "title": "NewSpreadsheet" }
            })",
        };
      });

  auto result = client.create_new_spreadsheet_async("NewSpreadsheet").get();
  REQUIRE(result.has_value());
  CHECK(*result == "newly-created-id");
}

TEST_CASE("fetch_root_spreadsheets_async successful execution", "[client]") {
  auto const config = gsheetpp::ServiceAccountConfig{
      .client_email = "svc@example.iam.gserviceaccount.com",
      .private_key  = make_test_private_key(),
      .token_uri    = "https://oauth2.googleapis.com/token",
      .project_id   = "demo-project",
  };

  auto client = gsheetpp::detail::make_google_sheets_client_for_test(
      config, [](gsheetpp::detail::HttpRequest const& request) -> std::expected<gsheetpp::detail::HttpResponse, gsheetpp::GoogleSheetsError> {
        if (request.url == "https://oauth2.googleapis.com/token") {
          return gsheetpp::detail::HttpResponse{
              .status_code = 200,
              .body        = R"({"access_token":"token-value","token_type":"Bearer","expires_in":3600})",
          };
        }

        CHECK(request.method == "GET");
        CHECK(request.url.find("drive/v3/files") != std::string::npos);
        CHECK(request.url.find("q=") != std::string::npos);
        return gsheetpp::detail::HttpResponse{
            .status_code = 200,
            .body        = R"({
              "files": [
                { "id": "id1", "name": "Sheet1" },
                { "id": "id2", "name": "Sheet2" }
              ]
            })",
        };
      });

  auto result = client.fetch_root_spreadsheets_async().get();
  REQUIRE(result.has_value());
  REQUIRE(result->size() == 2);
  CHECK((*result)[0].id == "id1");
  CHECK((*result)[0].name == "Sheet1");
}
