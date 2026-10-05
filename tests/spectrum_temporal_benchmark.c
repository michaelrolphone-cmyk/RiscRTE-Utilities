/* Deterministic matching-work bounds plus optional host CPU timing.
 * Synthetic PCM only. Timings do not qualify Watch hardware real-time behavior.
 * Build: cc -std=c11 -O1 -Wall -Wextra -Werror tests/spectrum_temporal_benchmark.c -o /tmp/spectrum_temporal_benchmark
 */
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include "../Apps/spectrum_temporal.h"

#define MATCH_BUDGET 64u
#define SETUP_UNITS 8u
#define HOST_REPETITIONS 24u
static spectrum_signature_analyzer fft;
static spectrum_background background;
static st_segmenter segment;
static st_library library;
static st_matcher matcher;
static uint32_t previous[128];
static uint64_t sample;
static unsigned tone_multiplier;

typedef struct {
    unsigned ticks, cells, units, peak_tick_units, peak_tick_cells, comparisons;
    double elapsed_ms, peak_tick_ms;
} measurements;

/* Same canonical 16 kHz, FFT512 and two-frame feature cadence as the app. */
static void feed_column(bool active) {
    static const int16_t wave[8] = {0, 8485, 12000, 8485, 0, -8485, -12000, -8485};
    int16_t pcm[SPECTRUM_DSP_CHUNK];
    uint64_t pair[128] = {0};
    bool foreground = false;
    for (unsigned chunk = 0; chunk < 4; ++chunk) {
        for (unsigned i = 0; i < SPECTRUM_DSP_CHUNK; ++i, ++sample)
            pcm[i] = active ? wave[(sample * tone_multiplier) % 8u] : 0;
        unsigned before = fft.transforms;
        assert(spectrum_signature_feed(&fft, pcm, SPECTRUM_DSP_CHUNK));
        if (fft.transforms == before) continue;
        spectrum_background_observe(&background, fft.power);
        uint32_t salient[128];
        spectrum_background_salient(&background, salient);
        foreground |= background.foreground
                   && spectrum_background_db(spectrum_signature_total(salient)) >= -6000;
        for (unsigned band = 0; band < 128; ++band) pair[band] += salient[band];
    }
    uint32_t power[128];
    for (unsigned band = 0; band < 128; ++band) power[band] = (uint32_t)(pair[band] / 2u);
    st_frame frame = st_frame_make(power, previous, foreground);
    memcpy(previous, power, sizeof(power));
    if (background.ready) st_segment_observe(&segment, &frame);
    else st_segment_reset(&segment);
}

static st_example maximum_pcm_event(bool have_precontext, bool octave) {
    spectrum_signature_init(&fft);
    spectrum_background_reset(&background);
    st_segment_reset(&segment);
    memset(previous, 0, sizeof(previous));
    sample = 0;
    tone_multiplier = octave ? 2u : 1u;
    for (unsigned i = 0; i < SPECTRUM_BG_WARMUP / 2u + ST_PRE; ++i) feed_column(false);
    assert(background.ready && segment.used == ST_PRE && !segment.ready);
    background.freeze_upward = true;
    if (!have_precontext) st_segment_reset(&segment);
    /* Normal storage retains four context + 56 active + four tail columns,
     * but DTW aligns only onset..end. The legal absolute maximum starts with
     * no precontext, contains64 active columns and is explicitly clipped. */
    unsigned active_columns = have_precontext ? ST_FRAMES - 2u * ST_PRE : ST_FRAMES;
    for (unsigned i = 0; i < active_columns; ++i) feed_column(true);
    if (have_precontext) {
        assert(!segment.ready);
        for (unsigned i = 0; i < ST_PRE; ++i) feed_column(false);
    }
    assert(segment.ready && segment.event.count == ST_FRAMES);
    assert(segment.event.flags == (have_precontext ? 0u : ST_CLIPPED));
    assert(segment.event.pre == (have_precontext ? ST_PRE : 0u));
    assert(segment.event.duration_ms == active_columns * ST_FRAME_MS);
    assert(st_example_valid(&segment.event));
    return segment.event;
}

