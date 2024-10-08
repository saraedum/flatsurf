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

#include "../flatsurf/equivalence_class.hpp"

#include <ostream>

#include "../flatsurf/deformation.hpp"
#include "impl/equivalence.impl.hpp"
#include "impl/equivalence_class.impl.hpp"
#include "impl/equivalence_class_code.hpp"

namespace flatsurf {

template <typename Surface>
EquivalenceClass<Surface>::EquivalenceClass(const Surface& surface, const Equivalence<Surface>& equivalence) :
  self(spimpl::make_impl<ImplementationOf<EquivalenceClass>>(surface, equivalence)) {}

template <typename Surface>
bool EquivalenceClass<Surface>::operator==(const EquivalenceClass<Surface>& other) const {
  return *self->code == *other.self->code && *self->equivalence == *other.self->equivalence;
}

template <typename Surface>
size_t EquivalenceClass<Surface>::automorphisms() const {
  return self->automorphisms;
}

template <typename Surface>
const Surface& EquivalenceClass<Surface>::representative() const {
  return self->surface;
}

template <typename Surface>
ImplementationOf<EquivalenceClass<Surface>>::ImplementationOf(const Surface& surface, const Equivalence<Surface>& equivalence) :
  equivalence(equivalence) {
  auto [code, normalization, deformations] = equivalence.self->code(surface);

  this->surface = *normalization;
  this->code = std::move(code);
  this->automorphisms = deformations.size();
}

template <typename Surface>
size_t ImplementationOf<EquivalenceClass<Surface>>::hash(const EquivalenceClass<Surface>& self) {
  return self.self->code->hash();
}

template <typename Surface>
std::ostream& operator<<(std::ostream& os, const EquivalenceClass<Surface>& clazz) {
  return os << "[" << clazz.representative() << "] identified by (" << *clazz.self->code << ")";
}

}  // namespace flatsurf

namespace std {

using namespace flatsurf;

template <typename Surface>
size_t hash<EquivalenceClass<Surface>>::operator()(const EquivalenceClass<Surface>& self) const {
  return ImplementationOf<EquivalenceClass<Surface>>::hash(self);
}

}  // namespace std

// Instantiations of templates so implementations are generated for the linker
#include "util/instantiate.ipp"

LIBFLATSURF_INSTANTIATE_MANY_WRAPPED((LIBFLATSURF_INSTANTIATE_WITH_IMPLEMENTATION)(LIBFLATSURF_INSTANTIATE_HASH), EquivalenceClass, LIBFLATSURF_FLAT_TRIANGULATION_TYPES)
