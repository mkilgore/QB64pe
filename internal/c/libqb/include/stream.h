
#pragma once

#include <stdint.h>

#include "byte-element.h"
#include "error_handle.h"

enum class special_handle_type {
    Invalid,
    Stream,
    Host,
    Http,
    Custom,
};

class special_handle {
  public:
    int32_t id;

    virtual void close() = 0;

    virtual void get(int64_t offset, byte_element_struct *ele, int passed) {
        (void)offset; (void)ele; (void)passed;
        error(QB_ERROR_BAD_FILE_NAME_OR_NUMBER);
    }

    virtual void get2(int64_t offset, qbs *retstr, int passed) {
        (void)offset; (void)retstr; (void)passed;
        error(QB_ERROR_BAD_FILE_NAME_OR_NUMBER);
    }

    virtual void put(int64_t offset, byte_element_struct *ele, int passed) {
        (void)offset; (void)ele; (void)passed;
        error(QB_ERROR_BAD_FILE_NAME_OR_NUMBER);
    }

    virtual int64_t lof() {
        error(QB_ERROR_BAD_FILE_NAME_OR_NUMBER);
        return 0;
    }

    virtual int32_t eof() {
        error(QB_ERROR_BAD_FILE_NAME_OR_NUMBER);
        return 0;
    }

    virtual void connectionaddress(qbs *retstr) {
        (void)retstr;
        error(QB_ERROR_BAD_FILE_NAME_OR_NUMBER);
    }

    virtual int32_t connected() {
        error(QB_ERROR_BAD_FILE_NAME_OR_NUMBER);
        return 0;
    }
};

void special_handle_custom_open(special_handle *);
void special_handle_close(special_handle *);
