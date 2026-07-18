<!-- AUTO-GENERATED START -->
# [[INetworkAdapter]]

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
- `virtual ~[[INetworkAdapter]]() = default;`
- `[[nodiscard]] virtual std::expected<void, [[NetworkError]]> initialize() = 0;`
- `virtual void set_receive_callback(NetworkReceiveCallback callback) = 0;`
- `[[nodiscard]] virtual std::expected<void, [[NetworkError]]> send_packet(std::span<const uint8_t> data) = 0;`
- `virtual void shutdown() = 0;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T08:05:14.367303+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/hal/network.hpp`

<!-- AUTO-GENERATED END -->










