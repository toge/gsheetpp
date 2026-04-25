#include "gsheetpp/google_sheets_client.hpp"

#include <glaze/glaze.hpp>

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

auto read_file(std::string_view path) -> std::expected<std::string, std::string> {
  auto input = std::ifstream{std::string{path}};
  if (!input.is_open()) {
    return std::unexpected{"failed to open service account JSON file"};
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

}  // namespace

auto main(int argc, char** argv) -> int {
  if (argc != 6) {
    std::cerr
        << "usage: gsheetpp_example <service-account.json> <spreadsheet-id> <read-range> "
           "<write-range> <write-values-json>\n";
    return 1;
  }

  auto const service_account_path = std::string_view{argv[1]};
  auto const spreadsheet_id = std::string_view{argv[2]};
  auto const read_range = std::string_view{argv[3]};
  auto const write_range = std::string_view{argv[4]};
  auto const write_values_json = std::string_view{argv[5]};

  auto file_contents = read_file(service_account_path);
  if (!file_contents) {
    std::cerr << file_contents.error() << '\n';
    return 1;
  }

  auto config = gsheetpp::parse_service_account_config(*file_contents);
  if (!config) {
    print_error(config.error());
    return 1;
  }

  auto values = parse_values_json(write_values_json);
  if (!values) {
    std::cerr << values.error() << '\n';
    return 1;
  }

  auto client = gsheetpp::GoogleSheetsClient{*config};

  auto auth = client.authenticate_async().get();
  if (!auth) {
    print_error(auth.error());
    return 1;
  }
  std::cout << "authenticated as token type " << auth->token_type << '\n';

  auto read_result = client.read_values_async(spreadsheet_id, read_range).get();
  if (!read_result) {
    print_error(read_result.error());
    return 1;
  }
  std::cout << "read range: " << read_result->range << " rows=" << read_result->values.size()
            << '\n';

  auto write_result =
      client.write_values_async(spreadsheet_id, write_range, std::span{values.value()}).get();
  if (!write_result) {
    print_error(write_result.error());
    return 1;
  }
  std::cout << "updated range: " << write_result->updated_range
            << " cells=" << write_result->updated_cells << '\n';

  return 0;
}
