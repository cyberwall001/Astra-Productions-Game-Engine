// github/WinHttpTransport.cpp - HTTPS GET via WinHTTP (no third-party HTTP stack).
#include "astra/github/ITransport.h"

#if defined(_WIN32)

#include <windows.h>
#include <winhttp.h>

#include <string>

#include "astra/core/Log.h"

#pragma comment(lib, "winhttp.lib")

namespace astra::github {
namespace {

std::wstring Widen(const std::string& text) {
    if (text.empty()) return {};
    const int needed = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                           nullptr, 0);
    std::wstring out(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), needed);
    return out;
}

std::string Narrow(const std::wstring& text) {
    if (text.empty()) return {};
    const int needed = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                           nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), needed,
                        nullptr, nullptr);
    return out;
}

// Splits "https://api.github.com/path?query" into components.
struct UrlParts {
    std::wstring host;
    std::wstring path;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
    bool secure = true;
};

bool ParseUrl(const std::string& url, UrlParts& out) {
    const std::wstring wide = Widen(url);
    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);
    components.dwSchemeLength = static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(wide.c_str(), static_cast<DWORD>(wide.size()), 0, &components)) {
        return false;
    }
    out.secure = components.nScheme == INTERNET_SCHEME_HTTPS;
    out.host.assign(components.lpszHostName, components.dwHostNameLength);
    std::wstring path(components.lpszUrlPath, components.dwUrlPathLength);
    if (components.dwExtraInfoLength > 0) {
        path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }
    if (path.empty()) path = L"/";
    out.path = path;
    out.port = components.nPort;
    return true;
}

class WinHttpTransport final : public ITransport {
public:
    HttpResult Get(const std::string& url, const std::vector<std::string>& headers,
                   int timeoutMs) override {
        HttpResult result;
        UrlParts parts;
        if (!ParseUrl(url, parts)) {
            result.error = "Invalid URL";
            return result;
        }

        HINTERNET session = WinHttpOpen(L"AstraStudio/0.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session) {
            result.error = "WinHTTP could not start (error " + std::to_string(GetLastError()) + ")";
            return result;
        }
        WinHttpSetTimeouts(session, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

        HINTERNET connection = WinHttpConnect(session, parts.host.c_str(), parts.port, 0);
        if (!connection) {
            result.error = "Could not reach " + Narrow(parts.host);
            WinHttpCloseHandle(session);
            return result;
        }

        HINTERNET request = WinHttpOpenRequest(connection, L"GET", parts.path.c_str(), nullptr,
                                               WINHTTP_NO_REFERER,
                                               WINHTTP_DEFAULT_ACCEPT_TYPES,
                                               parts.secure ? WINHTTP_FLAG_SECURE : 0);
        if (!request) {
            result.error = "Could not build the HTTPS request";
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return result;
        }

        std::wstring headerBlock;
        for (const auto& header : headers) {
            headerBlock += Widen(header) + L"\r\n";
        }
        if (!headerBlock.empty()) {
            WinHttpAddRequestHeaders(request, headerBlock.c_str(),
                                     static_cast<DWORD>(headerBlock.size()),
                                     WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
        }

        if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA,
                                0, 0, 0) ||
            !WinHttpReceiveResponse(request, nullptr)) {
            const DWORD error = GetLastError();
            result.error = error == ERROR_WINHTTP_TIMEOUT
                               ? "GitHub took too long to respond"
                               : "Network error (WinHTTP " + std::to_string(error) + ")";
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return result;
        }

        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
                            WINHTTP_NO_HEADER_INDEX);
        result.status = static_cast<int>(statusCode);

        // Remaining rate limit is handy to surface in the hub status bar.
        DWORD remaining = 0;
        DWORD remainingSize = sizeof(remaining);
        if (WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM, L"X-RateLimit-Remaining",
                                &remaining, &remainingSize, WINHTTP_NO_HEADER_INDEX)) {
            result.rateLimitRemaining = static_cast<int>(remaining);
        }

        DWORD available = 0;
        do {
            if (!WinHttpQueryDataAvailable(request, &available) || available == 0) break;
            std::string chunk(available, '\0');
            DWORD read = 0;
            if (!WinHttpReadData(request, chunk.data(), available, &read) || read == 0) break;
            chunk.resize(read);
            result.body += chunk;
        } while (available > 0);

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);

        result.ok = result.status >= 200 && result.status < 300;
        if (!result.ok && result.error.empty()) {
            result.error = "GitHub returned HTTP " + std::to_string(result.status);
        }
        return result;
    }

    std::string Name() const override { return "WinHTTP"; }
};

}  // namespace

TransportPtr MakePlatformTransport() { return std::make_unique<WinHttpTransport>(); }

}  // namespace astra::github

#endif  // _WIN32
