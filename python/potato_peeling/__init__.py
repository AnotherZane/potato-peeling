"""potato-peeling: the largest convex polygon inside a simple polygon."""
import sys

from ._core import aronov, cabello, chang_and_yap, refinement

__all__ = ["aronov", "cabello", "chang_and_yap", "refinement"]

# Make `import potato_peeling.aronov` work, not just attribute access.
for _name in __all__:
    sys.modules[f"{__name__}.{_name}"] = globals()[_name]
