# Calculator

Calculator 0.1.0 is a shared optional utility. Its source lives in
`RiscRTE-Utilities/Apps/calculator.c`, with arithmetic in `calculator_core.h` and
small scalable numeric/text drawing in `daily_draw.h`. Watch builds consume this
source through the shared PortableApps adapter; there is no Watch-owned copy.

## Controls and display

The 240 × 240 layout provides a direct four-column, five-row keypad:

- C clears the whole calculation, including a pending or repeated operation.
- +/− changes the entry's sign. After an operator, it starts a signed new entry.
- DEL removes the last entry character. It can edit a displayed result; it does
  nothing while an operator is waiting for its next operand. On an error it
  clears the calculator.
- Digits and the decimal point enter an operand. Leading zeroes are collapsed;
  a repeated decimal point or a seventh fractional digit is ignored.
- +, −, × and / perform the four arithmetic operations.
- The double-width = key evaluates the calculation. Subsequent presses repeat
  the last operator and operand: `2 + 3 = =` produces 8.
- Consecutive operators replace the pending operator. `12 + − 3 =` produces 9.
  Pressing = before entering the next operand reuses the displayed number:
  `2 + =` produces 4.
- Calculations execute left to right: `2 + 3 × 4 =` produces 20. There is no
  expression precedence, parenthesis, percentage or scientific mode.
- A digit or decimal point after a result starts a new calculation. An operator
  continues from that result.

Back at the top left, the existing Back navigation button, crownBack and the
runtime's exit request all leave the app. Directional navigation selects keys;
Confirm activates the selection. Button events use the existing input API's
semantics. No extra hold-to-repeat behavior is added.

Completed valid taps activate exactly one key. Key rectangles have four-pixel
inter-key gaps; out-of-screen touches, the display and gaps do not enter values.
On a runtime implementing the existing optional `touch_contact` member, contact
presses are highlighted, held contacts do not repeat, release removes the
highlight, and dragging outside the initial key cancels its highlight. The
current shared PortableApps adapter provides completed taps rather than contact
states, so it shows the new value and selected key on release, without a
press-state preview. Tap eligibility and swipe cancellation remain firmware
owned. No new runtime interface is required.

The display uses scalable 5 × 7 primitives instead of relying on missing
operator/decimal glyphs in the adapter's basic text renderer. Ordinary values
use 28-pixel-high digits at watch size; longer values shrink to fit and are not
silently truncated. The small `6 DP` label records decimal precision. These are
text glyphs, not replacement icons. The manifest uses the existing bundled
Font Awesome `solid:f00a` keypad/grid glyph.

## Arithmetic and errors

All values are signed 64-bit integer millionths. There is no binary floating
point, platform-dependent decimal formatting or 128-bit runtime dependency.

- Range: −999,999,999.999999 through +999,999,999.999999.
- Precision: six fractional decimal places.
- Addition and subtraction are exact within that range.
- Multiplication and division round to the nearest millionth, with ties away
  from zero. For example `2 / 3 =` gives `0.666667`, and `−0.000001 / 2 =`
  gives `−0.000001`.
- Input preserves its typed decimal point and trailing zeroes. Computed results
  omit unnecessary trailing zeroes.
- Division by zero displays `DIV BY ZERO`; exceeding the range, including a
  tenth integer entry digit, displays `OVERFLOW`.
- An error clears pending/repeat operations and does not display a fabricated
  numeric answer. C or DEL resets; a digit/decimal starts a fresh entry.
  Operators and sign changes leave an error visible.

Multiplication decomposes integer and fractional terms before checked
accumulation. Division uses six bounded long-division steps. Intermediate
unsigned products and remainders remain within 64-bit limits; bounds are checked
before conversion back to the signed result.

## Interfaces and lifetime

Only `t5_app_get_api(T5_APP_ABI_VERSION)` is imported by the app. It validates ABI,
structure size through `poll`, and required screen, clear, fill, present and poll
functions before drawing. Optional contact access is individually size-checked.
A polling interval of 30 ms yields regularly to the runtime. There is no busy
loop, filesystem/network access, sensor access, allocation, persistent state,
alarm or background task. Leaving the app discards the calculation.

Supported geometry is 200–4096 pixels wide and 240–4096 pixels high. Unsupported
or invalid geometry returns safely without drawing. Layout is derived from the
screen dimensions, with integer bounds kept within the touch-coordinate range.
The legacy-format manifest declares version 0.1.0 and minimum firmware 1.1.18;
portable-runtime packaging is handled by the shared build pipeline.

## Host verification

Run from the repository root:

```sh
cc -std=c11 -Wall -Wextra -Werror -Ilib/NativeApps/include \
  tests/calculator_core_failures.c -o /tmp/calculator-core
/tmp/calculator-core
cc -std=c11 -Wall -Wextra -Werror -Ilib/NativeApps/include \
  test/native_apps/calculator_test.c -o /tmp/calculator-ui
mkdir -p /tmp/calculator-frames
/tmp/calculator-ui /tmp/calculator-frames
```

The production arithmetic core is checked against 60,000 deterministic,
independent 128-bit host-oracle cases, plus explicit decimal, sign, deletion,
operator replacement, repeated equals, rounding, overflow, divide-by-zero and
recovery sequences. The oracle's 128-bit math is test-only.

The actual app source is exercised with a recording framebuffer and input API:
keypad taps, repeated digit/Confirm events, selection, optional contact press and
release/cancellation, crownBack, top-left Back, runtime exit and polling failure.
It also checks missing/malformed APIs, legacy structure sizes, negative and
out-of-bounds coordinates, keypad gaps, invalid dimensions and supported layouts
from 200 × 240 to 4096 × 4096. Every draw rectangle must be on screen. Optional
PGM frame output covers initial, decimal, maximum signed value and divide-by-zero
screens. Those frames were rendered and visually inspected at watch size.

Both fixtures also pass AddressSanitizer and UndefinedBehaviorSanitizer. This
sandbox requires `ASAN_OPTIONS=detect_leaks=0` because ptrace prevents
LeakSanitizer startup; production calculator code does not allocate memory.
These host checks establish source behavior and pixel bounds, not physical
watch usability, device latency, firmware/hardware integration or release
qualification. No deployment or flashing is part of these checks.
