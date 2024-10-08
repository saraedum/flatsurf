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

#ifndef LIBFLATSURF_IMPL_COMBINATORIAL_DEFORMATION_RELATION_HPP
#define LIBFLATSURF_IMPL_COMBINATORIAL_DEFORMATION_RELATION_HPP

#include <iosfwd>
#include <unordered_map>

#include "../../flatsurf/permutation.hpp"
#include "deformation_relation.hpp"

namespace flatsurf {

// A deformation between surfaces that are combinatorially equivalent, i.e.,
// their combinatorial structures are identical up to a change of labels.
template <typename Surface>
class CombinatorialDeformationRelation : public DeformationRelation<Surface> {
  using T = typename Surface::Coordinate;

 public:
  CombinatorialDeformationRelation(const Surface& domain, const Surface& codomain, std::unordered_map<HalfEdge, HalfEdge> mapping);
  CombinatorialDeformationRelation(const Surface& domain, const Surface& codomain, const Permutation<HalfEdge>& mapping);

  std::optional<Path<Surface>> operator()(const Path<Surface>&) const override;
  Point<Surface> operator()(const Point<Surface>&) const override;

  std::unique_ptr<DeformationRelation<Surface>> clone() const override;

  std::unique_ptr<DeformationRelation<Surface>> section() const override;

  bool trivial() const override;

  std::ostream& operator>>(std::ostream& os) const override;

  std::unordered_map<HalfEdge, HalfEdge> mapping;

  // Whether the deformation is trivial except for a relabeling of half edges.
  // If this is not the case, this deformation cannot be applied to saddle
  // connections and paths.
  bool relabeling;
};

}  // namespace flatsurf

#endif