static void fill_library(const st_example *event) {
    memset(&library, 0, sizeof(library));
    for (unsigned i = 0; i < ST_LABELS; ++i) {
        st_label *label = &library.labels[i];
        label->present = true; label->shift_limit = ST_MAX_SHIFT;
        snprintf(label->name, sizeof(label->name), "Worst case %u", i + 1u);
        label->next_id = ST_EXAMPLES + 1u;
        for (unsigned j = 0; j < ST_EXAMPLES; ++j) {
            label->examples[j] = *event;
            label->examples[j].id = j + 1u;
            /* Both kinds are visited, including nonwinning negative evidence. */
            label->examples[j].kind = j % 2u ? ST_NEGATIVE : ST_POSITIVE;
        }
        assert(st_label_valid(label));
    }
}

/* Derive work independently from matcher counters; a partially started coarse
 * comparison costs the full setup allowance and completed DTW rows cost one.
 * All examples are identical, so none can short-circuit or prune a DTW. */
static unsigned actual_units(const st_matcher *m, unsigned rows) {
    unsigned starts = m->compared + (m->work_started ? 1u : 0u);
    unsigned complete_rows = m->compared * rows;
    if (m->work_started) complete_rows += m->work.done ? rows : m->work.i - 1u;
    return starts * SETUP_UNITS + complete_rows;
}
static unsigned actual_cells(const st_matcher *m) {
    return m->cells + (m->work_started ? m->work.cells : 0u);
}

static measurements measure(const st_example *query, unsigned budget, bool report) {
    measurements result = {0};
    const unsigned rows = query->end - query->onset + 1u;
    st_match_begin(&matcher, query);
    st_matcher before_zero = matcher;
    st_match_tick(&matcher, &library, 0);
    assert(!memcmp(&matcher, &before_zero, sizeof(matcher)));
    st_match_tick(&matcher, &library, SETUP_UNITS - 1u);
    assert(!memcmp(&matcher, &before_zero, sizeof(matcher)));
    clock_t start = clock();
    while (matcher.running) {
        unsigned units_before = actual_units(&matcher, rows);
        unsigned cells_before = actual_cells(&matcher);
        unsigned charged_before = matcher.work_units;
        clock_t tick_start = clock();
        st_match_tick(&matcher, &library, budget);
        double tick_ms = (double)(clock() - tick_start) * 1000.0 / CLOCKS_PER_SEC;
        unsigned units = actual_units(&matcher, rows) - units_before;
        unsigned cells = actual_cells(&matcher) - cells_before;
        assert(matcher.work_units - charged_before <= budget);
        assert(cells <= budget * (2u * ST_DTW_RADIUS + 1u));
        if (units > result.peak_tick_units) result.peak_tick_units = units;
        if (cells > result.peak_tick_cells) result.peak_tick_cells = cells;
        if (tick_ms > result.peak_tick_ms) result.peak_tick_ms = tick_ms;
        ++result.ticks;
        /* A 48-example maximum must return to its caller after each budget,
         * never perform the complete library inside one UI/audio tick. */
        if (result.ticks == 1u) assert(matcher.running && matcher.compared < ST_LABELS * ST_EXAMPLES);
        assert(result.ticks <= ST_LABELS * ST_EXAMPLES * (ST_FRAMES + SETUP_UNITS));
    }
    result.elapsed_ms = (double)(clock() - start) * 1000.0 / CLOCKS_PER_SEC;
    result.cells = actual_cells(&matcher);
    result.units = actual_units(&matcher, rows);
    result.comparisons = matcher.compared;
    assert(matcher.complete && result.comparisons == ST_LABELS * ST_EXAMPLES);
    assert(matcher.reason == ST_RESULT_AMBIGUOUS && matcher.selected == -1);
    /* Equal-length proportional DTW has nine cells per interior row, with
     * 1+2+3+4 cells removed at each end of the corridor. */
    assert(result.cells == ST_LABELS * ST_EXAMPLES * (rows * (2u * ST_DTW_RADIUS + 1u) - ST_DTW_RADIUS * (ST_DTW_RADIUS + 1u)));
    assert(result.units == ST_LABELS * ST_EXAMPLES * (rows + SETUP_UNITS));
    assert(result.units == matcher.work_units);
    if (report) printf("budget=%u, pre=%u: %u comparisons, %u rows/example, %u cells, %u actual units, %u charged units, %u ticks, max %u units/tick, %u cells/tick; %.3f ms total, %.3f ms peak tick (host only)\n",
           budget, query->pre, result.comparisons, rows, result.cells, result.units,
           matcher.work_units, result.ticks, result.peak_tick_units, result.peak_tick_cells,
           result.elapsed_ms, result.peak_tick_ms);
    assert(result.peak_tick_units <= budget);
    return result;
}

