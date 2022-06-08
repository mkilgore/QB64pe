#ifndef INCLUDE_LIBQB_QBS_H
#define INCLUDE_LIBQB_QBS_H

#include <stdint.h>

// QB64 string descriptor structure
struct qbs_field {
    int32_t fileno;
    int64_t fileid;
    int64_t size;
    int64_t offset;
};

struct qbs {
    uint8_t *chr;    // a 32 bit pointer to the string's data
    int32_t len;     // must be signed for comparisons against signed int32s
    uint8_t in_cmem; // set to 1 if in the conventional memory DBLOCK
    uint16_t *cmem_descriptor;
    uint16_t cmem_descriptor_offset;
    uint32_t listi;    // the index in the list of strings that references it
    uint8_t tmp;       // set to 1 if the string can be deleted immediately after being processed
    uint32_t tmplisti; // the index in the list of strings that references it
    uint8_t fixed;     // fixed length string
    uint8_t readonly;  // set to 1 if string is read only
    qbs_field *field;
};

qbs *qbs_new(int32_t, uint8_t);
qbs *qbs_new_txt(const char *);
qbs *qbs_add(qbs *, qbs *);
qbs *qbs_set(qbs *, qbs *);

qbs *qbs_lcase(qbs *str);
qbs *qbs_ucase(qbs *str);

qbs *qbs_str(int64 value);
qbs *qbs_str(int32 value);
qbs *qbs_str(int16 value);
qbs *qbs_str(int8 value);
qbs *qbs_str(uint64 value);
qbs *qbs_str(uint32 value);
qbs *qbs_str(uint16 value);
qbs *qbs_str(uint8 value);
qbs *qbs_str(float value);
qbs *qbs_str(double value);
qbs *qbs_str(long double value);

qbs *qbs_new_txt_len(const char *txt, int32 len);

void qbs_free(qbs *str);

qbs *qbs_left(qbs *str, int32 l);

qbs *qbs_new_cmem(int32 size, uint8 tmp);

#endif
