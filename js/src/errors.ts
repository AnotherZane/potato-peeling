/** The polygon is not a simple polygon (or a start or mesh is invalid). */
export class InvalidPolygonError extends Error {
  override name = 'InvalidPolygonError';
}

/** A result failed its own exact check: a bug in the library. */
export class CertificationError extends Error {
  override name = 'CertificationError';
}
