<!-- AUTO-GENERATED START -->
# [[MockBlockDevice]]

## Purpose & Responsibilities
_Not explicitly documented._

## Dependencies & Interaction with CompositionRoot
_Constructor injected dependencies (Refer to Source Code)._

## Failure recovery & Performance considerations
**Failure Recovery**: _Unspecified_
**Performance**: _Unspecified_

## Extension points
_No explicit extension points defined._

## Public API
- `~[[MockBlockDevice]]() override;`
- `[[nodiscard]] std::expected<void, [[StorageError]]> initialize() override;`
- `[[nodiscard]] std::expected<void, [[StorageError]]> read_blocks(uint64_t start_block, size_t block_count, std::span<uint8_t> buffer) override;`
- `[[nodiscard]] std::expected<void, [[StorageError]]> write_blocks(uint64_t start_block, std::span<const uint8_t> buffer) override;`
- `[[nodiscard]] uint64_t get_capacity_blocks() const override;`
- `[[nodiscard]] size_t get_block_size() const override;`
- `void shutdown() override;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T06:24:40.139588+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/src/hal/mock_block_device.hpp`

<!-- AUTO-GENERATED END -->


