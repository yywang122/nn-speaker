#include <Arduino.h>
#include <esp_heap_caps.h>

#include "MemoryAssignment.h"

void applyMemoryAssignmentPolicy()
{
#ifdef BOARD_HAS_PSRAM
    // Prefer external RAM for generic malloc to keep internal RAM for TLS handshake.
    heap_caps_malloc_extmem_enable(0);
#endif
}

void printMemoryAssignmentInfo()
{
    Serial.printf("Total heap: %d\n", ESP.getHeapSize());
    Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
    Serial.printf("Total PSRAM: %d\n", ESP.getPsramSize());
    Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());
    Serial.printf("Internal heap free: %u\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    Serial.printf("Internal heap largest block: %u\n", heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    Serial.printf("Chip model: %s\n", ESP.getChipModel());
}
