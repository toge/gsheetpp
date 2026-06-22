#include "gsheetpp/google_sheets_client.hpp"

#include <glaze/glaze.hpp>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

auto constexpr SCOPES = "https://www.googleapis.com/auth/spreadsheets";

#define TOKEN_FILE "refresh_token.txt"
#define CONFIG_FILE "oauth_config.json"

// ── config ──────────────────────────────────────────────────────────────

struct OAuthConfig {
  std::string client_id;
  std::string client_secret;
  std::string redirect_uri;
};

template <>
struct glz::meta<OAuthConfig> {
  using T = OAuthConfig;
  static constexpr auto value = object(
      "client_id", &T::client_id,
      "client_secret", &T::client_secret,
      "redirect_uri", &T::redirect_uri
  );
};

namespace {

auto load_config(std::optional<std::string> cli_client_id) -> std::expected<OAuthConfig, std::string> {
  auto config = OAuthConfig{};

  // Try loading from config file first.
  auto input = std::ifstream{CONFIG_FILE};
  if (input.is_open()) {
    auto contents = std::string{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
    if (auto ec = glz::read_json(config, contents)) {
      return std::unexpected{"failed to parse " CONFIG_FILE ": " + glz::format_error(ec, contents)};
    }
  } else {
    return std::unexpected{"config file '" CONFIG_FILE "' not found. "};
  }

  // CLI override takes precedence.
  if (cli_client_id.has_value()) {
    config.client_id = std::move(*cli_client_id);
  }

  // Validate.
  if (config.client_id.empty())   { return std::unexpected{"client_id is required in " CONFIG_FILE}; }
  if (config.client_secret.empty()) { return std::unexpected{"client_secret is required in " CONFIG_FILE}; }
  if (config.redirect_uri.empty())  { return std::unexpected{"redirect_uri is required in " CONFIG_FILE}; }

  return config;
}

// ── helpers ────────────────────────────────────────────────────────────

auto read_file(std::string_view path) -> std::expected<std::string, std::string> {
  auto input = std::ifstream{std::string{path}};
  if (!input.is_open()) {
    return std::unexpected{"failed to open '" + std::string{path} + "'"};
  }
  return std::string{std::istreambuf_iterator<char>{input},
                     std::istreambuf_iterator<char>{}};
}

auto write_file(std::string_view path, std::string_view content) -> bool {
  auto output = std::ofstream{std::string{path}};
  if (!output.is_open()) { return false; }
  output << content;
  return output.good();
}

auto print_error(gsheetpp::GoogleSheetsError const& e) -> void {
  std::cerr << "Error [" << static_cast<int>(e.kind) << "] " << e.message << '\n';
  if (e.http_status) { std::cerr << "  HTTP " << *e.http_status << '\n'; }
  if (!e.response_body.empty()) { std::cerr << "  body: " << e.response_body << '\n'; }
}

// ── OAuth URL ──────────────────────────────────────────────────────────

auto print_auth_url(std::string_view client_id, std::string_view redirect_uri) -> void {
  auto const url =
      "https://accounts.google.com/o/oauth2/v2/auth?"
      "client_id=" + std::string{client_id} +
      "&redirect_uri=" + std::string{redirect_uri} +
      "&response_type=code"
      "&scope=" + std::string{SCOPES} +
      "&access_type=offline"
      "&prompt=consent";

  std::cout << "\nOpen this URL in your browser:\n" << url << "\n\n"
            << "After authorization, paste the authorization code here: ";
}

// ── operations ─────────────────────────────────────────────────────────

auto run_auth_flow(
    OAuthConfig config,
    std::string auth_code,
    std::string spreadsheet_id,
    std::string read_range,
    std::string write_range,
    std::vector<std::vector<std::string>> write_values) -> bool {

  auto client = gsheetpp::BasicGoogleSheetsClient<gsheetpp::UserOAuth2Auth>{
      gsheetpp::UserOAuth2Auth{
          .client_id = std::move(config.client_id),
          .client_secret = std::move(config.client_secret),
          .redirect_uri = std::move(config.redirect_uri),
          .scopes = {SCOPES},
          .authorization_code = std::move(auth_code),
      }};

  {
    auto result = client.read_values_async(spreadsheet_id, read_range).get();
    if (!result) { print_error(result.error()); return false; }
    std::cout << "Read OK — range=" << result->range
              << " rows=" << result->values.size() << '\n';
  }

  {
    auto result = client.write_values_async(spreadsheet_id, write_range, write_values).get();
    if (!result) { print_error(result.error()); return false; }
    std::cout << "Write OK — range=" << result->updated_range
              << " cells=" << result->updated_cells << '\n';
  }

  if (auto const rt = client.authenticator().current_refresh_token(); !rt.empty()) {
    if (write_file(TOKEN_FILE, rt)) {
      std::cout << "Refresh token saved to " TOKEN_FILE "\n";
    }
  }

  return true;
}

auto run_auth_and_list(
    OAuthConfig config,
    std::string auth_code) -> bool {

  auto client = gsheetpp::BasicGoogleSheetsClient<gsheetpp::UserOAuth2Auth>{
      gsheetpp::UserOAuth2Auth{
          .client_id = std::move(config.client_id),
          .client_secret = std::move(config.client_secret),
          .redirect_uri = std::move(config.redirect_uri),
          .scopes = {SCOPES},
          .authorization_code = std::move(auth_code),
      }};

  auto result = client.fetch_root_spreadsheets_async().get();
  if (!result) { print_error(result.error()); return false; }

  std::cout << "Spreadsheets:\n";
  for (auto const& f : *result) {
    std::cout << "  " << f.name << "\n    → " << f.id << '\n';
  }

  if (auto const rt = client.authenticator().current_refresh_token(); !rt.empty()) {
    if (write_file(TOKEN_FILE, rt)) {
      std::cout << "Refresh token saved to " TOKEN_FILE "\n";
    }
  }

  return true;
}

auto run_with_refresh_token(
    OAuthConfig config,
    std::string refresh_token,
    std::string spreadsheet_id,
    std::string read_range,
    std::string write_range,
    std::vector<std::vector<std::string>> write_values) -> bool {

  auto client = gsheetpp::BasicGoogleSheetsClient<gsheetpp::UserOAuth2Auth>{
      gsheetpp::UserOAuth2Auth{
          .client_id = std::move(config.client_id),
          .client_secret = std::move(config.client_secret),
          .refresh_token = std::move(refresh_token),
      }};

  {
    auto result = client.read_values_async(spreadsheet_id, read_range).get();
    if (!result) { print_error(result.error()); return false; }
    std::cout << "Read OK — range=" << result->range
              << " rows=" << result->values.size() << '\n';
  }

  {
    auto result = client.write_values_async(spreadsheet_id, write_range, write_values).get();
    if (!result) { print_error(result.error()); return false; }
    std::cout << "Write OK — range=" << result->updated_range
              << " cells=" << result->updated_cells << '\n';
  }

  return true;
}

auto run_list_spreadsheets(
    OAuthConfig config,
    std::string refresh_token) -> bool {

  auto client = gsheetpp::BasicGoogleSheetsClient<gsheetpp::UserOAuth2Auth>{
      gsheetpp::UserOAuth2Auth{
          .client_id = std::move(config.client_id),
          .client_secret = std::move(config.client_secret),
          .refresh_token = std::move(refresh_token),
      }};

  auto result = client.fetch_root_spreadsheets_async().get();
  if (!result) { print_error(result.error()); return false; }

  std::cout << "Spreadsheets:\n";
  for (auto const& f : *result) {
    std::cout << "  " << f.name << "\n    → " << f.id << '\n';
  }
  return true;
}

// ── config init (interactive) ──────────────────────────────────────────

auto run_init_config() -> bool {
  std::cout << "Creating " CONFIG_FILE "\n\n";

  auto config = OAuthConfig{};

  std::cout << "client_id: ";
  std::getline(std::cin, config.client_id);

  std::cout << "client_secret: ";
  std::getline(std::cin, config.client_secret);

  std::cout << "redirect_uri: ";
  std::getline(std::cin, config.redirect_uri);

  if (config.client_id.empty() || config.client_secret.empty() || config.redirect_uri.empty()) {
    std::cerr << "all fields are required\n";
    return false;
  }

  auto json = std::string{};
  if (auto ec = glz::write_json(config, json)) {
    std::cerr << "failed to serialize config\n";
    return false;
  }

  if (!write_file(CONFIG_FILE, json)) {
    std::cerr << "failed to write " CONFIG_FILE "\n";
    return false;
  }

  std::cout << "\n" CONFIG_FILE " created. You can now use 'auth-list' or 'auth'.\n";
  return true;
}

auto print_usage() -> void {
  std::cerr
      << "usage:\n"
      << "  oauth_sample init                     — create " CONFIG_FILE " interactively\n"
      << "\n"
      << "  oauth_sample [--client-id <id>] auth-list\n"
      << "              — full OAuth flow, then list spreadsheets\n"
      << "\n"
      << "  oauth_sample [--client-id <id>] auth <spreadsheet-id> <read-range>\n"
      << "              <write-range> <write-values-json>\n"
      << "              — full OAuth flow, then read & write\n"
      << "\n"
      << "  oauth_sample [--client-id <id>] list\n"
      << "              — list spreadsheets using saved refresh token\n"
      << "\n"
      << "  oauth_sample [--client-id <id>] refresh <spreadsheet-id> <read-range>\n"
      << "              <write-range> <write-values-json>\n"
      << "              — read & write using saved refresh token\n"
      << "\n"
      << "  All modes read client_id / client_secret / redirect_uri from\n"
      << "  " CONFIG_FILE " by default. --client-id overrides client_id only.\n";
}

// ── argument parsing ───────────────────────────────────────────────────

struct Args {
  std::string mode;
  std::optional<std::string> cli_client_id;
  std::vector<std::string_view> positional;
};

auto parse_args(int argc, char** argv) -> std::expected<Args, std::string> {
  auto args = Args{};

  int i = 1;
  if (i >= argc) { return std::unexpected{"missing mode"}; }

  // Parse flags before mode.
  while (i < argc && argv[i][0] == '-') {
    auto const flag = std::string_view{argv[i]};
    if (flag == "--client-id") {
      if (++i >= argc) { return std::unexpected{"--client-id requires a value"}; }
      args.cli_client_id = argv[i];
    } else if (flag == "--help" || flag == "-h") {
      args.mode = "help";
      return args;
    } else {
      return std::unexpected{"unknown flag: " + std::string{flag}};
    }
    ++i;
  }

  if (i >= argc) { return std::unexpected{"missing mode"}; }

  args.mode = argv[i++];

  while (i < argc) {
    args.positional.push_back(argv[i++]);
  }

  return args;
}

}  // namespace

auto main(int argc, char** argv) -> int {
  auto parsed = parse_args(argc, argv);
  if (!parsed) {
    std::cerr << parsed.error() << "\n\n";
    print_usage();
    return 1;
  }

  auto const& args = *parsed;

  // ── init ──────────────────────────────────────────────────────────────
  if (args.mode == "init") {
    return run_init_config() ? 0 : 1;
  }

  // ── help ──────────────────────────────────────────────────────────────
  if (args.mode == "help") {
    print_usage();
    return 0;
  }

  // ── load config ───────────────────────────────────────────────────────
  auto config = load_config(args.cli_client_id);
  if (!config) {
    std::cerr << config.error() << "\n"
              << "Run 'oauth_sample init' to create it, or pass --client-id.\n";
    return 1;
  }

  // ── auth-list ─────────────────────────────────────────────────────────
  if (args.mode == "auth-list") {
    print_auth_url(config->client_id, config->redirect_uri);
    auto auth_code = std::string{};
    std::getline(std::cin, auth_code);

    return run_auth_and_list(std::move(*config), std::move(auth_code)) ? 0 : 1;
  }

  // ── list ──────────────────────────────────────────────────────────────
  if (args.mode == "list") {
    auto token = read_file(TOKEN_FILE);
    if (!token) {
      std::cerr << "no refresh token found — run 'auth-list' first\n";
      return 1;
    }

    return run_list_spreadsheets(std::move(*config), std::move(*token)) ? 0 : 1;
  }

  // ── auth ──────────────────────────────────────────────────────────────
  if (args.mode == "auth") {
    if (args.positional.size() != 4) { print_usage(); return 1; }

    auto values = std::vector<std::vector<std::string>>{};
    if (auto ec = glz::read_json(values, std::string{args.positional[3]})) {
      std::cerr << "failed to parse write-values JSON\n";
      return 1;
    }

    print_auth_url(config->client_id, config->redirect_uri);
    auto auth_code = std::string{};
    std::getline(std::cin, auth_code);

    return run_auth_flow(
               std::move(*config), std::move(auth_code),
               std::string{args.positional[0]},
               std::string{args.positional[1]},
               std::string{args.positional[2]},
               std::move(values))
               ? 0 : 1;
  }

  // ── refresh ───────────────────────────────────────────────────────────
  if (args.mode == "refresh") {
    if (args.positional.size() != 4) { print_usage(); return 1; }

    auto values = std::vector<std::vector<std::string>>{};
    if (auto ec = glz::read_json(values, std::string{args.positional[3]})) {
      std::cerr << "failed to parse write-values JSON\n";
      return 1;
    }

    auto token = read_file(TOKEN_FILE);
    if (!token) {
      std::cerr << "no refresh token found — run 'auth' first\n";
      return 1;
    }

    return run_with_refresh_token(
               std::move(*config), std::move(*token),
               std::string{args.positional[0]},
               std::string{args.positional[1]},
               std::string{args.positional[2]},
               std::move(values))
               ? 0 : 1;
  }

  std::cerr << "unknown mode: " << args.mode << "\n\n";
  print_usage();
  return 1;
}
