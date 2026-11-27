from __future__ import annotations

from build123d import BasePartObject


def hole_depth(depth: float | None) -> float:
    """Defaults to through the part in BuildPart mode, as in bd_warehouse. Call from a custom object's __init__.

    There is no public API for the enclosing builder, so this uses the protected BaseObject hook that build123d's own
    Hole uses for its through depth.
    """
    if depth is not None:
        return depth
    context = BasePartObject._get_builder_context()
    if context is None:
        raise ValueError("depth is required outside of BuildPart")
    return 2 * context.max_dimension
