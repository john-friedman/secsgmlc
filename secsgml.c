#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#include "secsgml.h"
#include "uudecode.h"

#ifdef _WIN32
#include <windows.h>
static double now_ms(void) {
    static LARGE_INTEGER freq = {0};
    LARGE_INTEGER counter;
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
    }
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart * 1000.0 / (double)freq.QuadPart;
}
#else
#include <time.h>
static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}
#endif

static int is_space(uint8_t c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static byte_span trim_span(byte_span s) {
    while (s.len > 0 && is_space(s.ptr[0])) {
        s.ptr++;
        s.len--;
    }
    while (s.len > 0 && is_space(s.ptr[s.len - 1])) {
        s.len--;
    }
    return s;
}

static byte_span ltrim_span(byte_span s) {
    while (s.len > 0 && is_space(s.ptr[0])) {
        s.ptr++;
        s.len--;
    }
    return s;
}

static const uint8_t *find_subspan(const uint8_t *hay, size_t hay_len, const char *needle, size_t needle_len) {
    if (needle_len == 0 || hay_len < needle_len) return NULL;
    const uint8_t *end = hay + hay_len - needle_len;
    for (const uint8_t *p = hay; p <= end; p++) {
        if (memcmp(p, needle, needle_len) == 0) return p;
    }
    return NULL;
}

static const uint8_t *find_eol(const uint8_t *p, const uint8_t *end) {
    while (p < end && *p != '\n' && *p != '\r') p++;
    return p;
}

static const uint8_t *skip_eol(const uint8_t *p, const uint8_t *end) {
    if (p < end && *p == '\r') p++;
    if (p < end && *p == '\n') p++;
    return p;
}

static size_t find_double_newline(const uint8_t *buf, size_t len) {
    for (size_t i = 0; i + 1 < len; i++) {
        if (buf[i] == '\n' && buf[i + 1] == '\n') return i + 2;
        if (buf[i] == '\r' && i + 3 < len && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') {
            return i + 4;
        }
    }
    return len;
}

static byte_span read_tag_value(const uint8_t *start, size_t len, const char *tag) {
    size_t tag_len = strlen(tag);
    const uint8_t *tag_pos = find_subspan(start, len, tag, tag_len);
    if (!tag_pos) return (byte_span){0};
    const uint8_t *val_start = tag_pos + tag_len;
    const uint8_t *range_end = start + len;
    const uint8_t *val_end = find_eol(val_start, range_end);
    byte_span s = { val_start, (size_t)(val_end - val_start) };
    return trim_span(s);
}

static int is_begin_644_line(const uint8_t *p, size_t len) {
    const char *prefix = "begin 644";
    size_t n = strlen(prefix);
    if (len < n) return 0;
    if (memcmp(p, prefix, n) != 0) return 0;
    if (len == n) return 1;
    return p[n] == ' ' || p[n] == '\t';
}

static int is_end_line(const uint8_t *p, size_t len) {
    if (len < 3) return 0;
    if (memcmp(p, "end", 3) != 0) return 0;
    if (len == 3) return 1;
    return is_space(p[3]);
}

static byte_span decode_if_uuencoded(const uint8_t *text_start, const uint8_t *text_end, uint8_t **owned_out, sgml_parse_stats *stats) {
    const uint8_t *p = text_start;
    const uint8_t *begin_line = NULL;
    const uint8_t *begin_line_end = NULL;

    for (int line = 0; line < 3 && p < text_end; line++) {
        const uint8_t *line_end = find_eol(p, text_end);
        size_t line_len = (size_t)(line_end - p);
        if (is_begin_644_line(p, line_len)) {
            begin_line = p;
            begin_line_end = line_end;
            break;
        }
        p = skip_eol(line_end, text_end);
    }

    if (!begin_line) {
        *owned_out = NULL;
        // Trim leading/trailing whitespace for non-uuencoded content.
        while (text_start < text_end && is_space(*text_start)) text_start++;
        while (text_end > text_start && is_space(*(text_end - 1))) text_end--;

        // Strip simple wrappers if present: <PDF>...</PDF>, <XBRL>...</XBRL>, <XML>...</XML>.
        if ((size_t)(text_end - text_start) >= 5 && memcmp(text_start, "<PDF>", 5) == 0) {
            text_start += 5;
            while (text_start < text_end && is_space(*text_start)) text_start++;
        } else if ((size_t)(text_end - text_start) >= 6 && memcmp(text_start, "<XBRL>", 6) == 0) {
            text_start += 6;
            while (text_start < text_end && is_space(*text_start)) text_start++;
        } else if ((size_t)(text_end - text_start) >= 5 && memcmp(text_start, "<XML>", 5) == 0) {
            text_start += 5;
            while (text_start < text_end && is_space(*text_start)) text_start++;
        }

        if ((size_t)(text_end - text_start) >= 6 && memcmp(text_end - 6, "</PDF>", 6) == 0) {
            text_end -= 6;
            while (text_end > text_start && is_space(*(text_end - 1))) text_end--;
        } else if ((size_t)(text_end - text_start) >= 7 && memcmp(text_end - 7, "</XBRL>", 7) == 0) {
            text_end -= 7;
            while (text_end > text_start && is_space(*(text_end - 1))) text_end--;
        } else if ((size_t)(text_end - text_start) >= 6 && memcmp(text_end - 6, "</XML>", 6) == 0) {
            text_end -= 6;
            while (text_end > text_start && is_space(*(text_end - 1))) text_end--;
        }

        return (byte_span){ text_start, (size_t)(text_end - text_start) };
    }

    const uint8_t *enc_start = skip_eol(begin_line_end, text_end);
    const uint8_t *enc_end = text_end;

    const uint8_t *scan = enc_start;
    while (scan < text_end) {
        const uint8_t *line_end = find_eol(scan, text_end);
        size_t line_len = (size_t)(line_end - scan);
        if (is_end_line(scan, line_len)) {
            enc_end = scan;
            break;
        }
        scan = skip_eol(line_end, text_end);
    }

    size_t enc_len = (size_t)(enc_end - enc_start);
    uint8_t *out_buf = (uint8_t *)malloc(enc_len ? enc_len : 1);
    if (!out_buf) {
        *owned_out = NULL;
        return (byte_span){0};
    }

    double t0 = now_ms();
    size_t out_len = uudecode(enc_start, enc_len, out_buf);
    double t1 = now_ms();
    if (stats) stats->decode_ms += (t1 - t0);

    *owned_out = out_buf;
    return (byte_span){ out_buf, out_len };
}

sgml_parse_result parse_sgml(const uint8_t *buf, size_t len, sgml_parse_stats *stats) {
    sgml_parse_result result = {0};
    const char *doc_open = "<DOCUMENT>";
    const char *doc_close = "</DOCUMENT>";
    const char *text_open = "<TEXT>";
    const char *text_close = "</TEXT>";

    const uint8_t *p = buf;
    const uint8_t *end = buf + len;

    while (p < end) {
        const uint8_t *doc_start = find_subspan(p, (size_t)(end - p), doc_open, strlen(doc_open));
        if (!doc_start) break;
        const uint8_t *doc_end = find_subspan(doc_start, (size_t)(end - doc_start), doc_close, strlen(doc_close));
        if (!doc_end) break;

        const uint8_t *doc_body_start = doc_start + strlen(doc_open);
        size_t doc_body_len = (size_t)(doc_end - doc_body_start);

        const uint8_t *text_tag = find_subspan(doc_body_start, doc_body_len, text_open, strlen(text_open));
        if (!text_tag) {
            p = doc_end + strlen(doc_close);
            continue;
        }

        const uint8_t *text_end_tag = find_subspan(text_tag, (size_t)(end - text_tag), text_close, strlen(text_close));
        if (!text_end_tag) {
            p = doc_end + strlen(doc_close);
            continue;
        }

        const uint8_t *meta_start = doc_body_start;
        size_t meta_len = (size_t)(text_tag - meta_start);

        document doc = {0};
        doc.meta.type = read_tag_value(meta_start, meta_len, "<TYPE>");
        doc.meta.sequence = read_tag_value(meta_start, meta_len, "<SEQUENCE>");
        doc.meta.filename = read_tag_value(meta_start, meta_len, "<FILENAME>");
        doc.meta.description = read_tag_value(meta_start, meta_len, "<DESCRIPTION>");

        const uint8_t *text_start = text_tag + strlen(text_open);
        const uint8_t *text_end = text_end_tag;

        doc.content = decode_if_uuencoded(text_start, text_end, &doc.owned_content, stats);

        document *new_docs = (document *)realloc(result.docs, (result.doc_count + 1) * sizeof(document));
        if (!new_docs) {
            if (doc.owned_content) free(doc.owned_content);
            break;
        }
        result.docs = new_docs;
        result.docs[result.doc_count++] = doc;

        p = doc_end + strlen(doc_close);
    }

    return result;
}

void free_sgml_parse_result(sgml_parse_result *r) {
    if (!r) return;
    for (size_t i = 0; i < r->doc_count; i++) {
        if (r->docs[i].owned_content) free(r->docs[i].owned_content);
    }
    free(r->docs);
    r->docs = NULL;
    r->doc_count = 0;
}

static void add_event(submission_metadata *m, submission_event_type type, byte_span key, byte_span value, int depth) {
    submission_event *new_events = (submission_event *)realloc(m->events, (m->count + 1) * sizeof(submission_event));
    if (!new_events) return;
    m->events = new_events;
    m->events[m->count].type = type;
    m->events[m->count].key = key;
    m->events[m->count].value = value;
    m->events[m->count].depth = depth;
    m->count++;
}

static void parse_archive_metadata(submission_metadata *m, const uint8_t *buf, size_t len) {
    const uint8_t *p = buf;
    const uint8_t *end = buf + len;
    int depth = 0;

    while (p < end) {
        const uint8_t *line_end = find_eol(p, end);
        byte_span line = { p, (size_t)(line_end - p) };
        line = ltrim_span(line);
        if (line.len == 0) {
            p = skip_eol(line_end, end);
            continue;
        }

        if (line.ptr[0] == '<') {
            const uint8_t *gt = memchr(line.ptr, '>', line.len);
            if (gt) {
                byte_span key = { line.ptr + 1, (size_t)(gt - (line.ptr + 1)) };
                byte_span value = { gt + 1, (size_t)(line.ptr + line.len - (gt + 1)) };
                key = trim_span(key);
                value = trim_span(value);
                if (key.len > 0 && key.ptr[0] == '/') {
                    if (depth > 0) depth--;
                    add_event(m, SUB_EVENT_SECTION_END, key, (byte_span){0}, depth);
                } else if (value.len == 0) {
                    add_event(m, SUB_EVENT_SECTION_START, key, (byte_span){0}, depth);
                    depth++;
                } else {
                    add_event(m, SUB_EVENT_KEYVAL, key, value, depth);
                }
            }
        }

        p = skip_eol(line_end, end);
    }

    while (depth > 0) {
        depth--;
        add_event(m, SUB_EVENT_SECTION_END, (byte_span){0}, (byte_span){0}, depth);
    }
}

static void parse_tab_metadata(submission_metadata *m, const uint8_t *buf, size_t len) {
    const uint8_t *p = buf;
    const uint8_t *end = buf + len;
    int depth = 0;

    while (p < end) {
        const uint8_t *line_end = find_eol(p, end);
        byte_span line = { p, (size_t)(line_end - p) };
        if (line.len == 0) {
            p = skip_eol(line_end, end);
            continue;
        }

        size_t indent = 0;
        while (indent < line.len && line.ptr[indent] == '\t') indent++;
        byte_span content = { line.ptr + indent, line.len - indent };
        content = trim_span(content);
        if (content.len == 0) {
            p = skip_eol(line_end, end);
            continue;
        }

        while (depth > (int)indent) {
            depth--;
            add_event(m, SUB_EVENT_SECTION_END, (byte_span){0}, (byte_span){0}, depth);
        }

        const uint8_t *colon = memchr(content.ptr, ':', content.len);
        if (colon) {
            byte_span key = { content.ptr, (size_t)(colon - content.ptr) };
            byte_span value = { colon + 1, (size_t)(content.ptr + content.len - (colon + 1)) };
            key = trim_span(key);
            value = trim_span(value);

            if (value.len == 0) {
                add_event(m, SUB_EVENT_SECTION_START, key, (byte_span){0}, depth);
                depth++;
            } else {
                add_event(m, SUB_EVENT_KEYVAL, key, value, depth);
            }
        } else if (content.ptr[0] == '<') {
            const uint8_t *gt = memchr(content.ptr, '>', content.len);
            if (gt) {
                byte_span key = { content.ptr + 1, (size_t)(gt - (content.ptr + 1)) };
                byte_span value = { gt + 1, (size_t)(content.ptr + content.len - (gt + 1)) };
                key = trim_span(key);
                value = trim_span(value);
                if (key.len > 0 && key.ptr[0] == '/') {
                    if (depth > 0) depth--;
                    add_event(m, SUB_EVENT_SECTION_END, key, (byte_span){0}, depth);
                } else if (value.len == 0) {
                    add_event(m, SUB_EVENT_SECTION_START, key, (byte_span){0}, depth);
                    depth++;
                } else {
                    add_event(m, SUB_EVENT_KEYVAL, key, value, depth);
                }
            }
        }

        p = skip_eol(line_end, end);
    }

    while (depth > 0) {
        depth--;
        add_event(m, SUB_EVENT_SECTION_END, (byte_span){0}, (byte_span){0}, depth);
    }
}

submission_metadata parse_submission_metadata(const uint8_t *buf, size_t len) {
    submission_metadata m = {0};

    const uint8_t *doc_start = find_subspan(buf, len, "<DOCUMENT>", strlen("<DOCUMENT>"));
    size_t sub_len = doc_start ? (size_t)(doc_start - buf) : len;
    if (sub_len == 0) return m;

    byte_span sub = { buf, sub_len };
    sub = ltrim_span(sub);
    if (sub.len == 0) return m;

    if (sub.ptr[0] == '-') {
        size_t privacy_end = find_double_newline(sub.ptr, sub.len);
        if (privacy_end > 0 && privacy_end < sub.len) {
            byte_span key = { (const uint8_t *)"PRIVACY-ENHANCED-MESSAGE", strlen("PRIVACY-ENHANCED-MESSAGE") };
            byte_span value = { sub.ptr, privacy_end };
            add_event(&m, SUB_EVENT_KEYVAL, key, value, 0);
            sub.ptr += privacy_end;
            sub.len -= privacy_end;
            sub = ltrim_span(sub);
        }
        parse_tab_metadata(&m, sub.ptr, sub.len);
    } else if (sub.len >= 3 && memcmp(sub.ptr, "<SE", 3) == 0) {
        parse_tab_metadata(&m, sub.ptr, sub.len);
    } else {
        parse_archive_metadata(&m, sub.ptr, sub.len);
    }

    return m;
}

void free_submission_metadata(submission_metadata *m) {
    if (!m) return;
    free(m->events);
    m->events = NULL;
    m->count = 0;
}
