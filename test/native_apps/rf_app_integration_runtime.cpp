/* Production RF codecs/client against the real Runtime AppDataFiles backend.
 * Host-only: argv[1] is an empty temporary directory, never a device mount.
 * Namespace authority and executable lifetime remain Runtime-suite concerns. */
#define _Static_assert static_assert
#include "../../Apps/rf_temporal_files.h"
#include "../../Apps/rf_neural_store.h"
#include "runtime/storage/AppDataFiles.h"
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <unistd.h>

using RiscStorage::AppDataFiles;
enum Fault { NONE, READ, WRITE, RENAME_BEFORE, RENAME_AFTER, CLOSE };
static Fault fault = NONE;
static unsigned renames, provider_calls, io_calls, close_skip;
static bool retained_stage_close, stale_before_read, stale_before_replace;
static uint32_t ticks;
static constexpr unsigned rf_namespace = 3;
static constexpr unsigned other_namespace = 2; // Spectrum's published namespace.
static constexpr const char *checkpoint_name = "rf-neural.rnn";

static bool hit(Fault expected) {
    if (fault != expected) return false;
    fault = NONE;
    errno = expected == WRITE ? ENOSPC : EIO;
    return true;
}
static bool fail_read() {
    ++io_calls;
    return hit(READ);
}
extern "C" ssize_t __real_read(int, void *, size_t);
extern "C" ssize_t __wrap_read(int fd, void *bytes, size_t size) {
    return fail_read() ? -1 : __real_read(fd, bytes, size);
}
/* Ubuntu GCC11/glibc2.35 with ASan can lower even an unknown-size buffer's
 * read to __read_chk. Intercept both symbols so fortification cannot bypass
 * the same one-shot fault; keep libc's bounds check on real reads. */
extern "C" ssize_t __real___read_chk(int, void *, size_t, size_t);
extern "C" ssize_t __wrap___read_chk(int fd, void *bytes, size_t size, size_t capacity) {
    if (size > capacity) return __real___read_chk(fd, bytes, size, capacity);
    return fail_read() ? -1 : __real___read_chk(fd, bytes, size, capacity);
}
extern "C" ssize_t __real_write(int, const void *, size_t);
extern "C" ssize_t __wrap_write(int fd, const void *bytes, size_t size) {
    ++io_calls;
    return hit(WRITE) ? -1 : __real_write(fd, bytes, size);
}
extern "C" int __real_rename(const char *, const char *);
extern "C" int __wrap_rename(const char *from, const char *to) {
    ++io_calls;
    ++renames;
    if (hit(RENAME_BEFORE)) return -1;
    int result = __real_rename(from, to);
    return hit(RENAME_AFTER) ? -1 : result;
}
extern "C" int __real_close(int);
extern "C" int __wrap_close(int fd) {
    ++io_calls;
    bool fail = false;
    if (fault == CLOSE) {
        if (close_skip) --close_skip;
        else {
            char proc[64], path[256];
            snprintf(proc, sizeof(proc), "/proc/self/fd/%d", fd);
            ssize_t count = readlink(proc, path, sizeof(path) - 1);
            assert(count > 0);
            path[count] = 0;
            retained_stage_close = std::string(path).find("/n00000003/.pending") != std::string::npos;
            fail = true;
        }
    }
    int result = __real_close(fd);
    return fail && hit(CLOSE) ? -1 : result;
}
static uint32_t now(void *) { return ticks; }
static bool cooperate(void *) { ++ticks; return true; }

