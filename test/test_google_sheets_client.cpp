#include "catch2/catch_all.hpp"

#include "gsheetpp/google_sheets_client.hpp"
#include "google_sheets_client_detail.hpp"

#include <jwt-cpp/traits/nlohmann-json/defaults.h>

#include <chrono>
#include <atomic>
#include <mutex>
#include <thread>

namespace {

auto make_test_private_key() -> std::string {
  return
      "-----BEGIN PRIVATE KEY-----\n"
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

TEST_CASE("service account JSON is parsed into config", "[service-account]") {
  auto const json = R"({
    "client_email":"svc@example.iam.gserviceaccount.com",
    "private_key":"-----BEGIN PRIVATE KEY-----\nabc\n-----END PRIVATE KEY-----\n",
    "token_uri":"https://oauth2.googleapis.com/token",
    "project_id":"demo-project"
  })";

  auto parsed = gsheetpp::parse_service_account_config(json);

  REQUIRE(parsed.has_value());
  CHECK(parsed->client_email == "svc@example.iam.gserviceaccount.com");
  CHECK(parsed->token_uri == "https://oauth2.googleapis.com/token");
}

TEST_CASE("token response is parsed", "[responses]") {
  auto const json = R"({
    "access_token":"token-value",
    "token_type":"Bearer",
    "expires_in":3600
  })";

  auto parsed = gsheetpp::detail::parse_token_response(json);

  REQUIRE(parsed.has_value());
  CHECK(parsed->access_token == "token-value");
  CHECK(parsed->token_type == "Bearer");
  CHECK(parsed->expires_in_seconds == 3600);
}

TEST_CASE("token error response is parsed into authentication error", "[responses]") {
  auto const json = R"({
    "error":"invalid_grant",
    "error_description":"bad assertion"
  })";

  auto parsed = gsheetpp::detail::parse_token_error_response(json, 400);

  REQUIRE_FALSE(parsed.has_value());
  CHECK(parsed.error().kind == gsheetpp::GoogleSheetsErrorKind::authentication);
  CHECK(parsed.error().http_status == 400);
  CHECK(parsed.error().response_body == json);
}

TEST_CASE("read values response is parsed", "[responses]") {
  auto const json = R"({
    "range":"Sheet1!A1:B2",
    "majorDimension":"ROWS",
    "values":[["name","value"],["foo","42"]]
  })";

  auto parsed = gsheetpp::detail::parse_read_values_response(json);

  REQUIRE(parsed.has_value());
  CHECK(parsed->range == "Sheet1!A1:B2");
  CHECK(parsed->major_dimension == "ROWS");
  REQUIRE(parsed->values.size() == 2);
  CHECK(parsed->values[1][0] == "foo");
}

TEST_CASE("write values response is parsed", "[responses]") {
  auto const json = R"({
    "spreadsheetId":"spreadsheet-id",
    "updatedRange":"Sheet1!C1:D2",
    "updatedRows":2,
    "updatedColumns":2,
    "updatedCells":4
  })";

  auto parsed = gsheetpp::detail::parse_write_values_response(json);

  REQUIRE(parsed.has_value());
  CHECK(parsed->spreadsheet_id == "spreadsheet-id");
  CHECK(parsed->updated_range == "Sheet1!C1:D2");
  CHECK(parsed->updated_cells == 4);
}

TEST_CASE("malformed JSON becomes serialization error", "[responses]") {
  auto parsed = gsheetpp::detail::parse_read_values_response("{");

  REQUIRE_FALSE(parsed.has_value());
  CHECK(parsed.error().kind == gsheetpp::GoogleSheetsErrorKind::serialization);
}

TEST_CASE("api error payload becomes api_response error", "[responses]") {
  auto const json = R"({
    "error": {
      "code": 403,
      "message": "The caller does not have permission",
      "status": "PERMISSION_DENIED"
    }
  })";

  auto parsed = gsheetpp::detail::parse_api_error_response(json, 403);

  REQUIRE_FALSE(parsed.has_value());
  CHECK(parsed.error().kind == gsheetpp::GoogleSheetsErrorKind::api_response);
  CHECK(parsed.error().http_status == 403);
  CHECK(parsed.error().response_body == json);
}

