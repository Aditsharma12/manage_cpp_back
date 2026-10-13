#pragma once

#include <string>
#include <chrono>

namespace security {

// Base64 URL safe encoding/decoding
std::string base64UrlEncode(const std::string& input);
std::string base64UrlDecode(const std::string& input);

// HMAC SHA256 computation
std::string hmacSha256(const std::string& data, const std::string& key);

// Password hashing & verification
std::string hashPassword(const std::string& password);
bool verifyPassword(const std::string& password, const std::string& storedHash);

// JWT generation and verification
std::string createJwtToken(const std::string& userId, const std::string& secretKey, std::chrono::seconds expirySeconds = std::chrono::seconds(7 * 24 * 3600));
bool verifyJwtToken(const std::string& token, const std::string& secretKey, std::string& outUserId);

} // namespace security
