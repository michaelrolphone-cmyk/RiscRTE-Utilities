/* Synthetic PCM only: no microphone/device I/O. The fixture runs the production
 * canonical FFT512, background, 2 x 32 ms averaging, features and segmentation.
 * Build: cc -std=c11 -O1 -Wall -Wextra -Werror tests/spectrum_temporal_pcm_test.c -lm -o /tmp/spectrum_temporal_pcm_test
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../Apps/spectrum_temporal.h"

#define PCM_COLUMN_SAMPLES (SPECTRUM_DSP_RATE * ST_FRAME_MS / 1000u)
#define PCM_PI 3.14159265358979323846

typedef struct {
    spectrum_signature_analyzer fft;
    spectrum_background background;
    st_segmenter segment;
    uint32_t previous[128];
    uint64_t sample;
    unsigned columns;
} pcm_pipeline;
static pcm_pipeline pipeline;
static st_library library;
static st_matcher matcher;

/* Continuous oscillators avoid incidental boundary clicks. All event envelopes
 * are held for one 64 ms column, independent of FFT implementation details. */
static void pcm_column(pcm_pipeline *p, double hz, unsigned amplitude,
                       unsigned ambient_amplitude) {
    int16_t pcm[SPECTRUM_DSP_CHUNK];
    uint64_t pair[128] = {0};
    bool active = false;
    for (unsigned chunk = 0; chunk < PCM_COLUMN_SAMPLES / SPECTRUM_DSP_CHUNK; ++chunk) {
        for (unsigned i = 0; i < SPECTRUM_DSP_CHUNK; ++i, ++p->sample) {
            double t = (double)p->sample / SPECTRUM_DSP_RATE;
            double v = amplitude * sin(2.0 * PCM_PI * hz * t)
                     + ambient_amplitude * sin(2.0 * PCM_PI * 250.0 * t);
            assert(v >= -32768.0 && v <= 32767.0);
            pcm[i] = (int16_t)lround(v);
        }
        unsigned before = p->fft.transforms;
        assert(spectrum_signature_feed(&p->fft, pcm, SPECTRUM_DSP_CHUNK));
        if (p->fft.transforms == before) continue;
        spectrum_background_observe(&p->background, p->fft.power);
        uint32_t salient[128];
        spectrum_background_salient(&p->background, salient);
        for (unsigned band = 0; band < 128; ++band) pair[band] += salient[band];
        active |= p->background.foreground
               && spectrum_background_db(spectrum_signature_total(salient)) >= -6000;
    }
    uint32_t power[128];
    for (unsigned band = 0; band < 128; ++band) power[band] = (uint32_t)(pair[band] / 2u);
    st_frame frame = st_frame_make(power, p->previous, active);
    memcpy(p->previous, power, sizeof(power));
    if (p->background.ready) st_segment_observe(&p->segment, &frame);
    else st_segment_reset(&p->segment);
    ++p->columns;
    assert(p->fft.transforms == p->columns * 2u);
}

static void pcm_start(pcm_pipeline *p, unsigned ambient_amplitude) {
    memset(p, 0, sizeof(*p));
    spectrum_signature_init(&p->fft);
    for (unsigned column = 0; column < SPECTRUM_BG_WARMUP / 2u + ST_PRE + 2u; ++column) {
        pcm_column(p, 0.0, 0, ambient_amplitude);
        assert(!p->background.foreground && !p->segment.collecting && !p->segment.ready);
    }
    assert(p->background.ready && p->segment.used == ST_PRE);
    /* Match capture mode's explicit freeze of upward ambient adaptation. */
    p->background.freeze_upward = true;
}

