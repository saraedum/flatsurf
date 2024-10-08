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
#ifndef LIBFLATSURF_EQUIVALENCE_CLASS_CODE_HPP
#define LIBFLATSURF_EQUIVALENCE_CLASS_CODE_HPP

#include <boost/operators.hpp>
#include <functional>

#include "../../flatsurf/forward.hpp"

namespace flatsurf {

// A hashable code for the equivalence class of a surface. Two surfaces are in
// the same equivalence class iff they have the same code.
// Instances of this are created by ImplementationOf<Equivalence>::code() and
// consumed by EquivalenceClass. It is otherwise an implementation detail and
// not exposed to the user.
struct EquivalenceClassCode : boost::equality_comparable<EquivalenceClassCode> {
  virtual ~EquivalenceClassCode();

  // Return a hash value for this code.
  virtual size_t hash() const = 0;

  // Return whether two codes describe the same equivalence class.
  virtual bool equal(const EquivalenceClassCode&) const = 0;

  // Return a printable representation of this code.
  virtual std::string toString() const = 0;

  friend bool operator==(const EquivalenceClassCode& lhs, const EquivalenceClassCode& rhs);

  friend std::ostream& operator<<(std::ostream&, const EquivalenceClassCode& code);
};

}  // namespace flatsurf

template <>
struct std::hash<::flatsurf::EquivalenceClassCode> {
  size_t operator()(const ::flatsurf::EquivalenceClassCode&) const;
};

#endif