static void rejected_setup_budget(const st_example *query) {
    st_match_begin(&matcher, query);
    unsigned ticks = 0;
    while (matcher.running) {
        unsigned before = matcher.compared;
        unsigned charged = matcher.work_units;
        st_match_tick(&matcher, &library, MATCH_BUDGET);
        assert((matcher.compared - before) * SETUP_UNITS <= MATCH_BUDGET);
        assert(matcher.work_units - charged <= MATCH_BUDGET);
        assert(matcher.cells == 0);
        ++ticks;
        if (ticks == 1u) assert(matcher.running);
        assert(ticks <= ST_LABELS * ST_EXAMPLES + 1u);
    }
    assert(matcher.compared == ST_LABELS * ST_EXAMPLES);
    assert(matcher.complete && matcher.reason == ST_RESULT_UNKNOWN && matcher.selected == -1);
    printf("48 frequency-rejected comparisons: %u ticks, no DTW cells; setup remains budgeted.\n", ticks);
}

int main(void) {
    puts("Temporal matcher host CPU evidence only; no hardware real-time qualification.");
    st_example normal = maximum_pcm_event(true, false);
    fill_library(&normal);
    measurements baseline = measure(&normal, MATCH_BUDGET, true);
    double total = baseline.elapsed_ms, peak_tick = baseline.peak_tick_ms;
    for (unsigned i = 1; i < HOST_REPETITIONS; ++i) {
        measurements next = measure(&normal, MATCH_BUDGET, false);
        assert(next.ticks == baseline.ticks && next.cells == baseline.cells && next.units == baseline.units);
        total += next.elapsed_ms;
        if (next.peak_tick_ms > peak_tick) peak_tick = next.peak_tick_ms;
    }
    measurements minimum = measure(&normal, SETUP_UNITS, true);
    assert(minimum.cells == baseline.cells && minimum.units == baseline.units);
    st_example octave = maximum_pcm_event(true, true);
    assert(!st_absolute_frequency_compatible(&octave, &normal, ST_MAX_SHIFT));
    rejected_setup_budget(&octave);
    st_example maximum = maximum_pcm_event(false, false);
    fill_library(&maximum);
    measurements edge = measure(&maximum, MATCH_BUDGET, true);
    assert(edge.cells >= baseline.cells && edge.units >= baseline.units);
    printf("Fixed maximum library %u labels x %u examples x %u columns; library RAM %zu, matcher RAM %zu bytes; %u-run mean %.3f ms, worst observed tick %.3f ms.\n",
           ST_LABELS, ST_EXAMPLES, ST_FRAMES, sizeof(library), sizeof(matcher), HOST_REPETITIONS,
           total / HOST_REPETITIONS, peak_tick);
    return 0;
}
