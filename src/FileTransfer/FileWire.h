// src/FileTransfer/FileWire.h
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

#include <vector>

// The wire format for a file sent over the WebSocket, in either direction: one binary
// message holding [uint16 big-endian header length][JSON header][raw bytes]. Free
// functions over bytes and a JsonDocument - nothing here knows about SocketClient, the
// socket or the sketch's callbacks; who asks for a file, when it is sent and on which
// thread all stay in SocketClient (see s_uploadReq there).
//
// The server speaks the same format from its own module, sensordata2-server's
// utils/fileWire.js (tested in test/fileWire.test.js). The two are one contract: a change
// to the header or the length prefix here needs the same change there, and in web-ui's
// utils/fileWire.js.
namespace FileWire {

// Largest file either way over the socket. The server enforces the same limit.
constexpr size_t MAX_SIZE = 4096;

/**
 * @brief Builds one binary file frame.
 *
 * Sets hdr["message"] = "file" and hdr["size"]; the caller supplies the rest of the header
 * (filename, requestId, contentType). One reserve() for the whole frame.
 *
 * @param hdr   header to send, completed in place
 * @param data  the file's bytes
 * @param out   receives the frame
 *
 * @returns false if the header is over 64KB or the allocation failed
 */
bool build(JsonDocument &hdr, const std::vector<uint8_t> &data, std::vector<uint8_t> &out);

/**
 * @brief Parses one inbound binary file frame (see build).
 *
 * On success hdr holds the header and data/dataLen point into p - no copy is made, so they
 * are only valid while p is.
 *
 * @param p        the frame
 * @param len      its length
 * @param hdr      receives the parsed header
 * @param data     receives a pointer into p
 * @param dataLen  receives the number of file bytes
 *
 * @returns false for anything malformed, not a "file" message, or over MAX_SIZE
 */
bool parse(const uint8_t *p, size_t len, JsonDocument &hdr, const uint8_t *&data, size_t &dataLen);

/**
 * @brief vector::reserve that reports failure instead of aborting the device.
 *
 * @returns false if the capacity was not obtained
 */
bool reserve(std::vector<uint8_t> &v, size_t n);

/**
 * @brief vector::assign that reports failure instead of aborting the device.
 *
 * @returns false if the bytes were not copied
 */
bool assign(std::vector<uint8_t> &v, const uint8_t *p, size_t n);

}  // namespace FileWire
