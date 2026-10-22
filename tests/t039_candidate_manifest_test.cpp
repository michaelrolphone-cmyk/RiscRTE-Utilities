// Metadata-only T039 candidate check. Production parser/identity/declaration
// helpers are compiled unchanged from the selected Runtime Git objects.
// This does not invoke Runtime::appPolicies, grant providers, load an ELF,
// establish install compatibility, or execute target instructions.
#include "bootstrap/AppDeclarations.h"
#include "bootstrap/AppRequirementLimits.h"
#include "runtime/packages/PackageIdentity.h"
#include <cstdio>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

namespace {
constexpr size_t ManifestLimit = 2048;
struct Requirement { const char* capability; uint32_t api; };
constexpr Requirement Required[] = {
    {"display.output", 1}, {"input.touch.raw", 1}, {"rtc.clock", 2},
    {"storage.key-value", 1}, {"alarm.service", 1}, {"telemetry.broadcast", 1},
    {"contexts.service", 1}, {"input.power-key", 1}, {"power.sleep", 1},
    {"display.power", 1}, {"board.battery", 1}, {"net.wifi", 1},
    {"bluetooth.hci", 1}, {"motion.accel", 1}, {"bluetooth.hid", 1},
    {"storage.key-value.atomic", 1},
};
static_assert(RISC_APP_REQUIREMENT_ROWS == 16, "Keep the pinned default declaration bound");
static_assert(sizeof(Required) / sizeof(*Required) == 16, "Exact sixteen requirements");

// The top-level field/type predicate mirrors the metadata portion of pinned
// bootstrap/Runtime.cpp::appPolicies. Its policy/path/grant/provider logic is
// deliberately not reproduced. All parsing and leaf validation below use the
// production helpers rather than a second JSON or version parser.
const char* check(const std::string& raw) {
    if (raw.empty() || raw.size() > ManifestLimit) return "package-size";
    JsonDocument doc;
    if (!RiscBoot::parse(raw.data(), raw.size(), doc)) return "json";
    const auto manifest = doc.as<JsonObjectConst>();
    if (!RiscBoot::keys(manifest,
            {"type", "id", "version", "architecture", "file_name", "entry", "requires"},
            {"description", "display_name", "icon", "supported_file_types", "runtime_lifecycle",
             "launcher_visible", "min_firmware_version", "optional"})) return "schema";
    char id[64]{}, version[32]{}, artifact[128]{};
    if (!RiscBoot::eq(manifest["type"], "application") ||
        !RiscBoot::eq(manifest["architecture"], "xtensa-esp32s3") ||
        !RiscBoot::eq(manifest["entry"], "app_main") ||
        (!manifest["runtime_lifecycle"].isUnbound() &&
         !RiscBoot::eq(manifest["runtime_lifecycle"], "foreground") &&
         !RiscBoot::eq(manifest["runtime_lifecycle"], "legacy")) ||
        !RiscBoot::text(manifest["id"], id, sizeof(id)) || !RuntimePackages::safeId(id) ||
        !RiscBoot::text(manifest["version"], version, sizeof(version)) || !RuntimePackages::safeVersion(version) ||
        !RiscBoot::text(manifest["file_name"], artifact, sizeof(artifact)) ||
        !RuntimePackages::safeArtifact(artifact)) return "identity";
    const RiscBoot::AppDeclarations declarations(manifest);
    if (declarations.error(RISC_APP_REQUIREMENT_ROWS)) return "declarations";
    // Candidate-specific contract is deliberately narrower than Runtime's
    // generic parser. These constants confer no grants and change no ABI.
    if (!RiscBoot::keys(manifest,
            {"type", "id", "version", "architecture", "file_name", "entry", "requires", "display_name", "icon"}) ||
        std::strcmp(id, "ble_buttons") || std::strcmp(version, "0.1.31") ||
        std::strcmp(artifact, "ble_buttons.elf") ||
        !RiscBoot::eq(manifest["display_name"], "BLE Buttons") ||
        !RiscBoot::eq(manifest["icon"], "solid:f11c") || declarations.size() != 16) return "contract";
    for (size_t i = 0; i < 16; ++i) {
        const auto row = declarations[i];
        if (!declarations.required(i) || !RiscBoot::eq(row["capability"], Required[i].capability) ||
            row["api"].as<uint32_t>() != Required[i].api) return "contract";
    }
    return nullptr;
}

std::string changed(const std::string& original, const std::function<void(JsonDocument&)>& mutate) {
    JsonDocument doc;
    if (!RiscBoot::parse(original.data(), original.size(), doc)) return {};
    mutate(doc);
    std::string result;
    serializeJson(doc, result);
    return result;
}

struct Control { const char* name; std::string bytes; const char* boundary; };
}