TEST_CASE("jwt assertion contains required claims", "[jwt]") {
  auto const config = gsheetpp::ServiceAccountConfig{
      .client_email = "svc@example.iam.gserviceaccount.com",
      .private_key = make_test_private_key(),
      .token_uri = "https://oauth2.googleapis.com/token",
      .project_id = "demo-project",
  };
  auto const issued_at = std::chrono::system_clock::from_time_t(1'700'000'000);

  auto const jwt_token = gsheetpp::detail::build_jwt_assertion(config, issued_at);

  REQUIRE(jwt_token.has_value());
  auto const decoded = jwt::decode(jwt_token.value());
  CHECK(decoded.get_issuer() == config.client_email);
  CHECK(decoded.get_payload_claim("scope").as_string() ==
        "https://www.googleapis.com/auth/spreadsheets");
  CHECK(decoded.get_audience().contains(config.token_uri));
  CHECK(decoded.get_issued_at().time_since_epoch() == issued_at.time_since_epoch());
  CHECK(decoded.get_expires_at().time_since_epoch() ==
        (issued_at + std::chrono::hours{1}).time_since_epoch());
}

TEST_CASE("token request body includes JWT bearer grant type", "[requests]") {
  auto const body = gsheetpp::detail::build_token_request_body("signed.jwt.token");

  CHECK(body.find("grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer") !=
        std::string::npos);
  CHECK(body.find("assertion=signed.jwt.token") != std::string::npos);
}

TEST_CASE("write values request body serializes values and major dimension", "[requests]") {
  auto const values = std::vector<std::vector<std::string>>{
      {"demo", "123"},
      {"next", "456"},
  };

  auto const body = gsheetpp::detail::build_write_values_request_body(values);

  CHECK(body.find("\"majorDimension\":\"ROWS\"") != std::string::npos);
  CHECK(body.find("\"demo\"") != std::string::npos);
  CHECK(body.find("\"456\"") != std::string::npos);
}

TEST_CASE("authenticate_async returns token info from transport", "[client]") {
  auto const config = gsheetpp::ServiceAccountConfig{
      .client_email = "svc@example.iam.gserviceaccount.com",
      .private_key = make_test_private_key(),
      .token_uri = "https://oauth2.googleapis.com/token",
      .project_id = "demo-project",
  };

  auto client = gsheetpp::detail::make_google_sheets_client_for_test(
      config,
      [](gsheetpp::detail::HttpRequest const& request)
          -> std::expected<gsheetpp::detail::HttpResponse, gsheetpp::GoogleSheetsError> {
        CHECK(request.url == "https://oauth2.googleapis.com/token");
        return gsheetpp::detail::HttpResponse{
            .status_code = 200,
            .body = R"({"access_token":"token-value","token_type":"Bearer","expires_in":3600})",
        };
      });

  auto result = client.authenticate_async().get();

  REQUIRE(result.has_value());
  CHECK(result->access_token == "token-value");
}

TEST_CASE("read_values_async auto-authenticates when no cached token exists", "[client]") {
  auto const config = gsheetpp::ServiceAccountConfig{
      .client_email = "svc@example.iam.gserviceaccount.com",
      .private_key = make_test_private_key(),
      .token_uri = "https://oauth2.googleapis.com/token",
      .project_id = "demo-project",
  };
  auto token_requests = 0;
  auto read_requests = 0;

  auto client = gsheetpp::detail::make_google_sheets_client_for_test(
      config,
      [&](gsheetpp::detail::HttpRequest const& request)
          -> std::expected<gsheetpp::detail::HttpResponse, gsheetpp::GoogleSheetsError> {
        if (request.url == "https://oauth2.googleapis.com/token") {
          ++token_requests;
          return gsheetpp::detail::HttpResponse{
              .status_code = 200,
              .body = R"({"access_token":"token-value","token_type":"Bearer","expires_in":3600})",
          };
        }

        ++read_requests;
        CHECK(request.method == "GET");
        CHECK(request.headers.at(0) == "Authorization: Bearer token-value");
        return gsheetpp::detail::HttpResponse{
            .status_code = 200,
            .body = R"({"range":"Sheet1!A1:B2","majorDimension":"ROWS","values":[["a","b"]]})",
        };
      });

  auto result = client.read_values_async("spreadsheet-id", "Sheet1!A1:B2").get();

  REQUIRE(result.has_value());
  CHECK(token_requests == 1);
  CHECK(read_requests == 1);
}

TEST_CASE("write_values_async uses RAW input option", "[client]") {
  auto const config = gsheetpp::ServiceAccountConfig{
      .client_email = "svc@example.iam.gserviceaccount.com",
      .private_key = make_test_private_key(),
      .token_uri = "https://oauth2.googleapis.com/token",
      .project_id = "demo-project",
  };

  auto client = gsheetpp::detail::make_google_sheets_client_for_test(
      config,
      [](gsheetpp::detail::HttpRequest const& request)
          -> std::expected<gsheetpp::detail::HttpResponse, gsheetpp::GoogleSheetsError> {
        if (request.url == "https://oauth2.googleapis.com/token") {
          return gsheetpp::detail::HttpResponse{
              .status_code = 200,
              .body = R"({"access_token":"token-value","token_type":"Bearer","expires_in":3600})",
          };
        }

        CHECK(request.method == "PUT");
        CHECK(request.url.find("valueInputOption=RAW") != std::string::npos);
        return gsheetpp::detail::HttpResponse{
            .status_code = 200,
            .body = R"({
              "spreadsheetId":"spreadsheet-id",
              "updatedRange":"Sheet1!C1:D2",
              "updatedRows":1,
              "updatedColumns":2,
              "updatedCells":2
            })",
        };
      });

  auto const values = std::vector<std::vector<std::string>>{{"demo", "123"}};
  auto result = client.write_values_async("spreadsheet-id", "Sheet1!C1:D2", values).get();

  REQUIRE(result.has_value());
  CHECK(result->updated_cells == 2);
}

