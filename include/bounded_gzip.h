#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <bounded_uzlib.h>

struct GzipBuffer {
    uint8_t* data = nullptr;
    size_t size = 0;
    GzipBuffer() = default;
    GzipBuffer(const GzipBuffer&) = delete;
    GzipBuffer& operator=(const GzipBuffer&) = delete;
    void reset() { std::free(data); data = nullptr; size = 0; }
    ~GzipBuffer() { reset(); }
};

inline bool decodeGzipBounded(const uint8_t* input,size_t bytes,size_t limit,GzipBuffer& output) {
    output.reset();
    if (!input || bytes < 18 || bytes > 32768) return false;
    if (limit > 65536) limit = 65536;
    const uint32_t declared = uint32_t(input[bytes-4]) | (uint32_t(input[bytes-3]) << 8) |
        (uint32_t(input[bytes-2]) << 16) | (uint32_t(input[bytes-1]) << 24);
    if (declared > limit) return false;
    // The pinned uzlib parser skips FHCRC without checking it. Validate optional
    // header bounds/checksum here before letting that parser consume the input.
    size_t headerEnd = 10;
    const size_t headerLimit = bytes-8;
    if (input[3] & 4) {
        if (headerLimit-headerEnd < 2) return false;
        const size_t extra = size_t(input[headerEnd]) | (size_t(input[headerEnd+1]) << 8);
        headerEnd += 2;
        if (extra > headerLimit-headerEnd) return false;
        headerEnd += extra;
    }
    for (uint8_t flag=8;flag<=16;flag*=2) {
        if (!(input[3] & flag)) continue;
        while (headerEnd < headerLimit && input[headerEnd]) ++headerEnd;
        if (headerEnd == headerLimit) return false;
        ++headerEnd;
    }
    if (input[3] & 2) {
        if (headerLimit-headerEnd < 2) return false;
        const uint16_t expected = uint16_t(input[headerEnd]) | (uint16_t(input[headerEnd+1]) << 8);
        if (uint16_t(~bounded_uzlib_crc32(input,headerEnd,0xffffffff)) != expected) return false;
    }
    bounded_uzlib_uncomp state = {};
    bounded_uzlib_uncompress_init(&state,nullptr,0);
    state.source = input;
    state.source_limit = input + bytes;
    if (bounded_uzlib_gzip_parse_header(&state) != TINF_OK || state.eof) return false;
    GzipBuffer temporary;
    // One sentinel output byte distinguishes exact completion from a forged small ISIZE.
    temporary.data = static_cast<uint8_t*>(std::malloc(size_t(declared)+1));
    if (!temporary.data) return false;
    state.dest_start = state.dest = temporary.data;
    state.dest_limit = temporary.data + size_t(declared)+1;
    const int result = bounded_uzlib_uncompress_chksum(&state);
    const size_t produced = size_t(state.dest-temporary.data);
    if (result != TINF_DONE || state.eof || produced != declared || state.source != input+bytes)
        return false;
    output.data = temporary.data; output.size = produced;
    temporary.data = nullptr;
    return true;
}
