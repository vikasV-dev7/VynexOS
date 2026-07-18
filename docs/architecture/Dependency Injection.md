<!-- AUTO-GENERATED START -->
# Dependency Injection

## Dependency Injection
Strict constructor injection.

## Testing and Encapsulation
As highlighted in [[INC-0004_CompositionRoot_Encapsulation_Violation]], tests must never violate architectural boundaries or encapsulation to inject mocks. All mock injection must happen either via public constructor injection boundaries or through standard interfaces exposed by the DI container. Extracting private dependencies from the root container is strictly forbidden.
<!-- AUTO-GENERATED END -->
