#include <Arduino.h>
#include <esp_heap_caps.h>

#include "MemoryAssignment.h"

namespace
{
    struct MemorySnapshot
    {
        size_t internal;
        size_t dma;
        size_t spiram;
    };

    MemorySnapshot captureSnapshot()
    {
        MemorySnapshot snapshot;
        snapshot.internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        snapshot.dma = heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
        snapshot.spiram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        return snapshot;
    }
}

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

void runExercise2MemoryAssignmentOutput()
{
    constexpr size_t TEST_ALLOC_BYTES = 64 * 1024;

    const MemorySnapshot before = captureSnapshot();

    void *internal_ptr = heap_caps_malloc(TEST_ALLOC_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    void *dma_ptr = heap_caps_malloc(TEST_ALLOC_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    void *spiram_ptr = heap_caps_malloc(TEST_ALLOC_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    const MemorySnapshot after = captureSnapshot();

    Serial.println("=== Before Allocation ===");
    Serial.printf("INTERNAL free: %u\n", static_cast<unsigned>(before.internal));
    Serial.printf("DMA free: %u\n", static_cast<unsigned>(before.dma));
    Serial.printf("SPIRAM free: %u\n", static_cast<unsigned>(before.spiram));
    Serial.printf("INTERNAL allocation result: %s (%u bytes)\n", internal_ptr ? "SUCCESS" : "FAIL", static_cast<unsigned>(TEST_ALLOC_BYTES));
    Serial.printf("DMA allocation result: %s (%u bytes)\n", dma_ptr ? "SUCCESS" : "FAIL", static_cast<unsigned>(TEST_ALLOC_BYTES));
    Serial.printf("SPIRAM allocation result: %s (%u bytes)\n", spiram_ptr ? "SUCCESS" : "FAIL", static_cast<unsigned>(TEST_ALLOC_BYTES));

    Serial.println("=== After Allocation ===");
    Serial.printf("INTERNAL free: %u\n", static_cast<unsigned>(after.internal));
    Serial.printf("DMA free: %u\n", static_cast<unsigned>(after.dma));
    Serial.printf("SPIRAM free: %u\n", static_cast<unsigned>(after.spiram));

    Serial.println("=== Allocation Delta (After - Before) ===");
    Serial.printf("INTERNAL delta: %d\n", static_cast<int>(after.internal) - static_cast<int>(before.internal));
    Serial.printf("DMA delta: %d\n", static_cast<int>(after.dma) - static_cast<int>(before.dma));
    Serial.printf("SPIRAM delta: %d\n", static_cast<int>(after.spiram) - static_cast<int>(before.spiram));

    if (internal_ptr)
    {
        free(internal_ptr);
    }
    if (dma_ptr)
    {
        free(dma_ptr);
    }
    if (spiram_ptr)
    {
        free(spiram_ptr);
    }
}
