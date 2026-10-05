/* Regression contract for the current Points/Watch Wi-Fi 8x4 keyboard.
 * Run the production Spectrum controller with the established provider fixture.
 * The expected ASCII pages and pixel geometry are independent test literals. */
#define main original_controller_main
#include "audio_spectrum_test.c"
#undef main
#include <limits.h>

static const char expected_pages[3][33] = {
    " !\"#$%&'()*+,-./0123456789:;<=>?",
    "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_",
    "`abcdefghijklmnopqrstuvwxyz{|}~"
};
static const char full_label[] = "Az 09!?_[]{}|~ab";
static spectrum_label unchanged_labels[SPECTRUM_LABEL_MAX];
static unsigned char unchanged_cells[sizeof(cells)];
static unsigned unchanged_puts;

static void snapshot_saved(void) {
    memcpy(unchanged_labels, labels, sizeof(labels));
    memcpy(unchanged_cells, cells, sizeof(cells));
    unchanged_puts = puts_count;
}
static void assert_saved_unchanged(void) {
    assert(!memcmp(unchanged_labels, labels, sizeof(labels)));
    assert(!memcmp(unchanged_cells, cells, sizeof(cells)));
    assert(puts_count == unchanged_puts && !pending_save);
}
static void key_tap(int x, int y) {
    bool toggle_requested = false, freeze_requested = false;
    assert(!tap_action(x, y, &toggle_requested, &freeze_requested));
    assert(!toggle_requested && !freeze_requested);
}
static void tap_key(unsigned key) {
    assert(key < PWK_COUNT);
    if (key < PWK_CHARACTERS)
        key_tap(12 + (int)(key % 8) * 27 + 13, 76 + (int)(key / 8) * 24 + 11);
    else
        key_tap(12 + (int)(key - PWK_PAGE) * 72 + 35, 190);
}
static void type_char(unsigned ch) {
    assert(ch >= 32 && ch <= 126 && page == PAGE_KEYBOARD);
    unsigned target_page = (ch - 32) / 32;
    for (unsigned tries = 0; key_page != target_page; ++tries) {
        assert(tries < 3);
        tap_key(PWK_PAGE);
    }
    tap_key((ch - 32) % 32);
}
static void open_empty_keyboard(void) {
    open_label(-1, 777);
    assert(page == PAGE_LABEL && editing.present && !editing.name[0]);
    key_tap(120, 120);
    assert(page == PAGE_KEYBOARD && key_page == 2 && key_choice == 0 && !back_exits);
}
static int expected_hit(int x, int y) {
    /* Enumerate the original hit rectangles instead of repeating the helper's
     * division expression. The 2px visual gutters intentionally remain active. */
    for (unsigned key = 0; key < 32; ++key) {
        int left = 12 + (int)(key % 8) * 27;
        int top = 76 + (int)(key / 8) * 24;
        if (x >= left && x < left + 27 && y >= top && y < top + 24) return (int)key;
    }
    for (unsigned key = 32; key < 35; ++key) {
        int left = 12 + (int)(key - 32) * 72;
        if (x >= left && x < left + 72 && y >= 178 && y < 207) return (int)key;
    }
    return -1;
}
static void shared_contract(void) {
    assert(PWK_CHARACTERS == 32 && PWK_PAGES == 3 && PWK_COUNT == 35 && PWK_INITIAL_PAGE == 2);
    assert(PWK_PAGE == 32 && PWK_DELETE == 33 && PWK_DONE == 34);
    bool seen[127] = {false};
    unsigned count_printable = 0;
    for (unsigned p = 0; p < 3; ++p) {
        for (unsigned key = 0; key < 32; ++key) {
            unsigned ch = portable_watch_key_character(p, key);
            assert(ch == (unsigned char)expected_pages[p][key]);
            if (ch) {
                assert(ch >= 32 && ch <= 126 && !seen[ch]);
                seen[ch] = true;
                ++count_printable;
            }
        }
    }
    assert(count_printable == 95);
    for (unsigned ch = 32; ch <= 126; ++ch) assert(seen[ch]);
    assert(!portable_watch_key_character(2, 31)); /* ASCII DEL is blank/inert. */
    assert(!portable_watch_key_character(3, 0));
    assert(!portable_watch_key_character(0, 32));
    assert(!portable_watch_key_character(UINT_MAX, UINT_MAX));
    for (unsigned key = 0; key < 35; ++key) {
        portable_watch_key_rect r = {0};
        assert(portable_watch_key_bounds(key, &r));
        if (key < 32) {
            assert(r.x == 12 + (int)(key % 8) * 27 && r.y == 76 + (int)(key / 8) * 24);
            assert(r.w == 25 && r.h == 22);
        } else {
            assert(r.x == 12 + (int)(key - 32) * 72 && r.y == 178);
            assert(r.w == (key == 34 ? 72 : 68) && r.h == 29);
        }
    }
    portable_watch_key_rect untouched = {1, 2, 3, 4};
    assert(!portable_watch_key_bounds(35, &untouched));
    assert(!portable_watch_key_bounds(UINT_MAX, &untouched));
    assert(untouched.x == 1 && untouched.y == 2 && untouched.w == 3 && untouched.h == 4);
    assert(!portable_watch_key_bounds(0, NULL));
    for (int y = -1; y <= 240; ++y)
        for (int x = -1; x <= 240; ++x)
            assert(portable_watch_key_hit(x, y) == expected_hit(x, y));
    assert(portable_watch_key_hit(INT_MIN, INT_MAX) == -1);
    assert(portable_watch_key_hit(INT_MAX, INT_MIN) == -1);
}
static void printable_touches(void) {
    snapshot_saved();
    for (unsigned p = 0; p < 3; ++p) {
        for (unsigned key = 0; key < 32; ++key) {
            open_empty_keyboard();
            while (key_page != p) tap_key(PWK_PAGE);
            tap_key(key);
            char expected[2] = {expected_pages[p][key], 0};
            assert(!strcmp(editing.name, expected) && key_choice == key);
            assert_saved_unchanged();
            tap_key(PWK_DONE);
            assert(page == PAGE_LABEL && !strcmp(editing.name, expected));
            assert_saved_unchanged();
            key_tap(50, 185); /* Discard label after DONE: still no Save. */
            assert(page == PAGE_MAIN);
            assert_saved_unchanged();
        }
    }
}
static void touch_cell_edges(void) {
    snapshot_saved();
    for (unsigned key = 0; key < 32; ++key) {
        for (unsigned corner = 0; corner < 4; ++corner) {
            open_empty_keyboard();
            int x = 12 + (int)(key % 8) * 27 + ((corner & 1) ? 26 : 0);
            int y = 76 + (int)(key / 8) * 24 + ((corner & 2) ? 23 : 0);
            key_tap(x, y);
            char expected[2] = {expected_pages[2][key], 0};
            assert(!strcmp(editing.name, expected) && key_choice == key);
        }
    }
    open_empty_keyboard();
    static const int misses[][2] = {
        {-1, 100}, {240, 100}, {20, -1}, {20, 240}, {11, 76}, {228, 76},
        {12, 75}, {12, 172}, {12, 177}, {12, 207}, {11, 178}, {228, 178},
        {120, 40}, {120, 60}, {120, 230}
    };
    for (unsigned i = 0; i < sizeof(misses) / sizeof(misses[0]); ++i) {
        key_tap(misses[i][0], misses[i][1]);
        assert(page == PAGE_KEYBOARD && key_page == 2 && key_choice == 0 && !editing.name[0]);
    }
    keyboard_activate(PWK_COUNT);
    keyboard_activate(UINT_MAX);
    assert(page == PAGE_KEYBOARD && !editing.name[0]);
    key_tap(12, 178); assert(key_page == 0 && key_choice == PWK_PAGE);
    key_tap(83, 206); assert(key_page == 1 && key_choice == PWK_PAGE);
    key_tap(83, 206); assert(key_page == 2);
    type_char('a'); type_char('b');
    key_tap(84, 178); assert(!strcmp(editing.name, "a") && key_choice == PWK_DELETE);
    key_tap(155, 206); assert(!editing.name[0]);
    key_tap(155, 206); assert(!editing.name[0]);
    key_tap(156, 178); assert(page == PAGE_LABEL);
    key_tap(120, 120); key_tap(227, 206); assert(page == PAGE_LABEL);
    assert_saved_unchanged();
    key_tap(50, 185);
}
static void cancel_and_save(void) {
    assert(strlen(full_label) == SPECTRUM_LABEL_NAME_MAX);
    snapshot_saved();
    open_label(0, 0);
    spectrum_label initial = editing;
    key_tap(120, 120);
    type_char('!');
    assert(strcmp(editing.name, initial.name));
    key_tap(20, 20); /* Standard header Back means cancel this keyboard edit. */
    assert(page == PAGE_LABEL && !strcmp(editing.name, initial.name));
    assert(editing.frequency_hz == initial.frequency_hz && editing.color == initial.color);
    assert_saved_unchanged();
    key_tap(120, 120);
    assert(key_page == 2 && key_choice == 0);
    for (unsigned i = 0; i < 30; ++i) tap_key(PWK_DELETE);
    assert(!editing.name[0]);
    for (const char *s = full_label; *s; ++s) type_char((unsigned char)*s);
    assert(!strcmp(editing.name, full_label) && editing.name[16] == 0);
    for (unsigned p = 0; p < 3; ++p) {
        tap_key(PWK_PAGE);
        tap_key(1); /* Full capacity: extra printable input leaves all 16 bytes. */
        assert(!strcmp(editing.name, full_label) && editing.name[16] == 0);
    }
    assert_saved_unchanged();
    tap_key(PWK_DELETE);
    assert(strlen(editing.name) == 15 && editing.name[15] == 0);
    type_char('b');
    assert(!strcmp(editing.name, full_label));
    type_char('x'); /* Capacity overflow must not swallow DONE's bottom edge. */
    key_tap(227, 206);
    assert(page == PAGE_LABEL && !strcmp(editing.name, full_label));
    assert_saved_unchanged();
    key_tap(120, 120);
    type_char('x');
    tap_key(PWK_DELETE);
    key_tap(20, 20);
    assert(page == PAGE_LABEL && !strcmp(editing.name, full_label));
    assert_saved_unchanged();
    key_tap(180, 185); /* Only the label dialog's Save may persist. */
    assert(page == PAGE_MAIN && puts_count == unchanged_puts + 1 && !pending_save);
    assert(!strcmp(labels[0].name, full_label));
    assert(labels[0].frequency_hz == initial.frequency_hz && labels[0].color == initial.color);
    for (unsigned i = 1; i < SPECTRUM_LABEL_MAX; ++i)
        assert(!memcmp(&labels[i], &unchanged_labels[i], sizeof(labels[i])));
    bool found = false;
    for (unsigned i = 0; i < 12; ++i) if (!strcmp(cells[i].key, "spectrum_l0")) {
        spectrum_label decoded = {0};
        assert(spectrum_label_decode(&decoded, cells[i].bytes, cells[i].size));
        assert(!strcmp(decoded.name, full_label));
        assert(decoded.frequency_hz == initial.frequency_hz && decoded.color == initial.color);
        found = true;
    }
    assert(found);
}

