# Private LoRa graph integration

This suite compiles unchanged LoRa production controller, actual Runtime/Graph,
scene_host, text_input and scene_profile as separately loaded modules. Hardware
input, display, radio and storage are explicit deterministic fixtures. Scene
instrumentation only observes state/function boundaries; inputs enter through
hardware navigation/HID APIs. Controller models and scene queues are not seeded.

Normal and --sanitize runs cover eager/demand activation, optional UI/text/radio/
storage absence, 240-byte compose and single explicit TX, 255-byte RX/history,
exact 32-byte profile persistence in namespace 9 (namespace 1 never written),
short Back, delayed display and TX completion, short radio API tables, changing
radio selector output, malformed stored profile, actual Runtime retention from
provider callbacks and failed cancellation, and no subsequent service I/O.
Malformed/changing radio provider responses are NOT malformed Runtime grants.
Runtime itself constructs valid generation-bound grants; corrupt grant delivery
cannot be claimed without a separately qualified Runtime fault-injection seam.

The configure regression observes production scene_configure/update_document/
scene_snapshot via compiler instrumentation. It injects hardware Home at entry
to update_document after snapshot has returned. It asserts one configure call,
one delivered input and at least one document update, and rejects any subsequent
configure. The event survives the update through the actual scene provider.

Production resident host attempt is separately selectable with --resident.
It is currently BLOCKED, not qualified. The pinned System adapter requires
RiscDisplayOutputSnapshotV1.h -> RiscDisplayOutputMetricsV1.h ->
RiscDisplayOutputPowerV1.h -> display history suffix. These are absent from the
mandated Runtime-first SDK. The examined X4 archived display header at commit
6b7d1abe9b2dcc236f6086d07c0f619244c907cb assigns bit 6 QUIESCE_SLEEP, whereas the
canonical staged header assigns bit 6 ROUND and omits history. Do not overwrite
the canonical header to make the host compile. Snapshot header copied unchanged
from that X4 pin has SHA256 b835fd1a3600551d54efb2806cbe8dd3ecc512a538572dc853aaf11f4e2490b6.

System eb4dd5306906d57e65f24f911d325bc87370666b provides idle-only and alarm-only
contract headers, but its production resident host does not implement those
reasons or register attention callbacks. Alarm/idle/controls settled-owner host
behavior and text-owner resident handoff remain unqualified until a compatible
production host+SDK source set is supplied. Existing callback-double root cases
are not a substitute. The synchronous radio cancel API has no AGAIN status;
this suite qualifies false as terminal retention, not fabricated async success.

No physical-device, deployed product, real RF, production persistence or product
binding/catalog claim is made. No production source was changed.