int main(int argc, char** argv) {
    if (argc != 2) { std::fputs("usage: manifest-test MANIFEST\n", stderr); return 2; }
    std::ifstream stream(argv[1], std::ios::binary);
    if (!stream) { std::fputs("manifest unreadable\n", stderr); return 2; }
    const std::string original((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (const char* error = check(original)) {
        std::fprintf(stderr, "candidate manifest rejected at %s\n", error); return 1;
    }
    std::vector<Control> controls;
    const auto add = [&](const char* name, const char* boundary, const std::function<void(JsonDocument&)>& mutate) {
        controls.push_back({name, changed(original, mutate), boundary});
    };
    add("prerelease-version", "identity", [](JsonDocument& d) { d["version"] = "0.1.31-dev1"; });
    add("build-metadata-version", "identity", [](JsonDocument& d) { d["version"] = "0.1.31+build"; });
    add("incomplete-version", "identity", [](JsonDocument& d) { d["version"] = "0.1"; });
    add("overflow-version", "identity", [](JsonDocument& d) { d["version"] = "4294967296.1.1"; });
    add("different-candidate-version", "contract", [](JsonDocument& d) { d["version"] = "0.1.32"; });
    add("wrong-type", "identity", [](JsonDocument& d) { d["type"] = "driver"; });
    add("wrong-architecture", "identity", [](JsonDocument& d) { d["architecture"] = "x86_64"; });
    add("wrong-entry", "identity", [](JsonDocument& d) { d["entry"] = "driver_main"; });
    add("unsafe-id", "identity", [](JsonDocument& d) { d["id"] = "../ble_buttons"; });
    add("different-id", "contract", [](JsonDocument& d) { d["id"] = "another_app"; });
    add("traversal-artifact", "identity", [](JsonDocument& d) { d["file_name"] = "../ble_buttons.elf"; });
    add("absolute-artifact", "identity", [](JsonDocument& d) { d["file_name"] = "/ble_buttons.elf"; });
    add("different-artifact", "contract", [](JsonDocument& d) { d["file_name"] = "other.elf"; });
    add("missing-requires", "schema", [](JsonDocument& d) { d.remove("requires"); });
    add("nonarray-requires", "declarations", [](JsonDocument& d) { d["requires"] = "not-an-array"; });
    add("missing-required-row", "contract", [](JsonDocument& d) { d["requires"].as<JsonArray>().remove(15); });
    add("seventeenth-row", "declarations", [](JsonDocument& d) {
        const auto row = d["requires"].as<JsonArray>().add<JsonObject>(); row["capability"] = "test.extra"; row["api"] = 1;
    });
    add("duplicate-requirement", "declarations", [](JsonDocument& d) { d["requires"][15]["capability"] = "display.output"; });
    add("optional-overflow", "declarations", [](JsonDocument& d) {
        const auto row = d["optional"].to<JsonArray>().add<JsonObject>(); row["capability"] = "test.extra"; row["api"] = 1;
    });
    add("nonobject-row", "declarations", [](JsonDocument& d) { d["requires"][0] = "display.output"; });
    add("extra-row-key", "declarations", [](JsonDocument& d) { d["requires"][0]["grant"] = true; });
    add("missing-row-api", "declarations", [](JsonDocument& d) { d["requires"][0].as<JsonObject>().remove("api"); });
    add("string-api", "declarations", [](JsonDocument& d) { d["requires"][0]["api"] = ">=1"; });
    add("zero-api", "declarations", [](JsonDocument& d) { d["requires"][0]["api"] = 0; });
    add("negative-api", "declarations", [](JsonDocument& d) { d["requires"][0]["api"] = -1; });
    add("fractional-api", "declarations", [](JsonDocument& d) { d["requires"][0]["api"] = 1.5; });
    add("boolean-api", "declarations", [](JsonDocument& d) { d["requires"][0]["api"] = true; });
    add("overflow-api", "declarations", [](JsonDocument& d) { d["requires"][0]["api"] = uint64_t(UINT32_MAX) + 1; });
    add("wrong-api-pair", "contract", [](JsonDocument& d) { d["requires"][2]["api"] = 1; });
    add("wrong-capability-pair", "contract", [](JsonDocument& d) { d["requires"][0]["capability"] = "test.other"; });
    add("provider-only-capability", "declarations", [](JsonDocument& d) { d["requires"][0]["capability"] = RISC_BOUND_KEY_VALUE_CAPABILITY; });
    add("size-field-not-runtime-schema", "schema", [](JsonDocument& d) { d["size_bytes"] = 52; });
    add("sha-field-not-runtime-schema", "schema", [](JsonDocument& d) { d["sha256"] = std::string(64, '0'); });
    add("unknown-top-level-key", "schema", [](JsonDocument& d) { d["grants"] = true; });
    const auto compact = changed(original, [](JsonDocument&) {});
    controls.push_back({"duplicate-json-key", compact.substr(0, compact.size()-1) + ",\"id\":\"ble_buttons\"}", "json"});
    controls.push_back({"escaped-json-key", compact.substr(0, compact.size()-1) + ",\"\\u0069d\":\"ble_buttons\"}", "json"});
    controls.push_back({"trailing-content", compact + "{}", "json"});
    controls.push_back({"embedded-nul", compact + std::string(1, '\0'), "json"});
    controls.push_back({"empty-manifest", "", "package-size"});
    controls.push_back({"oversized-manifest", compact + std::string(ManifestLimit + 1 - compact.size(), ' '), "package-size"});

    JsonDocument report;
    report["manifest_metadata_accepted"] = true;
    report["exact_sixteen_requirements"] = true;
    report["candidate_version"] = "0.1.31";
    report["declaration_limit"] = RISC_APP_REQUIREMENT_ROWS;
    report["production_json_identity_declaration_helpers"] = true;
    report["full_runtime_app_policies_executed"] = false;
    report["owner_grants_qualified"] = false;
    report["installation_qualified"] = false;
    report["target_instructions_executed"] = false;
    auto outcomes = report["case_outcomes"].to<JsonArray>();
    auto positive = outcomes.add<JsonObject>();
    positive["id"] = "exact-candidate-manifest";
    positive["accepted"] = true; positive["passed"] = true;
    auto results = report["negative_controls"].to<JsonArray>();
    for (const auto& control : controls) {
        const char* actual = check(control.bytes);
        if (!actual || std::strcmp(actual, control.boundary)) {
            std::fprintf(stderr, "%s: expected rejection at %s; got %s\n", control.name,
                         control.boundary, actual ? actual : "accepted"); return 1;
        }
        auto row = results.add<JsonObject>();
        row["id"] = control.name; row["rejected"] = true; row["boundary"] = actual;
        auto outcome = outcomes.add<JsonObject>();
        outcome["id"] = control.name; outcome["accepted"] = false;
        outcome["passed"] = true; outcome["boundary"] = actual;
    }
    report["negative_control_count"] = controls.size();
    std::string output; serializeJson(report, output); std::puts(output.c_str());
    return 0;
}
