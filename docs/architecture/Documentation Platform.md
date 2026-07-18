# Documentation Platform Architecture

## Purpose
The VynexOS Documentation Platform is a bespoke, automated toolchain designed to generate production-quality engineering documentation directly from C++ source code. It ensures that API documentation, architectural metrics, and validation reports are always synchronized with the codebase.

## Architecture
The platform is heavily decoupled, favoring a `PluginRegistry` over hardcoded logic.
It processes data in strict, unidirectional stages:
`Parser -> Intermediate Representation (IR) -> Renderer -> Validation -> Output/Cache`

### Plugin System
All functional components implement one of the core interfaces defined in `tools/docs/plugins.py`:
- `IDocumentationParser`
- `IDocumentationRenderer`
- `IDocumentationValidator`
- `IDocumentationIndexer`
- `IDocumentationReporter`
- `IDocumentationCache`

Plugins must declare their supported `IR_VERSION`. The registry validates compatibility at startup and halts on mismatches to prevent data corruption.

## Intermediate Representation (IR)
The Intermediate Document Model (`tools/docs/model.py`) is the canonical contract between parsers and renderers. 
- **IR_VERSION 1.0**: Supports classes, interfaces, base classes, methods, and a `Metadata` block for Doxygen tags (`@Purpose`, `@ThreadSafety`, etc.).
- Every `DocumentEntity` calculates a deterministic SHA-256 hash of its properties to facilitate the Caching Strategy.

## Caching Strategy
The system uses a JSON-backed cache (`.docs_cache.json`) implementing `IDocumentationCache`.
- It caches the source file path and the `entity_hash`.
- The cache validates globally against the `IR_VERSION`, `SCHEMA_VERSION`, `generator_version`, and `parser_version`. If any toolchain version bumps, the cache automatically invalidates itself completely.
- Incremental generation vastly speeds up performance by skipping render and file-write stages for unmodified entities.

## Validation Pipeline
`DocumentationValidator` scans the output `docs/` directory post-generation.
It produces a report catching:
- Broken Markdown/Wiki links
- Orphaned files (pages lacking incoming links)
- Duplicate page titles
- Invalid Mermaid syntax injections
- Auto-generated stubs that are missing crucial engineering metadata.

## Search Index
An `IDocumentationIndexer` plugin serializes the parsed IR into a single `documentation_index.json`. This index exposes metadata, public APIs, and namespaces, serving as the foundation for static site generation, semantic search, and AI tool integration.

## Versioning Strategy
- **IR Version**: Bumps when the Python schema in `model.py` changes. Requires plugin updates.
- **Documentation Schema Version**: Bumps when the markdown output format changes.
- **Generator Version**: Bumps on CLI/Orchestrator improvements.
- **Parser Version**: Bumps on specific parser capability updates (e.g., ClangAST parser extracting new attributes).

## Performance Characteristics
- **Parsing**: Clang AST parsing is I/O intensive but accurate.
- **Caching**: The hash-based cache bypasses rendering/disk writes for ~98% of files during minor code edits.
- **Parallelization**: The system is designed functionally. While currently single-threaded, the parser execution and rendering map/reduce steps can be trivially parallelized in the future.

## Extension Guide
To add a new output format (e.g., HTML):
1. Create a class implementing `IDocumentationRenderer`.
2. Register it in `generate_docs.py` -> `build_registry()`.
3. The orchestration pipeline will automatically loop through the new renderer without needing to alter the parser or IR.

## Migration Guide
- Legacy regex users must migrate to the `ClangASTParser` as the primary engine.
- Ensure `compile_commands.json` is generated via CMake: `cmake -B build -G Ninja`.
- Install dependencies: `pip install libclang`.

## Known Limitations
- MSVC Generator does not easily support `CMAKE_EXPORT_COMPILE_COMMANDS` out-of-the-box on older CMake versions. Use Ninja.
- The `RegexParser` fallback does not correctly associate multi-line Doxygen comments with their methods.

## Future Roadmap
- Implementation of PDF Renderer.
- Parallelized AST parsing.
- Automated Graph mapping in `documentation_index.json` linking dependencies dynamically.
