<!-- AUTO-GENERATED START -->
# [[ITerminal]]

## Purpose
Interface for [[ITerminal]].

## Namespace
`vynexos::apps`

## Source Location
`C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/apps/terminal.hpp`

## Responsibilities
To be documented.

## Dependencies
None (pure interface).

## Methods
- `virtual ~[[ITerminal]]() = default`
- `virtual void write_output(std::string_view text) = 0`
- `virtual void on_input(std::function<void(std::string_view)> callback) = 0`
- `virtual void request_ai_explanation(std::string_view command_context) = 0`
- `virtual void request_ai_suggestion(std::string_view intent) = 0`

## Thread Safety
Implementations must ensure thread safety if accessed across multiple sub-systems via Dependency Injection.

## Used By
See [[Composition Root]]

## Implemented By
See Services directory.

<!-- AUTO-GENERATED END -->
