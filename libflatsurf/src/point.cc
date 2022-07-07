/**********************************************************************
 *  This file is part of flatsurf.
 *
 *        Copyright (C) 2022 Julian Rüth
 *
 *  Flatsurf is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Flatsurf is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with flatsurf. If not, see <https://www.gnu.org/licenses/>.
 *********************************************************************/

#include <ostream>
#include <stdexcept>

#include "../flatsurf/point.hpp"
#include "../flatsurf/ccw.hpp"
#include "../flatsurf/edge.hpp"
#include "../flatsurf/vertex.hpp"
#include "../flatsurf/half_edge.hpp"
#include "../flatsurf/half_edge_set.hpp"
#include "../flatsurf/half_edge_set_iterator.hpp"
#include "../flatsurf/vector.hpp"
#include "util/assert.ipp"
#include "impl/point.impl.hpp"

namespace flatsurf {

namespace {
template<typename>
struct size;

template<typename T, size_t N>
struct size<std::array<T, N> > {
    static size_t const value = N;
};

template <typename A, size_t... I>
auto as_tuple(const A& array, std::index_sequence<I...>) {
  return std::make_tuple(array[I]...);
}

template <typename A, size_t N = size<A>::value>
auto as_tuple(const A& array) {
  return as_tuple(array, std::make_index_sequence<N>{});
}

}

template <typename Surface>
Point<Surface>::Point(const Surface& surface, const Vertex& vertex) : Point(surface, *vertex.outgoing().begin(), T(1), T(), T()) {}

template <typename Surface>
Point<Surface>::Point(const Surface& surface, HalfEdge face, const T& a, const T& b, const T& c) : self(spimpl::make_impl<ImplementationOf<Point>>(surface, face, a, b, c)) {
  self->normalize();
}

template <typename Surface>
Point<Surface>::Point(const Surface& surface, HalfEdge face, const std::array<T, 3>& coordinates) : Point(surface, face, std::move(coordinates[0]), std::move(coordinates[1]), std::move(coordinates[2])) {
}

template <typename Surface>
std::array<typename Surface::Coordinate, 3> Point<Surface>::coordinates(HalfEdge face) const {
  LIBFLATSURF_CHECK_ARGUMENT(in(face), "point has no coordinates in this face");

  // If the point is specified with respect to face, return its coordinates as stored.
  for (const auto side : self->surface->face(face))
    if (side == self->face)
      return self->rotated(face);

  // If this is a vertex or on an edge of the face, translate the coordinates
  // into ``face``.
  for (const auto side : self->surface->face(face)) {
    if (self->face == -side and !self->c)
      return ImplementationOf<Point>{*self->surface, side, self->b, self->a, self->c}.rotated(face);
    if (self->surface->previousInFace(self->face) == -side and !self->b and !self->c)
      return ImplementationOf<Point>{*self->surface, side, self->a, self->b, self->c}.rotated(face);
  }

  const auto vertex = this->vertex();

  LIBFLATSURF_ASSERT(vertex, "point is in this face but not in the interior or on an edge so it must be a vertex");

  for (const auto side : self->surface->face(face))
    if (vertex == Vertex::source(side, *self->surface))
      return ImplementationOf<Point>{*self->surface, side, T(1), T(), T()}.rotated(face);

  LIBFLATSURF_UNREACHABLE("point is in this face so it must be either inside the face or on its boundary");
}

template <typename Surface>
const Surface& Point<Surface>::surface() const {
  return *self->surface;
}

template <typename Surface>
bool Point<Surface>::operator==(const Point& rhs) const {
  if (!rhs.in(self->face))
    return false;

  if (vertex())
    return vertex() == rhs.vertex();

  const auto [a_, b_, c_] = rhs.coordinates(self->face);

  const T λ = self->a + self->b + self->c;
  const T μ_ = a_ + b_ + c_;

  return λ * a_ == μ_ * self->a && λ * b_ == μ_ * self->b && λ * c_ == μ_ * self->c;
}

template <typename Surface>
HalfEdge Point<Surface>::face() const {
  return self->face;
}

template <typename Surface>
std::optional<Vertex> Point<Surface>::vertex() const {
  if (self->b || self->c)
    // normalize() makes sure that vertices are of the form {1, 0, 0}.
    return std::nullopt;

  return Vertex::source(self->face, *self->surface);
}

template <typename Surface>
std::optional<Edge> Point<Surface>::edge() const {
  // normalize() makes sure that edge points are of the form {a, b, 0}.
  if (self->c)
    return std::nullopt;

  return self->face;
}

template <typename Surface>
bool Point<Surface>::in(HalfEdge face) const {
  for (const auto& side : self->surface->face(face))
    if (self->face == side)
      return true;

  for (const auto& side : self->surface->face(face))
    if (on(side))
      return true;

  return false;
}

template <typename Surface>
bool Point<Surface>::on(Edge edge) const {
  const auto vertex = this->vertex();
  if (vertex)
    return *vertex == Vertex::source(edge.positive(), *self->surface) || *vertex == Vertex::source(edge.negative(), *self->surface);

  return this->edge() == edge;
}

template <typename Surface>
bool Point<Surface>::at(const Vertex& vertex) const {
  return this->vertex() == vertex;
}

template <typename Surface>
ImplementationOf<Point<Surface>>::ImplementationOf(const Surface& surface, HalfEdge face, T a, T b, T c) : surface(surface), face(face), a(std::move(a)), b(std::move(b)), c(std::move(c)) {
}

template <typename Surface>
void ImplementationOf<Point<Surface>>::normalize() {
  if (a + b + c == 0)
    throw std::invalid_argument("cannot create point from barycentric coordinates that sum to zero");

  // Normalize if outside a face.
  {
    if (a < 0 || b < 0 || c < 0)
      // This is not hard to do actually but it's hard to make this roundtrip with coordinates().
      throw std::logic_error("not implemented: cannot normalize point outside of a face");
  }

  // Normalize strange inputs.
  {
    if (a + b + c < 0) {
      a *= -1;
      b *= -1;
      c *= -1;
    }
  }

  // Normalize inside a face.
  {
    if (a > 0 && b > 0 && c > 0)
      return;
  }

  // Normalize if on an edge or vertex.
  {
    while (c || !a) {
      const auto face = surface->nextInFace(this->face);
      std::tie(a, b, c) = as_tuple(rotated(face));
      this->face = face;
    }
  }

}

template <typename Surface>
std::array<typename Surface::Coordinate, 3> ImplementationOf<Point<Surface>>::rotated(HalfEdge face) const {
  if (face == this->face)
    return {a, b, c};

  if (face == surface->nextInFace(this->face))
    return {b, c, a};

  LIBFLATSURF_ASSERT_ARGUMENT(face == surface->previousInFace(this->face), "half edge is not delimiting this face");

  return {c, a, b};
}

template <typename Surface>
void ImplementationOf<Point<Surface>>::rotate(HalfEdge face) {
  if (face == surface->nextInFace(this->face)) {
    // Shift (a, b, c) to (b, c, a) i.e., apply (a c b)=(b c)(a b)
    std::swap(a, b);
    std::swap(b, c);
  } else if (face == surface->previousInFace(this->face)) {
    // Shift (a, b, c) to (c, a, b) i.e., apply (a b c)=(a b)(b c)
    std::swap(b, c);
    std::swap(a, b);
  } else
    LIBFLATSURF_ASSERT(face == this->face, "cannot rotate to a side not delimiting this face");

  this->face = face;
}

template <typename Surface>
std::array<typename Surface::Coordinate, 3> ImplementationOf<Point<Surface>>::crossed() const {
  // Consider the face (A, B, C) where our ``face`` goes from A to B.
  // Let (B, A, D) be the face on the other side of ``face``.

  // Let us first write C with coordinates in (B, A, D).
  // If we pretend that D is the origin, we can write C = a_c A + b_c B + d_c D with an arbitrary d_c.
  // For any λ if we set d_c such that λ a_c + λ b_c + d_c = λ, this gives
  // barycentric coordinates for C. (Classically, one would pick λ=1 but
  // since we want to avoid divisions, we are going to collect the divisors
  // into the λ.)
  // To determine a_c and b_c we solve the system:
  // / B_x A_x \  / b_c \   / C_x \
  // |         |  |     | = |     |
  // \ B_y A_y /  \ a_c /   \ C_y /
  const auto A = -surface->fromHalfEdge(surface->nextInFace(-face));
  const auto B = surface->fromHalfEdge(surface->previousInFace(-face));
  const auto C = B + surface->fromHalfEdge(surface->nextInFace(face));
  const T det = B.x() * A.y() - B.y() * A.x();

  const T λb_c =  A.y() * C.x() - A.x() * C.y();
  const T λa_c = -B.y() * C.x() + B.x() * C.y();
  const T d_c = det - λa_c - λb_c;

  // Now we can rewrite this point in terms of the other face:
  // P = (aA + bB + cC) / (a + b + c)
  //   = (aA + bB + c(λ a_c A + λ b_c B + d_c D) / λ) / (a + b + c)
  // So in (denormalized) barycentric coordinates with respect to (B, A, D) we get:
  // P = (λ b + c λ b_c, λ a + c λ a_c, c d_c)
  return {
    det * b + c * λb_c,
    det * a + c * λa_c,
    c * d_c,
  };
}

template <typename Surface>
std::ostream& operator<<(std::ostream& os, const Point<Surface>& point) {
  return os << "(" << point.self->a << ", " << point.self->b << ", " << point.self->c << ") in " << "(" << point.self->face << ", " << point.self->surface->nextInFace(point.self->face) << ", " << point.self->surface->previousInFace(point.self->face) << ")";
}

}


// Instantiations of templates so implementations are generated for the linker
#include "util/instantiate.ipp"

LIBFLATSURF_INSTANTIATE_MANY_WRAPPED((LIBFLATSURF_INSTANTIATE_WITH_IMPLEMENTATION)(LIBFLATSURF_INSTANTIATE_HASH), Point, LIBFLATSURF_FLAT_TRIANGULATION_TYPES)