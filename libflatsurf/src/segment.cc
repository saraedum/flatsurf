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

#include "../flatsurf/segment.hpp"

#include "../flatsurf/saddle_connection.hpp"
#include "../flatsurf/vertex.hpp"
#include "../flatsurf/edge.hpp"
#include "../flatsurf/ccw.hpp"

#include "impl/segment.impl.hpp"

#include "util/instantiate.ipp"
#include "util/assert.ipp"
#include "util/hash.ipp"

namespace flatsurf {

template <typename Surface>
Segment<Surface>::Segment(const Point<Surface>& start, const Vector<T>&) {
  throw std::logic_error("not implemented: Segment()");
}

template <typename Surface>
Segment<Surface>::Segment(const Point<Surface>& start, const Point<Surface>& end, const Vector<T>&) {
}

template <typename Surface>
Segment<Surface>::Segment(const Point<Surface>& start, HalfEdge source, const Vector<T>&) {
  throw std::logic_error("not implemented: Segment()");
}

template <typename Surface>
Segment<Surface>::Segment(const Point<Surface>& start, HalfEdge source, const Point<Surface>& end, HalfEdge target, const Vector<T>& vector) :
  self(spimpl::make_impl<ImplementationOf<Segment>>(source, start, target, end, vector)) {
  LIBFLATSURF_CHECK_ARGUMENT(&start.surface() == &end.surface(), "start and end must be defined on the same surface");
  LIBFLATSURF_CHECK_ARGUMENT(start.in(source), "start point of segment must be in source face");
  LIBFLATSURF_CHECK_ARGUMENT(end.in(target), "end point of segment must be in target face");
  LIBFLATSURF_CHECK_ARGUMENT(vector, "vector defining segment must not be trivial");

  self->normalize();
}

template <typename Surface>
const Point<Surface>& Segment<Surface>::start() const {
  throw std::logic_error("not implemented: Segment::start()");
}

template <typename Surface>
const Point<Surface>& Segment<Surface>::end() const {
  throw std::logic_error("not implemented: Segment::end()");
}

template <typename Surface>
HalfEdge Segment<Surface>::source() const {
  throw std::logic_error("not implemented: Segment::source()");
}

template <typename Surface>
HalfEdge Segment<Surface>::target() const {
  throw std::logic_error("not implemented: Segment::target()");
}

template <typename Surface>
const FlatTriangulation<typename Surface::Coordinate>& Segment<Surface>::surface() const {
  throw std::logic_error("not implemented: Segment::surface()");
}

template <typename Surface>
const Vector<typename Surface::Coordinate>& Segment<Surface>::vector() const {
  throw std::logic_error("not implemented: Segment::vector()");
}

template <typename Surface>
std::optional<SaddleConnection<Surface>> Segment<Surface>::saddleConnection() const {
  throw std::logic_error("not implemented: Segment::saddleConnection()");
}

template <typename Surface>
Segment<Surface> Segment<Surface>::operator-() const {
  throw std::logic_error("not implemented: Segment::operator-()");
}

template <typename Surface>
bool Segment<Surface>::operator==(const Segment<Surface>&) const {
  throw std::logic_error("not implemented: Segment::end()");
}

template <typename Surface>
std::ostream &operator<<(std::ostream &, const Segment<Surface> &) {
  throw std::logic_error("not implemented: Segment::end()");
}

template <typename Surface>
ImplementationOf<Segment<Surface>>::ImplementationOf(HalfEdge source, const Point<Surface>& start, HalfEdge target, const Point<Surface>& end, const Vector<T>& vector) : surface(start.surface()), source(source), start(start), target(target), end(end), vector(vector) {}

template <typename Surface>
void ImplementationOf<Segment<Surface>>::normalize() {
  const auto normalizeSourceAtVertex = [](const auto& surface, auto& source, const auto& vector) {
    while(true) {
      if (surface.inSector(source, vector))
        break;
      if (surface.fromHalfEdge(surface.nextAtVertex(source)).parallel(vector)) {
        source = surface.nextAtVertex(source);
        break;
      }

      source = surface.nextInFace(source);
    }
  };

  const auto normalizeSourceAtEdge = [](const auto& surface, auto& source, const auto& vector) {
    if (surface.fromHalfEdge(source).ccw(vector) == CCW::COUNTERCLOCKWISE || surface.fromHalfEdge(source).parallel(vector))
      return;

    source = -source;
  };

  if (start.vertex())
    normalizeSourceAtVertex(*surface, source, vector);
  else if (start.edge())
    normalizeSourceAtEdge(*surface, source, vector);
  else
    source = start.face();

  if (end.vertex())
    normalizeSourceAtVertex(*surface, target, -vector);
  else if (end.edge())
    normalizeSourceAtEdge(*surface, target, -vector);
  else
    target = end.face();
}

}

namespace std {

using namespace flatsurf;

template <typename Surface>
size_t hash<Segment<Surface>>::operator()(const Segment<Surface>& self) const {
  // We do not hash source & target because they are not unique for segments starting in the interior of a face.
  return ::flatsurf::hash_combine(self.start(), self.end(), self.vector());
}

}  // namespace std

// Instantiations of templates so implementations are generated for the linker
#include "util/instantiate.ipp"

LIBFLATSURF_INSTANTIATE_MANY_WRAPPED((LIBFLATSURF_INSTANTIATE_WITH_IMPLEMENTATION)(LIBFLATSURF_INSTANTIATE_HASH), Segment, LIBFLATSURF_FLAT_TRIANGULATION_TYPES)
