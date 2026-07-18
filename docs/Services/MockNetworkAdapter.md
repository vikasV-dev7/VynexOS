<!-- AUTO-GENERATED START -->
# [[MockNetworkAdapter]]

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
- `~[[MockNetworkAdapter]]() override;`
- `[[nodiscard]] std::expected<void, [[NetworkError]]> initialize() override;`
- `void set_receive_callback(NetworkReceiveCallback callback) override;`
- `[[nodiscard]] std::expected<void, [[NetworkError]]> send_packet(std::span<const uint8_t> data) override;`
- `void shutdown() override;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T06:24:40.172641+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/src/hal/mock_network_adapter.hpp`

<!-- AUTO-GENERATED END -->