static int32_t put(AppDataFiles &store, unsigned space, const char *name,
                   const void *bytes, uint32_t size) {
    uint32_t old_size = 0;
    uint64_t revision = 0;
    int32_t result = store.stat(space, name, &old_size, &revision);
    if (result && result != RISC_APP_DATA_NOT_FOUND) return result;
    return store.replace(space, name, revision, bytes, size);
}
static void retire_token(AppDataFiles &store) {
    /* A real successful write in a different namespace retires global CAS. */
    assert(put(store, other_namespace, "cas-race", "x", 1) == RISC_APP_DATA_OK);
}
static risc_app_data_v1 bind(AppDataFiles &store) {
    return {1, sizeof(risc_app_data_v1), &store,
        [](void *context, const char *name, uint32_t *size, uint64_t *revision) {
            ++provider_calls;
            return static_cast<AppDataFiles *>(context)->stat(rf_namespace, name, size, revision);
        },
        [](void *context, const char *name, uint64_t revision, void *bytes,
           uint32_t capacity, uint32_t *size, uint64_t *actual) {
            ++provider_calls;
            auto &backend = *static_cast<AppDataFiles *>(context);
            if (stale_before_read) { stale_before_read = false; retire_token(backend); }
            return backend.read(rf_namespace, name, revision, bytes, capacity, size, actual);
        },
        [](void *context, const char *name, uint64_t revision, const void *bytes, uint32_t size) {
            ++provider_calls;
            auto &backend = *static_cast<AppDataFiles *>(context);
            if (stale_before_replace) { stale_before_replace = false; retire_token(backend); }
            return backend.replace(rf_namespace, name, revision, bytes, size);
        }};
}

static rt_files client, reloaded;
static rt_library library, loaded, before;
static rn_trainer checkpoint_trainer, checkpoint_loaded, checkpoint_before;
static uint8_t bytes[RT_BANK_MAX], expected[RT_BANK_MAX], checkpoint[RN_RECORD_SIZE];

