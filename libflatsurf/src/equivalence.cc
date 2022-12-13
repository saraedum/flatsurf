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

#include "../flatsurf/equivalence.hpp"
#include "../flatsurf/edge.hpp"
#include "../flatsurf/vector.hpp"
#include "impl/combinatorial_equivalence.hpp"
#include "impl/linear_equivalence.hpp"

namespace flatsurf {

template <typename Surface>
Equivalence<Surface> Equivalence<Surface>::combinatorial(bool oriented, std::function<bool(const Surface&, Edge)> predicate) {
  return Equivalence(PrivateConstructor{}, std::make_shared<CombinatorialEquivalence<Surface>>(oriented, predicate));
}

template <typename Surface>
Equivalence<Surface> Equivalence<Surface>::unlabeled(std::function<bool(const Surface&, Edge)> predicate) {
  static const auto normalization = std::function([](const Surface&, HalfEdge, HalfEdge) -> std::tuple<T, T, T, T> {
    return {T(1), T(), T(), T(1)};
  });
  using GROUP = typename LinearEquivalence<Surface>::GROUP;

  return Equivalence(PrivateConstructor{}, std::make_shared<LinearEquivalence<Surface>>(true, GROUP::TRIVIAL, predicate));
}

template <typename Surface>
Equivalence<Surface> Equivalence<Surface>::orthogonal(bool oriented, std::function<bool(const Surface&, Edge)> predicate) {
  using GROUP = typename LinearEquivalence<Surface>::GROUP;

  return Equivalence(PrivateConstructor{}, std::make_shared<LinearEquivalence<Surface>>(oriented, GROUP::O, predicate));
}

template <typename Surface>
Equivalence<Surface> Equivalence<Surface>::areaPreserving(bool oriented, std::function<bool(const Surface&, Edge)> predicate) {
  using GROUP = typename LinearEquivalence<Surface>::GROUP;

  return Equivalence(PrivateConstructor{}, std::make_shared<LinearEquivalence<Surface>>(oriented, GROUP::SL, predicate));
}

template <typename Surface>
Equivalence<Surface> Equivalence<Surface>::linear(bool oriented, std::function<Matrix(const Surface&, HalfEdge, HalfEdge)> normalization, std::function<bool(const Surface&, Edge)> predicate) {
  using Normalization = typename LinearEquivalence<Surface>::Normalization;
  using GROUP = typename LinearEquivalence<Surface>::GROUP;

  return Equivalence(PrivateConstructor{}, std::make_shared<LinearEquivalence<Surface>>(oriented, normalization == nullptr ? static_cast<Normalization>(GROUP::GL) : normalization, predicate));
}

template <typename Surface>
bool Equivalence<Surface>::isomorphic(const Surface&, const Surface&) const {
  // TODO: Implement me.
  throw std::logic_error("not implemented: isomorphic()");
}

template <typename Surface>
std::vector<Deformation<Surface>> Equivalence<Surface>::isomorphisms(const Surface&, const Surface&) const {
  // TODO: Implement me.
  throw std::logic_error("not implemented: isomorphisms()");
}

template <typename Surface>
bool Equivalence<Surface>::operator==(const Equivalence<Surface>& other) const {
  return self->equal(*other.self);
}

template <typename Surface>
ImplementationOf<Equivalence<Surface>>::~ImplementationOf() {}

template <typename Surface>
std::vector<Deformation<Surface>> ImplementationOf<Equivalence<Surface>>::isomorphisms(const Surface&, const Surface&) const {
  // TODO: Implement me.
  throw std::logic_error("not implemented: isomorphisms()");
}

template <typename Surface>
std::ostream& operator<<(std::ostream& os, const Equivalence<Surface>& equivalence) {
  return os << equivalence.self->toString();
}

}

// Instantiations of templates so implementations are generated for the linker
#include "util/instantiate.ipp"

LIBFLATSURF_INSTANTIATE_MANY_WRAPPED((LIBFLATSURF_INSTANTIATE_WITH_IMPLEMENTATION), Equivalence, LIBFLATSURF_FLAT_TRIANGULATION_TYPES)