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

#ifndef LIBFLATSURF_COMBINATORIAL_EQUIVALENCE_HPP
#define LIBFLATSURF_COMBINATORIAL_EQUIVALENCE_HPP

#include <optional>

#include "../../flatsurf/half_edge.hpp"
#include "equivalence_class_code.hpp"
#include "equivalence.impl.hpp"

namespace flatsurf {

template <typename Surface>
struct CombinatorialEquivalenceWalker;

template <typename Surface>
struct CombinatorialEquivalence : ImplementationOf<Equivalence<Surface>> {
  using T = typename Surface::Coordinate;
  using Predicate = std::function<bool(const Surface&, Edge)>;

  CombinatorialEquivalence(bool oriented, Predicate predicate);

  std::unique_ptr<EquivalenceClassCode> code(const Surface&) const override;
  std::vector<Deformation<Surface>> automorphisms(const Surface&) const override;
  Deformation<Surface> normalize(const Surface&) const override;
  bool equal(const ImplementationOf<Equivalence<Surface>>&) const override;
  std::string toString() const override;

  template <typename S>
  friend std::ostream& operator<<(std::ostream&, const CombinatorialEquivalence<S>&);

 private:
  bool oriented;
  Predicate predicate;
};

}

#endif