# Core Architecture

The Core subsystem provides foundational utilities and low-level architectural primitives that the rest of VynexOS depends upon.

## Components
- [[Composition Root]]
- [[Event Bus]]
- [[Logger]]
- [[Virtual File System]]
- [[Notification System]]
- [[Memory Manager]]
- [[Process Manager]]
- [[Kernel]]
- [[Scheduler]]
- [[Bootstrap]]
- [[IPC]]

## Interfaces
- [[IEventBus]]
- [[ILogger]]
- [[ITaskScheduler]]

## Implementations
- [[BasicTaskScheduler]]

## Related Architecture
- [[Dependency Injection]] (Strategy used by Core)
