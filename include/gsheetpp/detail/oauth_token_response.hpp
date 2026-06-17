#pragma once

/**
 * @file oauth_token_response.hpp
 * @brief OAuth2 トークン応答の内部表現を宣言します。
 */

#include "gsheetpp/detail/internal_types.hpp"

#include <optional>
#include <string>

namespace gsheetpp::detail {

/**
 * @brief OAuth2 トークン応答のうち refresh token を保持できる形です。
 */
struct OAuthTokenResponse {
  TokenInfo                  token{};          ///< 取得したアクセストークンです。
  std::optional<std::string> refresh_token{};  ///< 新たに払い出された refresh token です。
};

}  // namespace gsheetpp::detail
