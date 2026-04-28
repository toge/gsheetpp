# String Copy Optimization Design

**Problem:** Reduce avoidable `std::string` copies in `gsheetpp` request construction and token parsing so high-frequency Google Sheets API calls incur less allocation overhead, while preserving all existing public behavior, error handling, and tests.

**Scope:** This design covers the five requested optimization points in `src/google_sheets_client.cpp` and `include/gsheetpp/google_sheets_client_impl.hpp`: percent-encoding, values URL/query construction, OAuth retry header replacement, token response parsing, and write-values request body generation. It does not change the public API surface, authentication flow semantics, or error model.

## Goals

- Remove unnecessary intermediate `std::string` allocations in URL and query construction
- Avoid rebuilding Authorization headers during OAuth 401 retry when an in-place update is sufficient
- Parse token responses directly into the final result types instead of an intermediate payload struct
- Avoid copying write-value matrices when generating the JSON request body
- Keep all observable behavior unchanged

## Non-Goals

- Adding benchmark code or performance-test infrastructure
- Changing public async APIs, return types, or retry rules
- Refactoring unrelated parsing or request-building code paths

## Design

### 1. Percent Encoding and URL Construction

- Keep the existing `percent_encode(std::string_view) -> std::string` function for backward compatibility
- Add an internal overload: `void percent_encode(std::string_view input, std::string& out)`
- Make the returning overload allocate its result once, then delegate encoding into that buffer
- Update `build_values_url` and `append_query_parameter` to append directly into the destination URL string instead of composing temporary encoded strings
- Preserve the same percent-encoding rules and output bytes as today

This keeps existing call sites working while allowing hot-path URL builders to reuse one destination string.

### 2. Authorization Header Retry Update

- Keep the current User OAuth2-only 401 retry behavior: one refresh and one retry
- Replace the temporary `replacement` string in `execute_request` with an in-place update of the matching header
- Use a shared `std::string_view` prefix constant for safe comparison and substring extraction
- When rewriting an existing header:
  - compare via `std::string_view`
  - `resize` the header back to the prefix length
  - append the refreshed access token directly
- If no Authorization header exists, keep the current fallback of pushing a new one

This removes one avoidable string construction while preserving retry semantics and exception safety.

### 3. Direct Token Parsing

- Remove the internal `TokenResponsePayload` struct
- Define `glz::meta` mappings so token JSON can be read directly into:
  - `TokenInfo` for service-account token responses
  - `OAuthTokenResponse` for OAuth token responses
- Map JSON key `expires_in` to `TokenInfo::expires_in_seconds`
- Keep unknown-key tolerance unchanged by continuing to use the existing `ignore_unknown_keys_opts`
- Keep parse failure behavior unchanged: the same helper functions still return the same `GoogleSheetsErrorKind::serialization` errors and messages

This eliminates an intermediate object and keeps ownership moving directly into the final response structures.

### 4. Write Values Request Body

- Change the internal helper signature from `std::vector<std::vector<std::string>> const&` to `std::span<std::vector<std::string> const>`
- Keep `write_values_async` public behavior unchanged
- Build the JSON body directly with `glz::obj{"majorDimension", "ROWS", "values", values}` and serialize it with `glz::write_json`
- Avoid introducing a dedicated `WriteValuesRequestPayload` temporary for this path

This removes a full matrix copy in the body-builder hot path and keeps the emitted JSON shape unchanged.

## Compatibility and Error Handling

- The public overload set and existing external call patterns remain unchanged
- `write_values_async` keeps its current signature and request/response behavior
- `GoogleSheetsError` contents, kinds, HTTP status propagation, and response-body preservation remain unchanged
- Service account and API key authentication behavior remain unchanged
- Existing RAII-based refresh coordination remains intact

## Testing and Validation

- Keep existing tests as the primary regression safety net
- Add or adjust only narrowly targeted tests if needed to cover:
  - direct token parsing with unknown fields still ignored
  - write-values JSON shape unchanged after the helper signature change
  - OAuth retry still rewrites and reuses the Authorization header correctly
- Validate with the repository’s existing build/test flow:
  - `cmake --build build`
  - `cd build && ctest -V`
- No new benchmark or measurement code is added

## Implementation Notes

- Prefer direct append-based string construction over format-heavy temporary assembly in the touched hot paths
- Keep the edits surgical and localized to the current files unless a small declaration change is required in `google_sheets_client_impl.hpp`
- Stay within the current C++20-compatible code style used by this repository
