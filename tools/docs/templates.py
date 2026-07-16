INTERFACE_TEMPLATE = """# {name}

## Purpose
{purpose}

## Namespace
`{namespace}`

## Source Location
`{source_path}`

## Responsibilities
{responsibilities}

## Dependencies
{dependencies}

## Methods
{methods}

## Thread Safety
Implementations must ensure thread safety if accessed across multiple sub-systems via Dependency Injection.

## Used By
{used_by}

## Implemented By
{implemented_by}
"""

SERVICE_TEMPLATE = """# {name}

## Purpose
{purpose}

## Source Location
`{source_path}`

## Implemented Interfaces
{interfaces}

## Dependencies
{dependencies}

## Initialization
{initialization}

## Internal Design & State Machine
{internal_design}

## Interactions
{interactions}

## Future Improvements
{future}
"""
