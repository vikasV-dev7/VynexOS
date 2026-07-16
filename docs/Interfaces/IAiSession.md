<!-- AUTO-GENERATED START -->
# [[IAiSession]]

## Purpose
Interface for [[IAiSession]].

## Namespace
`vynexos::ai`

## Source Location
`C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/ai/ai_runtime.hpp`

## Responsibilities
To be documented.

## Dependencies
None (pure interface).

## Methods
- `virtual ~[[IAiSession]]() = default`
- `virtual void prompt_async(std::string_view prompt, AsyncCallback callback) = 0`

## Thread Safety
Implementations must ensure thread safety if accessed across multiple sub-systems via Dependency Injection.

## Used By
See [[Composition Root]]

## Implemented By
See Services directory.

<!-- AUTO-GENERATED END -->
