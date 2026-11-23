"""Shared internals for bd_util Part objects."""
from __future__ import annotations

from build123d.build_part import BuildPart
from build123d.geometry import Plane

BORE_PLANE = Plane((0, 0, 0), z_dir=(0, 0, -1))  # hole convention: surface at the origin, cutting toward -Z


def hole_depth(context: BuildPart | None, depth: float | None) -> float:
  """Hole depth, defaulting to through the part in BuildPart mode, as in bd_warehouse."""
  if depth is not None:
    return depth
  elif context is not None:
    return 2 * context.max_dimension
  else:
    raise ValueError("No depth provided")
