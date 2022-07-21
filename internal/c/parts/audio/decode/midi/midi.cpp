
#include "libqb-common.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "snd.h"
#include "tml.h"
#include "tsf.h"
#include "soundfont.h"
#include "midi.h"

// Number of samples per second
#define SAMPLE_RATE (44100)

// The loaded soundfont to be used for playing MIDI content
// If none is provided, then MIDI support is disabled
static tsf *midi_soundfont;

static void snd_sequence_initialize(snd_sequence_struct *seq)
{
    memset(seq, 0, sizeof(snd_sequence_struct));
    seq->references = 1;

    seq->channels = 2;
    seq->sample_rate = SAMPLE_RATE;
    seq->bits_per_sample = 16;
    seq->endian = 1; // little endian
    seq->is_unsigned = 0;
}

static void render_samples(snd_sequence_struct *seq, tml_message *current_msg, size_t total_samples)
{
    size_t sample_size = sizeof(int16_t);
    size_t channel_count = 2;

    size_t buffersize = total_samples * channel_count * sample_size;
    int16_t *buffer = (int16_t *)malloc(buffersize);
    memset(buffer, 0, buffersize);

    seq->data = (uint16_t *)buffer;
    seq->data_size = buffersize;

    size_t block_sample_count = 512;

    for (size_t block = 0; block * block_sample_count < total_samples; block++) {
        // Calculate where in time this block starts, in ms
        //
        // Calculating it every loop helps avoid any compounding rounding errors
        int current_time = ((block * block_sample_count) * 1000) / SAMPLE_RATE;

        for (; current_msg && current_msg->time <= current_time; current_msg = current_msg->next) {
            switch (current_msg->type) {
            case TML_PROGRAM_CHANGE:
                // Special handling for percussion channel
                tsf_channel_set_presetnumber(midi_soundfont, current_msg->channel, current_msg->program, current_msg->channel == 9);
                break;

            case TML_NOTE_ON:
                tsf_channel_note_on(midi_soundfont, current_msg->channel, current_msg->key, current_msg->velocity / 127.0f);
                break;

            case TML_NOTE_OFF:
                tsf_channel_note_off(midi_soundfont, current_msg->channel, current_msg->key);
                break;

            case TML_PITCH_BEND:
                tsf_channel_set_pitchwheel(midi_soundfont, current_msg->channel, current_msg->pitch_bend);
                break;

            case TML_CONTROL_CHANGE:
                tsf_channel_midi_control(midi_soundfont, current_msg->channel, current_msg->control, current_msg->control_value);
                break;
            }
        }

        size_t render_length = block_sample_count;

        // The last buffer might not be an exact block length in size
        if (total_samples < block * block_sample_count)
            render_length = total_samples - (block - 1) * block_sample_count;

        tsf_render_short(midi_soundfont, buffer + block * block_sample_count * channel_count, render_length, 0);
    }
}

void midi_init(void)
{
    midi_soundfont = tsf_load_memory(SOUNDFONT_BIN, SOUNDFONT_SIZE);

    // If the soundfont doesn't load for some reason then MIDI support will be left disabled
    if (!midi_soundfont)
        return;

    // Initialize preset on 10th MIDI channel to use percussion sound bank (128) if available
    tsf_channel_set_bank_preset(midi_soundfont, 9, 128, 0);

    tsf_set_output(midi_soundfont, TSF_STEREO_INTERLEAVED, SAMPLE_RATE, 0);
}

int midi_file_check(const uint8_t *content, size_t length)
{
    if (!midi_soundfont)
        return 0;

    // Logic largely from tml_load() implementation
    return length > 14 && strncmp((const char *)content, "MThd", 4) == 0;
}

snd_sequence_struct *snd_decode_midi(const uint8_t *content, size_t length)
{
    snd_sequence_struct *seq = NULL;

    if (!midi_soundfont)
        return NULL;

    tml_message *midi_file = tml_load_memory(content, length);
    if (!midi_file)
        return NULL;

    unsigned int total_length_ms;

    int ret = tml_get_info(midi_file, NULL, NULL, NULL, NULL, &total_length_ms);

    int32_t seq_handle = list_add(snd_sequences);

    seq = (snd_sequence_struct *)list_get(snd_sequences, seq_handle);
    snd_sequence_initialize(seq);

    // Make sure that we start from a state with nothing playing
    tsf_reset(midi_soundfont);

    // Determine the total number of samples that will make up this MIDI playback
    size_t total_samples = (total_length_ms * SAMPLE_RATE) / 1000;

    render_samples(seq, midi_file, total_samples);

  cleanup:
    if (midi_file)
        tml_free(midi_file);

    return seq;
}
