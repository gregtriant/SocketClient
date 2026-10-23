// src/FileTransfer/FileWire.cpp
#include "FileWire.h"

#include <new>

namespace FileWire {

// Allocation that reports failure instead of aborting. arduino-esp32 builds with exceptions on,
// where a failed reserve()/assign() throws std::bad_alloc; ESP8266 builds without them, where
// new aborts on its own and the capacity check is all there is.
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
#define FW_TRY_ALLOC(stmt) try { stmt; } catch (const std::bad_alloc &) { return false; }
#else
#define FW_TRY_ALLOC(stmt) stmt;
#endif

bool reserve(std::vector<uint8_t> &v, size_t n) {
    FW_TRY_ALLOC(v.reserve(n))
    return v.capacity() >= n;
}

bool assign(std::vector<uint8_t> &v, const uint8_t *p, size_t n) {
    FW_TRY_ALLOC(v.assign(p, p + n))
    return v.size() == n;
}

bool build(JsonDocument &hdr, const std::vector<uint8_t> &data, std::vector<uint8_t> &out) {
    hdr["message"] = "file";
    hdr["size"]    = data.size();
    size_t hlen = measureJson(hdr);
    if (hlen > 0xFFFF) return false;
    out.clear();
    if (!reserve(out, 2 + hlen + data.size())) return false;
    out.resize(2 + hlen);
    out[0] = (uint8_t)(hlen >> 8);
    out[1] = (uint8_t)(hlen & 0xFF);
    serializeJson(hdr, (char *)out.data() + 2, hlen);
    out.insert(out.end(), data.begin(), data.end());
    return true;
}

bool parse(const uint8_t *p, size_t len, JsonDocument &hdr, const uint8_t *&data, size_t &dataLen) {
    if (!p || len < 2) return false;
    size_t hlen = ((size_t)p[0] << 8) | p[1];
    if (hlen > len - 2) return false;
    if (deserializeJson(hdr, (const char *)p + 2, hlen)) return false;
    const char *msg = hdr["message"] | "";
    if (strcmp(msg, "file") != 0) return false;
    if (!hdr["filename"].is<const char *>()) return false;
    data    = p + 2 + hlen;
    dataLen = len - 2 - hlen;
    return dataLen <= MAX_SIZE;
}

}  // namespace FileWire