static rt_example example() {
    rt_segmenter segment = {};
    rt_frame frame = {};
    frame.level_db = -2000;
    frame.peak_coord = rt_bin_coord(81);
    frame.flags = RT_ACTIVE | RT_TONAL | RT_GAP_BEFORE;
    rt_set_nibble(&frame, 40, 15);
    for (unsigned i = 0; i < RT_FRAMES; ++i) {
        /* Preserve real sparse observation gaps, including a 60-second gap. */
        frame.timestamp_ms = 1000 + i * 100 + (i > 31 ? 59900 : 0);
        rt_segment_observe(&segment, &frame);
    }
    assert(segment.ready && segment.event.count == RT_FRAMES);
    segment.event.flags |= RT_CONFIRMED_END;
    assert(rt_example_valid(&segment.event));
    return segment.event;
}
static void fill_bank(unsigned bank) {
    assert(rt_files_begin(&client, &library, bank) == RISC_APP_DATA_OK);
    rt_example full = example();
    for (unsigned i = bank * 4; i < bank * 4 + 4; ++i) {
        rt_label &label = library.labels[i];
        label.present = true;
        label.shift_limit = 2;
        label.next_id = RT_EXAMPLES + 1;
        snprintf(label.name, sizeof(label.name), "RF %u", i);
        for (unsigned j = 0; j < RT_EXAMPLES; ++j) {
            label.examples[j] = full;
            label.examples[j].id = j + 1;
            label.examples[j].kind = j < 3 ? RT_POSITIVE : RT_NEGATIVE;
        }
    }
    assert(rt_files_freeze(&client, &library) && client.wanted_size == RT_BANK_MAX);
}
static void rename_label(const char *name) {
    assert(rt_files_begin(&client, &library, 0) == RISC_APP_DATA_OK);
    strcpy(library.labels[0].name, name);
    assert(rt_files_freeze(&client, &library));
}
static void load_both(rt_files &files, rt_library &value) {
    assert(rt_files_load(&files, &value, 0, false) == RISC_APP_DATA_OK);
    assert(rt_files_load(&files, &value, 1, false) == RISC_APP_DATA_OK);
}
static void assert_saved(rt_files &files, rt_library &value) {
    load_both(files, value);
    /* Canonical records compare every persisted field without depending on
     * unspecified C++ padding in decoded observation structures. */
    for (unsigned bank = 0; bank < 2; ++bank) {
        size_t wanted = rt_bank_encode(&library, bank, expected, sizeof(expected));
        size_t actual = rt_bank_encode(&value, bank, bytes, sizeof(bytes));
        assert(wanted == RT_BANK_MAX && actual == wanted && !memcmp(expected, bytes, wanted));
    }
    for (unsigned i = 0; i < RT_LABELS; ++i) {
        assert(rt_label_valid(&value.labels[i]));
        assert(rt_example_count(&value.labels[i], 0) == RT_EXAMPLES);
    }
}
static uint32_t bank_crc(unsigned bank) {
    size_t size = rt_bank_encode(&library, bank, bytes, sizeof(bytes));
    assert(size == RT_BANK_MAX);
    return rf_signature_u32(bytes + size - 4);
}
static void test_checkpoint_capacity(AppDataFiles &store) {
    uint32_t crc[2] = {bank_crc(0), bank_crc(1)};
    rn_reset(&checkpoint_trainer, &library, 255);
    assert(checkpoint_trainer.eligible == 255);
    /* Serializer fixture for a previously promoted model. Promotion quality is
     * tested by rf_temporal_model_test; this checks actual committed capacity. */
    checkpoint_trainer.has_active = true;
    checkpoint_trainer.state = RN_ACTIVE;
    checkpoint_trainer.calibrated = checkpoint_trainer.eligible;
    checkpoint_trainer.checks = 2 * RT_LABELS;
    checkpoint_trainer.baseline_correct = checkpoint_trainer.checks - 1;
    checkpoint_trainer.candidate_correct = checkpoint_trainer.checks;
    checkpoint_trainer.full_checks = RT_LABELS * RT_EXAMPLES;
    checkpoint_trainer.full_baseline_correct = checkpoint_trainer.full_checks - 1;
    checkpoint_trainer.full_candidate_correct = checkpoint_trainer.full_checks;
    assert(rn_record_encode(&checkpoint_trainer, &library, crc, checkpoint, sizeof(checkpoint)));
    assert(put(store, rf_namespace, checkpoint_name, checkpoint, sizeof(checkpoint)) == RISC_APP_DATA_OK);
    uint32_t size = 0;
    uint64_t revision = 0, actual = 0;
    assert(store.stat(rf_namespace, checkpoint_name, &size, &revision) == RISC_APP_DATA_OK);
    assert(size == RN_RECORD_SIZE);
    assert(store.read(rf_namespace, checkpoint_name, revision, bytes, sizeof(bytes), &size, &actual) == RISC_APP_DATA_OK);
    assert(actual == revision && size == sizeof(checkpoint) && !memcmp(bytes, checkpoint, size));
    rn_reset(&checkpoint_loaded, &library, 255);
    assert(rn_record_load(&checkpoint_loaded, &library, crc, bytes, size));
    assert(!memcmp(&checkpoint_loaded.active, &checkpoint_trainer.active, sizeof(rn_model)));
    checkpoint_before = checkpoint_loaded;
    bytes[size - 1] ^= 1;
    assert(!rn_record_load(&checkpoint_loaded, &library, crc, bytes, size));
    assert(!memcmp(&checkpoint_loaded, &checkpoint_before, sizeof(checkpoint_loaded)));
    assert(2u * RT_BANK_MAX + RN_RECORD_SIZE == 129004u);
    assert(2u * RT_BANK_MAX + RN_RECORD_SIZE <= RISC_APP_DATA_NAMESPACE_MAX);
}
static void assert_pending(const std::vector<uint8_t> &wanted) {
    assert(client.pending == 0 && client.wanted_size == wanted.size());
    assert(!memcmp(client.wanted, wanted.data(), wanted.size()));
}
static void test_unknown_append(Fault injected, uint32_t id) {
    assert(rt_files_begin(&client, &library, 0) == RISC_APP_DATA_OK);
    memset(&library.labels[0].examples[5], 0, sizeof(rt_example));
    assert(rt_files_freeze(&client, &library));
    assert(rt_files_save(&client) == RISC_APP_DATA_OK);
    assert(rt_files_begin(&client, &library, 0) == RISC_APP_DATA_OK);
    library.labels[0].examples[5] = example();
    library.labels[0].examples[5].id = id;
    library.labels[0].examples[5].kind = RT_NEGATIVE;
    library.labels[0].next_id = id + 1;
    assert(rt_files_freeze(&client, &library));
    std::vector<uint8_t> wanted(client.wanted, client.wanted + client.wanted_size);
    uint32_t generation = library.generation[0];
    fault = injected;
    assert(rt_files_save(&client) == RISC_APP_DATA_COMMIT_UNKNOWN && fault == NONE);
    assert_pending(wanted);
    assert(rt_files_load(&reloaded, &loaded, 0, false) == RISC_APP_DATA_OK);
    assert(rt_example_count(&loaded.labels[0], 0) == (injected == RENAME_BEFORE ? 5u : 6u));
    unsigned prior = renames;
    assert(rt_files_save(&client) == RISC_APP_DATA_OK);
    assert(renames == prior + (injected == RENAME_BEFORE ? 1u : 0u));
    assert_saved(reloaded, loaded);
    assert(loaded.generation[0] == generation && loaded.labels[0].next_id == id + 1);
    assert(loaded.labels[0].examples[5].id == id);
    prior = provider_calls;
    assert(rt_files_save(&client) == RISC_APP_DATA_OK && provider_calls == prior);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    AppDataFiles store({nullptr, now, cooperate, malloc, free});
    auto api = bind(store);
    library.identity = rf_identity_default();
    rt_files_init(&client, &api);
    assert(rt_files_begin(&client, &library, 0) == RISC_APP_DATA_UNAVAILABLE);
    assert(rt_files_load(&client, &library, 0, false) == RISC_APP_DATA_UNAVAILABLE && !client.ready);
    assert(store.configure(argv[1]));
    load_both(client, library);
    assert(client.ready == 3 && !client.exists && client.pending < 0);
    fill_bank(0);
    assert(rt_files_save(&client) == RISC_APP_DATA_OK);
    fill_bank(1);
    assert(rt_files_save(&client) == RISC_APP_DATA_OK);
    rt_files_init(&reloaded, &api);
    assert_saved(reloaded, loaded);
    test_checkpoint_capacity(store);

    uint32_t size = 0;
    uint64_t revision = 0, actual = 0;
    assert(store.stat(rf_namespace, rt_file_name(0), &size, &revision) == RISC_APP_DATA_OK && size == RT_BANK_MAX);
    rename_label("Other app CAS");
    assert(put(store, other_namespace, rt_file_name(0), "timecard", 8) == RISC_APP_DATA_OK);
    assert(store.read(rf_namespace, rt_file_name(0), revision, bytes, sizeof(bytes), &size, &actual) == RISC_APP_DATA_STALE);
    assert(rt_files_save(&client) == RISC_APP_DATA_OK);
    assert_saved(reloaded, loaded);
    assert(store.stat(other_namespace, rt_file_name(0), &size, &revision) == RISC_APP_DATA_OK);
    assert(store.read(other_namespace, rt_file_name(0), revision, bytes, sizeof(bytes), &size, &actual) == RISC_APP_DATA_OK);
    assert(size == 8 && !memcmp(bytes, "timecard", 8));
    uint32_t crc[2] = {bank_crc(0), bank_crc(1)};
    rn_reset(&checkpoint_loaded, &library, 255);
    checkpoint_before = checkpoint_loaded;
    assert(!rn_record_load(&checkpoint_loaded, &library, crc, checkpoint, sizeof(checkpoint)));
    assert(!memcmp(&checkpoint_loaded, &checkpoint_before, sizeof(checkpoint_loaded)));

    /* A fourth file consumes the exact remaining namespace quota. Deletion
     * reclaims payload bytes, and a failed restoration keeps the complete edit. */
    rt_example saved = library.labels[0].examples[0];
    assert(rt_files_begin(&client, &library, 0) == RISC_APP_DATA_OK);
    memset(&library.labels[0].examples[0], 0, sizeof(saved));
    assert(rt_files_freeze(&client, &library));
    assert(rt_files_save(&client) == RISC_APP_DATA_OK);
    const uint32_t slack = RISC_APP_DATA_NAMESPACE_MAX - (2 * RT_BANK_MAX + RN_RECORD_SIZE - RT_FRAMES * RT_FRAME_BYTES);
    std::vector<uint8_t> filler(slack, 1);
    assert(put(store, rf_namespace, "quota-test", filler.data(), filler.size()) == RISC_APP_DATA_OK);
    assert(rt_files_begin(&client, &library, 0) == RISC_APP_DATA_OK);
    library.labels[0].examples[0] = saved;
    assert(rt_files_freeze(&client, &library));
    std::vector<uint8_t> wanted(client.wanted, client.wanted + client.wanted_size);
    assert(rt_files_save(&client) == RISC_APP_DATA_NO_SPACE);
    assert_pending(wanted);
    assert(rt_files_load(&reloaded, &loaded, 0, false) == RISC_APP_DATA_OK && !loaded.labels[0].examples[0].id);
    assert(put(store, rf_namespace, "quota-test", nullptr, 0) == RISC_APP_DATA_OK);
    assert(rt_files_save(&client) == RISC_APP_DATA_OK);
    assert_saved(reloaded, loaded);

    rename_label("Write full");
    wanted.assign(client.wanted, client.wanted + client.wanted_size);
    fault = WRITE;
    assert(rt_files_save(&client) == RISC_APP_DATA_NO_SPACE && fault == NONE);
    assert_pending(wanted);
    assert(rt_files_load(&reloaded, &loaded, 0, false) == RISC_APP_DATA_OK);
    assert(!strcmp(loaded.labels[0].name, "Other app CAS"));
    assert(rt_files_save(&client) == RISC_APP_DATA_OK);
    assert_saved(reloaded, loaded);

    before = loaded;
    stale_before_read = true;
    assert(rt_files_load(&reloaded, &loaded, 0, false) == RISC_APP_DATA_STALE);
    assert(!memcmp(&before, &loaded, sizeof(loaded)) && !(reloaded.ready & 1));
    assert(rt_files_begin(&reloaded, &loaded, 0) == RISC_APP_DATA_STALE);
    assert(rt_files_load(&reloaded, &loaded, 0, false) == RISC_APP_DATA_OK);
    rename_label("Stale replace");
    wanted.assign(client.wanted, client.wanted + client.wanted_size);
    stale_before_replace = true;
    assert(rt_files_save(&client) == RISC_APP_DATA_STALE);
    assert_pending(wanted);
    assert(rt_files_save(&client) == RISC_APP_DATA_OK);
    assert_saved(reloaded, loaded);

    test_unknown_append(RENAME_BEFORE, 7);
    test_unknown_append(RENAME_AFTER, 8);

    /* A remount rereads complete records instead of trusting old CAS tokens. */
    AppDataFiles restarted({nullptr, now, cooperate, malloc, free});
    assert(restarted.configure(argv[1]));
    auto restarted_api = bind(restarted);
    rt_files_init(&reloaded, &restarted_api);
    assert_saved(reloaded, loaded);
    before = loaded;
    fault = READ;
    assert(rt_files_load(&reloaded, &loaded, 0, false) == RISC_APP_DATA_IO && fault == NONE);
    assert(!memcmp(&before, &loaded, sizeof(loaded)) && !(reloaded.ready & 1));
    assert(rt_files_begin(&reloaded, &loaded, 0) == RISC_APP_DATA_IO);
    assert(rt_files_load(&reloaded, &loaded, 0, false) == RISC_APP_DATA_OK);

    before = loaded;
    assert(rt_files_begin(&reloaded, &loaded, 0) == RISC_APP_DATA_OK);
    strcpy(loaded.labels[0].name, "Pending edit");
    assert(rt_files_freeze(&reloaded, &loaded));
    strcpy(before.labels[0].name, "External edit");
    ++before.generation[0];
    size = (uint32_t)rt_bank_encode(&before, 0, bytes, sizeof(bytes));
    assert(size == RT_BANK_MAX);
    assert(put(restarted, rf_namespace, rt_file_name(0), bytes, size) == RISC_APP_DATA_OK);
    unsigned prior = renames;
    assert(rt_files_save(&reloaded) == RT_FILES_CONFLICT && renames == prior);
    assert(reloaded.pending == 0 && !strcmp(loaded.labels[0].name, "Pending edit"));
    assert(rt_files_load(&reloaded, &loaded, 0, false) == RT_FILES_BUSY);
    assert(rt_files_load(&reloaded, &loaded, 0, true) == RISC_APP_DATA_OK);
    assert(reloaded.pending < 0 && !reloaded.wanted_size && !strcmp(loaded.labels[0].name, "External edit"));
    library = loaded;

    size = (uint32_t)rt_bank_encode(&library, 0, bytes, sizeof(bytes));
    assert(size == RT_BANK_MAX);
    bytes[5] ^= 1;
    assert(put(restarted, rf_namespace, rt_file_name(0), bytes, size) == RISC_APP_DATA_OK);
    before = loaded;
    assert(rt_files_load(&reloaded, &loaded, 0, false) == RT_FILES_CORRUPT);
    assert(!memcmp(&before, &loaded, sizeof(loaded)) && !(reloaded.ready & 1));
    assert(rt_files_begin(&reloaded, &loaded, 0) == RT_FILES_CORRUPT);
    bytes[5] ^= 1;
    assert(put(restarted, rf_namespace, rt_file_name(0), bytes, size) == RISC_APP_DATA_OK);
    assert(rt_files_load(&reloaded, &loaded, 0, false) == RISC_APP_DATA_OK);

    /* A physically successful close reported as failed leaves ambiguous
     * descriptor ownership. Both layers latch retention and stop all I/O. */
    assert(rt_files_begin(&reloaded, &loaded, 0) == RISC_APP_DATA_OK);
    strcpy(loaded.labels[0].name, "Retained");
    assert(rt_files_freeze(&reloaded, &loaded));
    fault = CLOSE;
    close_skip = 1; // Skip the committed-bank read; fail the staged-file close.
    assert(rt_files_save(&reloaded) == RISC_APP_DATA_RETAINED && fault == NONE);
    assert(reloaded.retained && restarted.retained() && retained_stage_close && !restarted.exitSafe());
    prior = provider_calls;
    unsigned prior_io = io_calls;
    assert(rt_files_save(&reloaded) == RISC_APP_DATA_RETAINED);
    assert(rt_files_load(&reloaded, &loaded, 0, true) == RISC_APP_DATA_RETAINED);
    assert(provider_calls == prior && io_calls == prior_io);
    assert(restarted.stat(rf_namespace, rt_file_name(0), &size, &revision) == RISC_APP_DATA_RETAINED);
    assert(restarted.read(rf_namespace, rt_file_name(0), revision, bytes, sizeof(bytes), &size, &actual) == RISC_APP_DATA_RETAINED);
    assert(restarted.replace(rf_namespace, rt_file_name(0), 0, bytes, 0) == RISC_APP_DATA_RETAINED);
    assert(io_calls == prior_io && !restarted.configure(argv[1]));

    puts("RF / real Runtime AppDataFiles: absent/unread/corrupt reservations, two maximum banks plus neural checkpoint, namespace isolation/global CAS, exact quota and ENOSPC, stale read/replace retry, rename-before/after identical append reconciliation without duplicates, remount, conflict/discard, retained stage-close/no-further-I/O PASS");
}