TEST_CASE("network failures surface as network errors", "[client]") {
  auto const config = gsheetpp::ServiceAccountConfig{
      .client_email = "svc@example.iam.gserviceaccount.com",
      .private_key = make_test_private_key(),
      .token_uri = "https://oauth2.googleapis.com/token",
      .project_id = "demo-project",
  };

  auto client = gsheetpp::detail::make_google_sheets_client_for_test(
      config,
      [](gsheetpp::detail::HttpRequest const&)
          -> std::expected<gsheetpp::detail::HttpResponse, gsheetpp::GoogleSheetsError> {
        return std::unexpected{gsheetpp::GoogleSheetsError{
            .kind = gsheetpp::GoogleSheetsErrorKind::network,
            .message = "dns failure",
        }};
      });

  auto result = client.authenticate_async().get();

  REQUIRE_FALSE(result.has_value());
  CHECK(result.error().kind == gsheetpp::GoogleSheetsErrorKind::network);
}

TEST_CASE("concurrent reads share one token refresh", "[client]") {
  auto const config = gsheetpp::ServiceAccountConfig{
      .client_email = "svc@example.iam.gserviceaccount.com",
      .private_key = make_test_private_key(),
      .token_uri = "https://oauth2.googleapis.com/token",
      .project_id = "demo-project",
  };
  auto token_requests = std::atomic_int{0};
  auto read_requests = std::atomic_int{0};
  auto request_mutex = std::mutex{};

  auto client = gsheetpp::detail::make_google_sheets_client_for_test(
      config,
      [&](gsheetpp::detail::HttpRequest const& request)
          -> std::expected<gsheetpp::detail::HttpResponse, gsheetpp::GoogleSheetsError> {
        auto _ = std::scoped_lock{request_mutex};
        if (request.url == "https://oauth2.googleapis.com/token") {
          ++token_requests;
          std::this_thread::sleep_for(std::chrono::milliseconds{25});
          return gsheetpp::detail::HttpResponse{
              .status_code = 200,
              .body = R"({"access_token":"token-value","token_type":"Bearer","expires_in":3600})",
          };
        }

        ++read_requests;
        return gsheetpp::detail::HttpResponse{
            .status_code = 200,
            .body = R"({"range":"Sheet1!A1:B2","majorDimension":"ROWS","values":[["a","b"]]})",
        };
      });

  auto future_a = client.read_values_async("spreadsheet-id", "Sheet1!A1:B2");
  auto future_b = client.read_values_async("spreadsheet-id", "Sheet1!A1:B2");

  auto const result_a = future_a.get();
  auto const result_b = future_b.get();

  REQUIRE(result_a.has_value());
  REQUIRE(result_b.has_value());
  CHECK(token_requests == 1);
  CHECK(read_requests == 2);
}