static unsigned expected_choice;
static void hardware_setup(void) { snapshot_saved(); open_empty_keyboard(); expected_choice = 0; }
static void check_next_choice(void) {
    expected_choice = (expected_choice + 1) % 35;
    assert(page == PAGE_KEYBOARD && key_choice == expected_choice && key_page == 2 && !editing.name[0]);
    assert_saved_unchanged();
}
static void check_previous_choice(void) {
    expected_choice = expected_choice ? expected_choice - 1 : 34;
    assert(page == PAGE_KEYBOARD && key_choice == expected_choice && key_page == 2 && !editing.name[0]);
    assert_saved_unchanged();
}
static void check_choice_done(void) { assert(page == PAGE_KEYBOARD && key_choice == PWK_DONE && !editing.name[0]); }
static void check_choice_zero(void) { assert(page == PAGE_KEYBOARD && key_choice == 0 && !editing.name[0]); }
static void check_choice_a(void) { assert(page == PAGE_KEYBOARD && key_choice == 1 && !editing.name[0]); }
static void check_typed_a(void) { assert(page == PAGE_KEYBOARD && key_choice == 1 && !strcmp(editing.name, "a")); }
static void check_priority_left(void) { assert(page == PAGE_KEYBOARD && key_choice == 0 && !strcmp(editing.name, "a")); }
static void check_typed_azz(void) { assert(page == PAGE_KEYBOARD && key_choice == 26 && !strcmp(editing.name, "azz")); }
static void check_hardware_page(void) { assert(page == PAGE_KEYBOARD && key_choice == PWK_PAGE && key_page == 1 && !strcmp(editing.name, "azz")); }
static void check_hardware_delete(void) { assert(page == PAGE_KEYBOARD && key_choice == PWK_DELETE && !strcmp(editing.name, "az")); }
static void check_hardware_done(void) { assert(page == PAGE_LABEL && !strcmp(editing.name, "az")); assert_saved_unchanged(); }
static void check_hardware_cancel(void) { assert(page == PAGE_LABEL && !strcmp(editing.name, "az")); assert_saved_unchanged(); }
static void check_label_cancel(void) { assert(page == PAGE_MAIN && back_exits); assert_saved_unchanged(); }
static void restored_maximum_begin(void) {
    assert(labels[0].present && !strcmp(labels[0].name, full_label));
    assert(labels[0].frequency_hz == 3210 && labels[0].color == 7);
    snapshot_saved();
    open_label(0, 0);
    key_tap(120, 120);
    assert(page == PAGE_KEYBOARD && key_page == 2 && key_choice == 0);
    assert(!strcmp(editing.name, full_label) && editing.name[16] == 0);
    tap_key(PWK_DELETE);
    assert(strlen(editing.name) == 15);
    assert_saved_unchanged();
}
static void restored_maximum_cancelled(void) {
    assert(page == PAGE_LABEL && !strcmp(editing.name, full_label) && editing.name[16] == 0);
    assert(editing.frequency_hz == 3210 && editing.color == 7);
    assert_saved_unchanged();
}