static st_example pcm_event(double hz, unsigned amplitude, unsigned stretch,
                            unsigned ambient_amplitude, bool reversed) {
    static const unsigned envelope[] = {100, 90, 80, 65, 50, 35, 23, 14, 8, 4, 2, 1};
    pcm_start(&pipeline, ambient_amplitude);
    for (unsigned column = 0; column < sizeof(envelope) / sizeof(*envelope) * stretch; ++column) {
        unsigned index = column / stretch;
        if (reversed) index = (unsigned)(sizeof(envelope) / sizeof(*envelope)) - index - 1u;
        pcm_column(&pipeline, hz, amplitude * envelope[index] / 100u, ambient_amplitude);
    }
    for (unsigned tail = 0; tail < ST_PRE; ++tail) pcm_column(&pipeline, hz, 0, ambient_amplitude);
    assert(pipeline.segment.ready);
    st_example event = pipeline.segment.event;
    assert(st_example_valid(&event));
    /* Independent sine RMS threshold: -60 dBFS is 1073.742 PCM-square
     * mean power. Our levels are safely away from the quantization boundary.
     * A quiet decay therefore legitimately ends before the louder decay. */
    unsigned active_columns = 0;
    for (unsigned i = 0; i < sizeof(envelope) / sizeof(*envelope); ++i) {
        unsigned level = amplitude * envelope[i] / 100u;
        if ((uint64_t)level * level >= 2148u) active_columns += stretch;
    }
    assert(event.pre == ST_PRE && event.count == ST_PRE + active_columns + ST_PRE);
    assert(event.onset == ST_PRE && event.end == ST_PRE + active_columns - 1u);
    assert(event.duration_ms == active_columns * ST_FRAME_MS && !(event.flags & ST_CLIPPED));
    for (unsigned i = 0; i < ST_PRE; ++i) {
        assert(!(event.frames[i].flags & ST_ACTIVE));
        assert(!(event.frames[event.count - 1u - i].flags & ST_ACTIVE));
    }
    return event;
}

static unsigned similarity(const st_example *query, const st_example *saved,
                           unsigned limit, int *shift) {
    st_dtw work;
    st_dtw_start(&work, query, saved, limit);
    while (!work.done) st_dtw_step(&work, query, saved, 3);
    if (shift) *shift = work.shift;
    return 1000u - work.cost;
}

