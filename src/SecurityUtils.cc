#include "SecurityUtils.h"
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <json/json.h>
#include <sstream>
#include <iomanip>
#include <vector>

namespace security {

std::string base64UrlEncode(const std::string& input) {
    static const char* b64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(b64Chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(b64Chars[((val << 8) >> (valb + 8)) & 0x3F]);
    return out;
}

std::string base64UrlDecode(const std::string& input) {
    std::string in = input;
    while (in.size() % 4 != 0) in.push_back('=');
    for (char& c : in) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }

    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) {
        T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;
    }

    std::string out;
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

std::string hmacSha256(const std::string& data, const std::string& key) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int len = 0;

    HMAC(EVP_sha256(),
         key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         hash, &len);

    return std::string(reinterpret_cast<char*>(hash), len);
}

std::string hashPassword(const std::string& password) {
    unsigned char salt[16];
    RAND_bytes(salt, sizeof(salt));

    std::ostringstream ss;
    for (unsigned char b : salt) ss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    std::string saltHex = ss.str();

    std::string combined = saltHex + ":" + password;
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(combined.data()), combined.size(), hash);

    std::ostringstream hashHex;
    for (unsigned char b : hash) hashHex << std::hex << std::setw(2) << std::setfill('0') << (int)b;

    return "sha256$" + saltHex + "$" + hashHex.str();
}

bool verifyPassword(const std::string& password, const std::string& storedHash) {
    if (storedHash.rfind("sha256$", 0) != 0) {
        // Fallback check or comparison
        return false;
    }

    size_t first = storedHash.find('$');
    size_t second = storedHash.find('$', first + 1);
    if (first == std::string::npos || second == std::string::npos) return false;

    std::string saltHex = storedHash.substr(first + 1, second - first - 1);
    std::string expectedHashHex = storedHash.substr(second + 1);

    std::string combined = saltHex + ":" + password;
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(combined.data()), combined.size(), hash);

    std::ostringstream hashHex;
    for (unsigned char b : hash) hashHex << std::hex << std::setw(2) << std::setfill('0') << (int)b;

    return hashHex.str() == expectedHashHex;
}

std::string createJwtToken(const std::string& userId, const std::string& secretKey, std::chrono::seconds expirySeconds) {
    Json::Value header;
    header["alg"] = "HS256";
    header["typ"] = "JWT";

    auto now = std::chrono::system_clock::now();
    auto exp = now + expirySeconds;
    int64_t expSeconds = std::chrono::duration_cast<std::chrono::seconds>(exp.time_since_epoch()).count();

    Json::Value payload;
    payload["sub"] = userId;
    payload["exp"] = static_cast<Json::Int64>(expSeconds);

    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    std::string headerJson = Json::writeString(writer, header);
    std::string payloadJson = Json::writeString(writer, payload);

    std::string encodedHeader = base64UrlEncode(headerJson);
    std::string encodedPayload = base64UrlEncode(payloadJson);
    std::string signInput = encodedHeader + "." + encodedPayload;

    std::string signature = hmacSha256(signInput, secretKey);
    std::string encodedSignature = base64UrlEncode(signature);

    return signInput + "." + encodedSignature;
}

bool verifyJwtToken(const std::string& token, const std::string& secretKey, std::string& outUserId) {
    size_t dot1 = token.find('.');
    size_t dot2 = token.rfind('.');
    if (dot1 == std::string::npos || dot2 == std::string::npos || dot1 == dot2) return false;

    std::string signInput = token.substr(0, dot2);
    std::string encodedSignature = token.substr(dot2 + 1);

    std::string expectedSig = hmacSha256(signInput, secretKey);
    if (base64UrlEncode(expectedSig) != encodedSignature) return false;

    std::string payloadJson = base64UrlDecode(token.substr(dot1 + 1, dot2 - dot1 - 1));
    Json::CharReaderBuilder reader;
    Json::Value payload;
    std::string errs;
    std::istringstream s(payloadJson);
    if (!Json::parseFromStream(reader, s, &payload, &errs)) return false;

    auto now = std::chrono::system_clock::now();
    int64_t nowSeconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    if (payload["exp"].asInt64() < nowSeconds) return false;

    outUserId = payload["sub"].asString();
    return true;
}

} // namespace security
