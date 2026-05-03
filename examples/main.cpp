#include "gsheetpp/google_sheets_client.hpp"

#include <glaze/glaze.hpp>

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace {

auto read_file(std::string_view path) -> std::expected<std::string, std::string> {
  auto input = std::ifstream{std::string{path}};
  if (!input.is_open()) {
    return std::unexpected{"failed to open input file"};
  }

  auto contents = std::string{
      std::istreambuf_iterator<char>{input},
      std::istreambuf_iterator<char>{},
  };
  return contents;
}

auto parse_values_json(std::string_view json)
    -> std::expected<std::vector<std::vector<std::string>>, std::string> {
  auto values = std::vector<std::vector<std::string>>{};
  auto const buffer = std::string{json};
  if (auto const ec = glz::read_json(values, buffer)) {
    return std::unexpected{"failed to parse write values JSON argument"};
  }
  return values;
}

auto print_error(gsheetpp::GoogleSheetsError const& error) -> void {
  std::cerr << error.message << '\n';
  if (error.http_status.has_value()) {
    std::cerr << "http_status: " << *error.http_status << '\n';
  }
  if (!error.response_body.empty()) {
    std::cerr << error.response_body << '\n';
  }
}

auto run_read(
    auto& client,
    std::string_view spreadsheet_id,
    std::string_view read_range) -> bool {
  auto read_result = client.read_values_async(spreadsheet_id, read_range).get();
  if (!read_result) {
    print_error(read_result.error());
    return false;
  }

  std::cout << "read range: " << read_result->range << " rows=" << read_result->values.size()
            << '\n';
  return true;
}

auto run_write(
    auto& client,
    std::string_view spreadsheet_id,
    std::string_view write_range,
    std::span<std::vector<std::string> const> values) -> bool {
  auto write_result = client.write_values_async(spreadsheet_id, write_range, values).get();
  if (!write_result) {
    print_error(write_result.error());
    return false;
  }

  std::cout << "updated range: " << write_result->updated_range
            << " cells=" << write_result->updated_cells << '\n';
  return true;
}

auto run_list_sheets(
    auto& client,
    std::string_view spreadsheet_id) -> bool {
  auto result = client.get_sheets_async(spreadsheet_id).get();
  if (!result) {
    print_error(result.error());
    return false;
  }

  std::cout << "sheets in spreadsheet: " << spreadsheet_id << '\n';
  for (auto const& sheet : *result) {
    std::cout << "  - " << sheet.title << " (id: " << sheet.sheet_id << ")\n";
  }
  return true;
}

auto print_usage() -> void {
  std::cerr
      << "usage:\n"
      << "  gsheetpp_example list-sheets <api-key> <spreadsheet-id>\n"
      << "  gsheetpp_example api-key <api-key> <spreadsheet-id> <read-range>\n"
      << "  gsheetpp_example service-account <service-account.json> <spreadsheet-id> "
         "<read-range> <write-range> <write-values-json>\n"
      << "  gsheetpp_example oauth-code <client-id> <client-secret> <redirect-uri> "
         "<authorization-code> <spreadsheet-id> <read-range> <write-range> "
         "<write-values-json>\n"
      << "  gsheetpp_example oauth-refresh <client-id> <client-secret> <refresh-token> "
         "<spreadsheet-id> <read-range> <write-range> <write-values-json>\n";
}

}  // namespace

auto main(int argc, char** argv) -> int {
  if (argc < 2) {
    print_usage();
    return 1;
  }

  auto const mode = std::string_view{argv[1]};
  if (mode == "list-sheets") {
    if (argc != 4) {
      print_usage();
      return 1;
    }

    auto client = gsheetpp::BasicGoogleSheetsClient<gsheetpp::ApiKeyAuth>{
        gsheetpp::ApiKeyAuth{
            .api_key = argv[2],
        }};
    return run_list_sheets(client, argv[3]) ? 0 : 1;
  }

  if (mode == "api-key") {
    if (argc != 5) {
      print_usage();
      return 1;
    }

    auto client = gsheetpp::BasicGoogleSheetsClient<gsheetpp::ApiKeyAuth>{
        gsheetpp::ApiKeyAuth{
            .api_key = argv[2],
        }};
    return run_read(client, argv[3], argv[4]) ? 0 : 1;
  }

  if (mode == "service-account") {
    if (argc != 7) {
      print_usage();
      return 1;
    }

    auto file_contents = read_file(argv[2]);
    if (!file_contents) {
      std::cerr << file_contents.error() << '\n';
      return 1;
    }

    auto config = gsheetpp::parse_service_account_config(*file_contents);
    if (!config) {
      print_error(config.error());
      return 1;
    }

    auto values = parse_values_json(argv[6]);
    if (!values) {
      std::cerr << values.error() << '\n';
      return 1;
    }

    auto client = gsheetpp::GoogleSheetsClient{*config};
    if (!run_read(client, argv[3], argv[4])) {
      return 1;
    }
    return run_write(client, argv[3], argv[5], values.value()) ? 0 : 1;
  }

  if (mode == "oauth-code") {
    if (argc != 10) {
      print_usage();
      return 1;
    }

    auto values = parse_values_json(argv[9]);
    if (!values) {
      std::cerr << values.error() << '\n';
      return 1;
    }

    auto client = gsheetpp::BasicGoogleSheetsClient<gsheetpp::UserOAuth2Auth>{
        gsheetpp::UserOAuth2Auth{
            .client_id = argv[2],
            .client_secret = argv[3],
            .redirect_uri = argv[4],
            .scopes = {"https://www.googleapis.com/auth/spreadsheets"},
            .authorization_code = argv[5],
        }};
    if (!run_read(client, argv[6], argv[7])) {
      return 1;
    }
    return run_write(client, argv[6], argv[8], values.value()) ? 0 : 1;
  }

  if (mode == "oauth-refresh") {
    if (argc != 9) {
      print_usage();
      return 1;
    }

    auto values = parse_values_json(argv[8]);
    if (!values) {
      std::cerr << values.error() << '\n';
      return 1;
    }

    auto client = gsheetpp::BasicGoogleSheetsClient<gsheetpp::UserOAuth2Auth>{
        gsheetpp::UserOAuth2Auth{
            .client_id = argv[2],
            .client_secret = argv[3],
            .refresh_token = argv[4],
        }};
    if (!run_read(client, argv[5], argv[6])) {
      return 1;
    }
    return run_write(client, argv[5], argv[7], values.value()) ? 0 : 1;
  }

  print_usage();
  return 1;
}