int main(void) {
    shared_contract();
    reset(); check(printable_touches); check(touch_cell_edges); check(cancel_and_save); back(); run(); clean(0, 0, 0);
    reset(); check(hardware_setup);
    for (unsigned i = 0; i < 35; ++i) { add(EVENT_INPUT, T5_APP_BUTTON_RIGHT, 0); check(check_next_choice); }
    for (unsigned i = 0; i < 35; ++i) { add(EVENT_INPUT, T5_APP_BUTTON_LEFT, 0); check(check_previous_choice); }
    add(EVENT_INPUT, T5_APP_BUTTON_UP, 0); check(check_choice_done);
    add(EVENT_INPUT, T5_APP_BUTTON_DOWN, 0); check(check_choice_zero);
    add(EVENT_INPUT, T5_APP_BUTTON_RIGHT | T5_APP_BUTTON_CONFIRM, 0); check(check_choice_a);
    add(EVENT_INPUT, T5_APP_BUTTON_CONFIRM, 0); check(check_typed_a);
    add(EVENT_INPUT, T5_APP_BUTTON_LEFT | T5_APP_BUTTON_DOWN | T5_APP_BUTTON_CONFIRM, 0); check(check_priority_left);
    tap(79, 159, 0); /* Lowercase z, key26. */
    add(EVENT_INPUT, T5_APP_BUTTON_CONFIRM, 0); check(check_typed_azz);
    tap(30, 190, 0); /* 2->0 page selection by touch, then 0->1 by hardware. */
    add(EVENT_INPUT, T5_APP_BUTTON_CONFIRM, 0); check(check_hardware_page);
    add(EVENT_INPUT, T5_APP_BUTTON_DOWN, 0); add(EVENT_INPUT, T5_APP_BUTTON_CONFIRM, 0); check(check_hardware_delete);
    add(EVENT_INPUT, T5_APP_BUTTON_DOWN, 0); add(EVENT_INPUT, T5_APP_BUTTON_CONFIRM, 0); check(check_hardware_done);
    tap(120, 120, 0); tap(52, 87, 0); back(); check(check_hardware_cancel);
    back(); check(check_label_cancel); back(); run(); clean(0, 0, 0);
    reset();
    spectrum_label saved = {true, 3210, 7, ""};
    memcpy(saved.name, full_label, sizeof(saved.name));
    uint8_t record[32];
    spectrum_label_encode(&saved, record);
    preload("spectrum_l0", record);
    check(restored_maximum_begin); back(); check(restored_maximum_cancelled);
    back(); back(); run(); clean(0, 0, 0);
    puts("Spectrum Watch keyboard: exact 8x4 geometry/hits, 95 ASCII characters, three pages/default lowercase, touch boundaries, hardware selection/activation, Delete/Done/Cancel, 16-character bound and no persistence before Save passed");
    return 0;
}
