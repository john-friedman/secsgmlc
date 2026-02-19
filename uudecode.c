#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "uudecode.h"

// Decode a uuencoded buffer (no begin/end lines) into out.
// Returns number of bytes written.
size_t uudecode(const uint8_t *in, size_t in_len, uint8_t *out) {
    size_t out_pos = 0;
    size_t i = 0;

    while (i < in_len) {
        // Length character: how many raw bytes on this line
        uint8_t len_char = in[i];
        if (len_char == '\n' || len_char == '\r') { i++; continue; }

        int nbytes = (len_char - 32) & 0x3f;
        if (nbytes == 0) break; // empty line = end

        i++; // skip length char

        // Decode groups of 4 encoded chars -> 3 raw bytes
        int produced = 0;
        while (produced < nbytes) {
            // Read 4 chars (pad with 32 if line is short)
            uint8_t a = (i < in_len && in[i] != '\n' && in[i] != '\r') ? in[i++] : 32;
            uint8_t b = (i < in_len && in[i] != '\n' && in[i] != '\r') ? in[i++] : 32;
            uint8_t c = (i < in_len && in[i] != '\n' && in[i] != '\r') ? in[i++] : 32;
            uint8_t d = (i < in_len && in[i] != '\n' && in[i] != '\r') ? in[i++] : 32;

            uint8_t da = (a - 32) & 0x3f;
            uint8_t db = (b - 32) & 0x3f;
            uint8_t dc = (c - 32) & 0x3f;
            uint8_t dd = (d - 32) & 0x3f;

            if (produced < nbytes) out[out_pos++] = (da << 2) | (db >> 4);
            produced++;
            if (produced < nbytes) out[out_pos++] = (db << 4) | (dc >> 2);
            produced++;
            if (produced < nbytes) out[out_pos++] = (dc << 6) | dd;
            produced++;
        }

        // Skip to end of line
        while (i < in_len && in[i] != '\n') i++;
        if (i < in_len) i++; // skip newline
    }

    return out_pos;
}