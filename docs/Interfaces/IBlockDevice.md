<!-- AUTO-GENERATED START -->
# [[IBlockDevice]]

## Why the interface exists & Architectural role
_Not explicitly documented._

## Lifetime & Ownership expectations
**Lifetime**: _Unspecified_
**Ownership**: _Unspecified_

## Threading guarantees & Failure behaviour
**Thread Safety**: _Unspecified_
**Failure Handling**: _Unspecified_

## Extension guidance
_No explicit extension points defined._

## Public API
- `virtual ~[[IBlockDevice]]() = default;`
- `[[nodiscard]] virtual std::expected<void, [[StorageError]]> initialize() = 0;`
- `[[nodiscard]] virtual std::expected<void, [[StorageError]]> read_blocks(uint64_t start_block, size_t block_count, std::span<uint8_t> buffer) = 0;`
- `[[nodiscard]] virtual std::expected<void, [[StorageError]]> write_blocks(uint64_t start_block, std::span<const uint8_t> buffer) = 0;`
- `[[nodiscard]] virtual uint64_t get_capacity_blocks() const = 0;`
- `[[nodiscard]] virtual size_t get_block_size() const = 0;`
- `virtual void shutdown() = 0;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T08:05:14.381686+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/hal/storage.hpp`

<!-- AUTO-GENERATED END -->










