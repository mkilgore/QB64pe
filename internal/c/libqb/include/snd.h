#ifndef INCLUDE_LIBQB_SND_H
#define INCLUDE_LIBQB_SND_H

#include "libqb-list.h"
#include <stdint.h>

struct snd_sequence_struct {
    uint16_t *data;
    int32_t data_size;
    uint8_t channels; // note: more than 2 channels may be supported in the future
    uint16_t *data_left;
    int32_t data_left_size;
    uint16_t *data_right;
    int32_t data_right_size;

    // origins of data (only relevent before src)
    uint8_t endian;          // 0=native, 1=little(Windows, x86), 2=big(Motorola, Xilinx Microblaze, IBM POWER)
    uint8_t is_unsigned;     // 1=unsigned, 0=signed(most common)
    int32_t sample_rate;     // eg. 11025, 22100
    int32_t bits_per_sample; // eg. 8, 16

    int32_t references; // number of SND handles dependent on this
};

// This list holds all of the snd_sequence_struct instances
extern list *snd_sequences;

#endif
