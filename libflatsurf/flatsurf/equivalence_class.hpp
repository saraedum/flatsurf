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
#ifndef LIBFLATSURF_EQUIVALENCE_CLASS_HPP
#define LIBFLATSURF_EQUIVALENCE_CLASS_HPP

#include <boost/operators.hpp>

#include "copyable.hpp"

namespace flatsurf {

// An equivalence class of surfaces modulo a notion of equality.
template <typename Surface>
class EquivalenceClass : boost::equality_comparable<EquivalenceClass<Surface>> {
  static_assert(std::is_same_v<Surface, std::decay_t<Surface>>, "type parameter must not have modifiers such as const");

 public:
  EquivalenceClass(const Surface &, const Equivalence<Surface> &);

  // Return the number of automorphisms of the elements of this equivalence class.
  size_t automorphisms() const;

  // Return a (possibly canonical) representative of this equivalence class.
  const Surface &representative() const;

  // Return whether two equivalence classes are equal.
  bool operator==(const EquivalenceClass<Surface> &) const;

  template <typename S>
  friend std::ostream &operator<<(std::ostream &, const EquivalenceClass<S> &);

 private:
  Copyable<EquivalenceClass> self;

  friend ImplementationOf<EquivalenceClass>;
};

}  // namespace flatsurf

template <typename Surface>
struct std::hash<::flatsurf::EquivalenceClass<Surface>> {
  size_t operator()(const ::flatsurf::EquivalenceClass<Surface> &) const;
};

#endif
