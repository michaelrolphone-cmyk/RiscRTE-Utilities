#pragma once
/* Explicit build profile. The default preserves both real input dependencies. */
#ifndef CONTEXTS_RF_ONLY
#define CONTEXTS_RF_ONLY 0
#endif
#if CONTEXTS_RF_ONLY != 0 && CONTEXTS_RF_ONLY != 1
#error "CONTEXTS_RF_ONLY must be 0 or 1"
#endif
#define CONTEXTS_HAS_AUDIO (!CONTEXTS_RF_ONLY)
#define CONTEXTS_SUPPORTED_SOURCES (CONTEXTS_RF_ONLY ? CONTEXTS_RADIO : CONTEXTS_ALL)
static inline bool contexts_source_supported(uint32_t source) {
    return (source == CONTEXTS_AUDIO || source == CONTEXTS_RADIO) &&
           (source & CONTEXTS_SUPPORTED_SOURCES);
}
