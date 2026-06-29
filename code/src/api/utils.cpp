/**
 * api/utils - API 层通用小工具实现
 */

#include "api/utils.hpp"
#include "core/device.hpp"
#include "utils/fsHelper.hpp"
#include "utils/http.hpp"
#include "utils/jsonResp.hpp"
#include <borealis/core/i18n.hpp>
#include <cstdio>

namespace api::utils {

bool isOk(const http::Response& resp) {
    if (resp.networkCode != 0) return false;
    if (resp.statusCode < 200) return false;
    if (resp.statusCode >= 300) return false;
    return true;
}

std::string headerValue(const http::Response& resp, const std::string& name) {
    for (const auto& header : resp.headers) {
        if (header.name == name) return header.value;
    }
    return "";
}

std::string responseErrorMessage(const http::Response& resp) {
    JsonResp json(resp);
    std::string message = json.getString("message");
    if (!message.empty()) return message;
    return brls::getStr("other/api/networkError");
}

std::string getParseErrorMessage() {
    return brls::getStr("other/api/parseError");
}

std::vector<uint8_t> toBytes(const std::string& value) {
    return {value.begin(), value.end()};
}

std::vector<http::Header> deviceIdentityHeaders() {
    return {
        {"X-Device-ID", std::to_string(deviceInfo::Identity::getDeviceId())},
    };
}

http::Request makeRequest(http::Method method, const std::string& url, std::stop_token token) {
    http::Request request;
    request.method = method;
    request.url = url;
    request.token = token;
    request.headers = deviceIdentityHeaders();
    return request;
}

void setTextBody(http::Request& request, const std::string& body, const std::string& contentType) {
    request.body = toBytes(body);
    addHeader(request.headers, "Content-Type", contentType);
}

http::Response downloadBytes(const std::string& url, const std::vector<http::Header>& headers, std::stop_token token) {
    auto request = makeRequest(http::Method::Get, url, token);
    for (const auto& header : headers) addHeader(request.headers, header.name, header.value);
    return http::requestToMemory(request);
}

http::Response downloadToFile(const std::string& url, const std::string& path, std::function<bool(size_t total, size_t now)> progress, std::stop_token token) {
    http::Response response;

    auto slashPos = path.rfind('/');
    if (slashPos != std::string::npos && slashPos != 0) fs::ensureDir(path.substr(0, slashPos));

    FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) return response;
    std::setvbuf(fp, nullptr, _IOFBF, 512 * 1024);

    auto request = makeRequest(http::Method::Get, url, token);
    request.progress = progress;

    response = http::requestStream(request, [fp](const uint8_t* data, size_t size) {
        return std::fwrite(data, 1, size, fp) == size;
    });

    std::fclose(fp);

    if (!isOk(response)) std::remove(path.c_str());

    return response;
}

void addHeader(std::vector<http::Header>& headers, const std::string& name, const std::string& value) {
    if (name.empty()) return;
    headers.push_back({name, value});
}

} // namespace api::utils