int main(void) {
    const double reference_hz = 2000.0;
    st_example reference = pcm_event(reference_hz, 12000, 1, 0, false);
    assert(st_tonal_example(&reference));
    assert(reference.impacts == 1);
    assert(similarity(&reference, &reference, 0, NULL) == 1000);
    /* Replay actual PCM-derived columns through the production stop path.
     * Explicit whole-event confirmation must compare identically despite
     * omitting the automatic four-column quiet terminator. */
    st_segmenter manual = {0};
    for (unsigned i = 0; i <= reference.end; ++i) st_segment_observe(&manual, &reference.frames[i]);
    assert(manual.collecting && !manual.ready);
    st_segment_stop(&manual);
    assert(manual.ready && st_example_valid(&manual.event));
    assert(st_partial(&manual.event));
    assert(similarity(&manual.event, &reference, 0, NULL) < ST_MATCH_MIN);
    manual.event.flags |= ST_CONFIRMED_END;
    assert(st_example_valid(&manual.event) && !st_partial(&manual.event));
    assert(manual.event.count + ST_PRE == reference.count);
    assert(similarity(&manual.event, &reference, 0, NULL) == 1000);
    assert(similarity(&reference, &manual.event, 0, NULL) == 1000);
    for (int shift = -2; shift <= 2; ++shift) {
        st_example shifted = pcm_event(reference_hz * pow(2.0, shift / 8.0), 12000, 1, 0, false);
        int selected_shift;
        unsigned score = similarity(&shifted, &reference, st_abs(shift), &selected_shift);
        printf("PCM pitch %+d: %u, selected %+d, measured %u/%u Hz\n", shift, score,
               selected_shift, st_frequency_evidence(&shifted), st_frequency_evidence(&reference));
        assert(score >= ST_MATCH_MIN && selected_shift == shift);
        if (shift) assert(similarity(&shifted, &reference, st_abs(shift) - 1u, NULL) < ST_MATCH_MIN);
    }
    for (int shift = -8; shift <= 8; ++shift) {
        if (shift >= -2 && shift <= 2) continue;
        st_example shifted = pcm_event(reference_hz * pow(2.0, shift / 8.0), 12000, 1, 0, false);
        assert(!st_absolute_frequency_compatible(&shifted, &reference, 2));
        assert(similarity(&shifted, &reference, 2, NULL) < ST_MATCH_MIN);
    }
    st_example quiet = pcm_event(reference_hz, 380, 1, 0, false);
    st_example slow = pcm_event(reference_hz, 12000, 2, 0, false);
    st_example too_slow = pcm_event(reference_hz, 12000, 3, 0, false);
    st_example reverse = pcm_event(reference_hz, 12000, 1, 0, true);
    st_example ambient = pcm_event(reference_hz, 12000, 1, 200, false);
    printf("PCM intensity %u (%.2f dB lower), tempo %u, reversed %u, ambient %u\n",
           similarity(&quiet, &reference, 1, NULL), (reference.peak_db - quiet.peak_db) / 100.0,
           similarity(&slow, &reference, 1, NULL), similarity(&reverse, &reference, 1, NULL),
           similarity(&ambient, &reference, 1, NULL));
    assert(reference.peak_db - quiet.peak_db >= 2900);
    assert(similarity(&quiet, &reference, 1, NULL) >= ST_MATCH_MIN);
    assert(similarity(&slow, &reference, 1, NULL) >= ST_MATCH_MIN);
    assert(similarity(&reference, &slow, 1, NULL) >= ST_MATCH_MIN);
    assert(similarity(&too_slow, &reference, 1, NULL) < ST_MATCH_MIN);
    assert(similarity(&reference, &too_slow, 1, NULL) < ST_MATCH_MIN);
    assert(similarity(&reverse, &reference, 1, NULL) < ST_MATCH_MIN);
    assert(similarity(&ambient, &reference, 1, NULL) >= ST_MATCH_MIN);
    /* A tiny high-SNR tone cannot borrow a much larger low-SNR ambient
     * fluctuation to cross the -60 dB event floor. All powers come from PCM. */
    pcm_start(&pipeline, 2000);
    pcm_column(&pipeline, reference_hz, 12, 2500);
    assert(pipeline.background.foreground);
    assert(spectrum_background_db(spectrum_signature_total(pipeline.background.excess)) > -6000);
    uint32_t salient[128];
    uint64_t salient_total = spectrum_background_salient(&pipeline.background, salient);
    assert(salient_total == spectrum_signature_total(salient));
    assert(spectrum_background_db(salient_total) < -6000);
    assert(!pipeline.segment.collecting && !pipeline.segment.ready);
    for (unsigned band = 0; band < 128; ++band)
        if (!pipeline.background.active[band]) assert(salient[band] == 0);
    /* Same absolute tone level can be ambient rather than an event. Verify
     * the real-PCM learned floor, then the 4x-power activation threshold and
     * return to quiet, without fabricating raw/excess power arrays. */
    pcm_start(&pipeline, 2000);
    pcm_column(&pipeline, 250.0, 500, 2000);
    assert(!pipeline.background.foreground && !pipeline.segment.collecting);
    pcm_column(&pipeline, 250.0, 4000, 2000);
    assert(pipeline.background.foreground && pipeline.segment.collecting);
    int16_t excess_db, snr_db;
    assert(spectrum_background_label(&pipeline.background, 250, -60, false, &excess_db, &snr_db));
    assert(snr_db >= 900);
    for (unsigned tail = 0; tail < ST_PRE; ++tail) pcm_column(&pipeline, 250.0, 0, 2000);
    assert(!pipeline.background.foreground && pipeline.segment.ready);
    assert(st_example_valid(&pipeline.segment.event));
    assert(pipeline.segment.event.duration_ms == ST_FRAME_MS);
    st_label *label = &library.labels[0];
    label->present = true; label->shift_limit = 2; label->next_id = 4;
    strcpy(label->name, "PCM decay");
    label->examples[0] = reference;
    label->examples[1] = slow; label->examples[1].id = 2;
    label->examples[2] = reverse; label->examples[2].id = 3; label->examples[2].kind = ST_NEGATIVE;
    assert(st_label_valid(label));
    st_match_begin(&matcher, &quiet);
    while (matcher.running) st_match_tick(&matcher, &library, 64);
    assert(matcher.reason == ST_RESULT_MATCH && matcher.selected == 0);
    label->examples[3] = quiet; label->examples[3].id = 4; label->examples[3].kind = ST_NEGATIVE;
    label->next_id = 5;
    st_match_begin(&matcher, &quiet);
    while (matcher.running) st_match_tick(&matcher, &library, 64);
    assert(matcher.reason == ST_RESULT_NEGATIVE && matcher.selected == -1);
    puts("Temporal PCM regression passed: actual FFT/background/features/segmentation, +/-1 and +/-2 pitch, bounded frequency, ~30 dB level, tempo, ambient salience, confirmed-end equivalence, positive and negative examples; synthetic host evidence only.");
    return 0;
}
