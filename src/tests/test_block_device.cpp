// MockBlockDevice: sparse, allocate-on-write storage behind the unchanged IBlockDevice interface.
// Covers initialization (nothing allocated, even at the 512 MiB capacity the system uses), read/write correctness across
// chunk boundaries, every bounds rule including arithmetic that used to wrap around, and measured allocation behaviour.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "../hal/mock_block_device.hpp"
#include "vynexos/core/logger.hpp"

#include "test_check.hpp"

using namespace vynexos;
using hal::MockBlockDevice;
using hal::StorageError;

namespace {

class CountingLogger : public core::ILogger {
public:
    void set_log_level(core::LogLevel) override {}
    [[nodiscard]] core::LogLevel get_log_level() const override { return core::LogLevel::Info; }
    void log_raw(core::LogLevel level, std::string_view) override {
        if (level >= core::LogLevel::Error) ++errors;
    }
    int errors = 0;
};

constexpr std::size_t kBlock = 512;
constexpr uint64_t kSystemCapacity = 1024 * 1024; // what CompositionRoot creates: 512 MiB
constexpr std::size_t kBlocksPerChunk = MockBlockDevice::kChunkBytes / kBlock;

std::vector<uint8_t> pattern(std::size_t blocks, uint8_t seed) {
    std::vector<uint8_t> data(blocks * kBlock);
    for (std::size_t i = 0; i < data.size(); ++i) data[i] = static_cast<uint8_t>(seed + i * 7 + i / kBlock);
    return data;
}

/// True when the operation failed with exactly this error (never reads error() of a success).
bool fails_with(const std::expected<void, StorageError>& result, StorageError error) {
    return !result.has_value() && result.error() == error;
}

bool all_equal(const std::vector<uint8_t>& data, uint8_t value) {
    return std::all_of(data.begin(), data.end(), [value](uint8_t b) { return b == value; });
}

void test_initialization_allocates_nothing() {
    auto logger = std::make_shared<CountingLogger>();
    MockBlockDevice device(logger, kSystemCapacity);
    std::vector<uint8_t> buffer(kBlock);
    CHECK(fails_with(device.read_blocks(0, 1, buffer), StorageError::DeviceNotReady)); // not initialized yet
    CHECK(fails_with(device.write_blocks(0, buffer), StorageError::DeviceNotReady));
    CHECK(device.initialize().has_value());
    CHECK(device.initialize().has_value()); // idempotent
    CHECK(device.get_capacity_blocks() == kSystemCapacity);
    CHECK(device.get_block_size() == kBlock);
    CHECK(device.allocated_bytes() == 0); // the eager implementation held 512 MiB here
    std::printf("test_initialization_allocates_nothing passed (capacity %llu MiB, allocated 0)\n",
                static_cast<unsigned long long>(kSystemCapacity * kBlock / (1024 * 1024)));
}

void test_unwritten_blocks_read_as_zeros() {
    auto logger = std::make_shared<CountingLogger>();
    MockBlockDevice device(logger, kSystemCapacity);
    CHECK(device.initialize().has_value());
    std::vector<uint8_t> buffer(4 * kBlock, 0xCD);
    CHECK(device.read_blocks(kSystemCapacity / 2, 4, buffer).has_value());
    CHECK(all_equal(buffer, 0));
    CHECK(device.allocated_bytes() == 0); // reading allocates nothing
    std::printf("test_unwritten_blocks_read_as_zeros passed\n");
}

void test_round_trip_across_chunk_boundaries() {
    auto logger = std::make_shared<CountingLogger>();
    MockBlockDevice device(logger, kSystemCapacity);
    CHECK(device.initialize().has_value());

    // Three blocks straddling the first chunk boundary.
    const uint64_t start = kBlocksPerChunk - 1;
    const auto data = pattern(3, 11);
    CHECK(device.write_blocks(start, data).has_value());
    CHECK(device.allocated_bytes() == 2 * MockBlockDevice::kChunkBytes);
    std::vector<uint8_t> back(data.size());
    CHECK(device.read_blocks(start, 3, back).has_value());
    CHECK(back == data);

    // A read that covers written and unwritten blocks gets both right.
    std::vector<uint8_t> wide(6 * kBlock, 0xCD);
    CHECK(device.read_blocks(start - 1, 6, wide).has_value());
    CHECK(std::all_of(wide.begin(), wide.begin() + kBlock, [](uint8_t b) { return b == 0; }));
    CHECK(std::equal(data.begin(), data.end(), wide.begin() + kBlock));
    CHECK(std::all_of(wide.begin() + 4 * kBlock, wide.end(), [](uint8_t b) { return b == 0; }));

    // Overwriting one block changes only that block.
    const auto one = pattern(1, 200);
    CHECK(device.write_blocks(start + 1, one).has_value());
    CHECK(device.read_blocks(start, 3, back).has_value());
    CHECK(std::equal(data.begin(), data.begin() + kBlock, back.begin()));
    CHECK(std::equal(one.begin(), one.end(), back.begin() + kBlock));
    CHECK(std::equal(data.begin() + 2 * kBlock, data.end(), back.begin() + 2 * kBlock));

    // A larger transfer: 300 blocks (several chunks) from an unaligned start.
    const auto big = pattern(300, 3);
    CHECK(device.write_blocks(1000, big).has_value());
    std::vector<uint8_t> big_back(big.size());
    CHECK(device.read_blocks(1000, 300, big_back).has_value());
    CHECK(big_back == big);
    CHECK(logger->errors == 0);
    std::printf("test_round_trip_across_chunk_boundaries passed\n");
}

void test_boundaries() {
    auto logger = std::make_shared<CountingLogger>();
    const uint64_t capacity = 1024;
    MockBlockDevice device(logger, capacity);
    CHECK(device.initialize().has_value());
    const auto block = pattern(1, 42);
    std::vector<uint8_t> back(kBlock);

    // The last block is usable; one past it is not.
    CHECK(device.write_blocks(capacity - 1, block).has_value());
    CHECK(device.read_blocks(capacity - 1, 1, back).has_value() && back == block);
    CHECK(fails_with(device.write_blocks(capacity, block), StorageError::OutOfBounds));
    CHECK(fails_with(device.read_blocks(capacity, 1, back), StorageError::OutOfBounds));
    CHECK(fails_with(device.write_blocks(capacity - 1, pattern(2, 1)), StorageError::OutOfBounds)); // runs past the end
    std::vector<uint8_t> two(2 * kBlock);
    CHECK(fails_with(device.read_blocks(capacity - 1, 2, two), StorageError::OutOfBounds));
    // Empty transfers at the very end are in bounds and do nothing.
    CHECK(device.read_blocks(capacity, 0, back).has_value());
    CHECK(device.write_blocks(capacity, std::span<const uint8_t>{}).has_value());

    // Arguments whose sum wraps around 2^64 (the old check `start + count > capacity` passed for these and copied
    // outside the storage).
    const uint64_t max = std::numeric_limits<uint64_t>::max();
    CHECK(fails_with(device.read_blocks(max, 1, back), StorageError::OutOfBounds));
    CHECK(fails_with(device.read_blocks(max - 1, 2, two), StorageError::OutOfBounds));
    CHECK(fails_with(device.write_blocks(max, block), StorageError::OutOfBounds));
    CHECK(fails_with(device.read_blocks(1, std::numeric_limits<size_t>::max(), back), StorageError::OutOfBounds));

    // Buffer rules.
    std::vector<uint8_t> small(kBlock - 1);
    CHECK(fails_with(device.read_blocks(0, 1, small), StorageError::OutOfBounds)); // too small
    CHECK(fails_with(device.write_blocks(0, std::vector<uint8_t>(kBlock + 1)), StorageError::OutOfBounds)); // not whole blocks
    CHECK(logger->errors > 0); // each refusal is logged as an error
    // Nothing above touched block 0 or the last block's neighbour.
    CHECK(device.read_blocks(0, 1, back).has_value() && all_equal(back, 0));
    CHECK(device.read_blocks(capacity - 1, 1, back).has_value() && back == block);
    std::printf("test_boundaries passed\n");
}

void test_allocation_follows_written_data() {
    auto logger = std::make_shared<CountingLogger>();
    MockBlockDevice device(logger, kSystemCapacity);
    CHECK(device.initialize().has_value());

    // Zeros written where nothing was ever written allocate nothing (a format of an empty disk costs no memory).
    CHECK(device.write_blocks(0, std::vector<uint8_t>(64 * kBlock, 0)).has_value());
    CHECK(device.allocated_bytes() == 0);

    // One block at each end of the 512 MiB device: two chunks.
    CHECK(device.write_blocks(0, pattern(1, 1)).has_value());
    CHECK(device.write_blocks(kSystemCapacity - 1, pattern(1, 2)).has_value());
    CHECK(device.allocated_bytes() == 2 * MockBlockDevice::kChunkBytes);

    // Writing within an allocated chunk allocates nothing more; zeros there do overwrite the data.
    CHECK(device.write_blocks(5, pattern(1, 3)).has_value());
    CHECK(device.allocated_bytes() == 2 * MockBlockDevice::kChunkBytes);
    CHECK(device.write_blocks(5, std::vector<uint8_t>(kBlock, 0)).has_value());
    std::vector<uint8_t> back(kBlock, 0xCD);
    CHECK(device.read_blocks(5, 1, back).has_value() && all_equal(back, 0));

    // 1 MiB of data costs 1 MiB (plus at most one partial chunk at each end).
    CHECK(device.write_blocks(4096, pattern(2048, 9)).has_value());
    const std::size_t expected = 2 * MockBlockDevice::kChunkBytes + 2048 * kBlock;
    CHECK(device.allocated_bytes() == expected);

    // Shutdown releases everything; the device then reads as new.
    device.shutdown();
    CHECK(device.allocated_bytes() == 0);
    CHECK(device.initialize().has_value());
    CHECK(device.read_blocks(0, 1, back).has_value() && all_equal(back, 0));
    std::printf("test_allocation_follows_written_data passed (512 MiB device, %zu KiB allocated for 1 MiB + 3 blocks)\n",
                expected / 1024);
}

void test_unaddressable_capacity_is_refused() {
    auto logger = std::make_shared<CountingLogger>();
    MockBlockDevice device(logger, std::numeric_limits<uint64_t>::max()); // byte offsets would not fit in 64 bits
    CHECK(fails_with(device.initialize(), StorageError::HardwareFailure));
    std::vector<uint8_t> buffer(kBlock);
    CHECK(fails_with(device.read_blocks(0, 1, buffer), StorageError::DeviceNotReady));
    CHECK(logger->errors == 1);
    std::printf("test_unaddressable_capacity_is_refused passed\n");
}

} // namespace

int main() {
    std::printf("Running block device tests...\n");
    test_initialization_allocates_nothing();
    test_unwritten_blocks_read_as_zeros();
    test_round_trip_across_chunk_boundaries();
    test_boundaries();
    test_allocation_follows_written_data();
    test_unaddressable_capacity_is_refused();
    std::printf("All block device tests passed.\n");
    return 0;
}
