#ifndef SECSGML_H
#define SECSGML_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const uint8_t *ptr;
    size_t len;
} byte_span;

typedef struct {
    byte_span type;
    byte_span sequence;
    byte_span filename;
    byte_span description;
} document_meta;

typedef struct {
    document_meta meta;
    byte_span content;
    uint8_t *owned_content;
} document;

typedef struct {
    document *docs;
    size_t doc_count;
} sgml_parse_result;

typedef struct {
    double decode_ms;
} sgml_parse_stats;

typedef enum {
    SUB_EVENT_SECTION_START = 1,
    SUB_EVENT_SECTION_END = 2,
    SUB_EVENT_KEYVAL = 3
} submission_event_type;

typedef struct {
    submission_event_type type;
    byte_span key;
    byte_span value;
    int depth;
} submission_event;

typedef struct {
    submission_event *events;
    size_t count;
} submission_metadata;

sgml_parse_result parse_sgml(const uint8_t *buf, size_t len, sgml_parse_stats *stats);
void free_sgml_parse_result(sgml_parse_result *r);
submission_metadata parse_submission_metadata(const uint8_t *buf, size_t len);
void free_submission_metadata(submission_metadata *m);

#endif
