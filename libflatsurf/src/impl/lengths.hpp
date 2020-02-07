/**********************************************************************
 *  This file is part of flatsurf.
 *
 *        Copyright (C) 2019 Julian Rüth
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

#ifndef LIBFLATSURF_IMPL_LENGTHS_HPP
#define LIBFLATSURF_IMPL_LENGTHS_HPP

#include <gmpxx.h>
#include <deque>
#include <iosfwd>
#include <memory>
#include <vector>

#include <intervalxt/label.hpp>

#include "../../flatsurf/edge.hpp"
#include "../../flatsurf/edge_map.hpp"
#include "../../flatsurf/saddle_connection.hpp"

#include "flow_decomposition_state.hpp"

namespace flatsurf {

template <typename Surface>
class Lengths {
  using Uncollapsed = FlatTriangulation<typename Surface::Coordinate>;

 public:
  Lengths(std::shared_ptr<const Vertical<Uncollapsed>>, const EdgeMap<typename Surface::SaddleConnection>&);

  void push(intervalxt::Label);
  void pop();
  void subtract(intervalxt::Label);
  intervalxt::Label subtractRepeated(intervalxt::Label);
  std::vector<mpq_class> coefficients(intervalxt::Label) const;
  int cmp(intervalxt::Label) const;
  int cmp(intervalxt::Label, intervalxt::Label) const;
  typename Surface::Coordinate get(intervalxt::Label) const;
  std::string render(intervalxt::Label) const;

  // TODO: This is a hack. We should pass this at construction time but
  // flatsurf::IntervalExchangeTransformation wants to exist independently of a
  // FlowDecomposition. Maybe there should be several flavours of Lengths?
  void registerDecomposition(std::shared_ptr<FlowDecompositionState<FlatTriangulation<typename Surface::Coordinate>>>);

  template <typename S>
  friend std::ostream& operator<<(std::ostream&, const Lengths<S>&);

 private:
  intervalxt::Label toLabel(Edge) const;
  Edge fromLabel(intervalxt::Label) const;

  typename Surface::Coordinate length(intervalxt::Label) const;
  typename Surface::Coordinate length() const;

  std::weak_ptr<FlowDecompositionState<FlatTriangulation<typename Surface::Coordinate>>> state;
  std::shared_ptr<const Vertical<Uncollapsed>> vertical;
  EdgeMap<typename Surface::SaddleConnection> lengths;

  std::deque<intervalxt::Label> stack;
  typename Surface::Coordinate sum;

  size_t degree;

  friend IntervalExchangeTransformation<Surface>;
};

}  // namespace flatsurf

#endif
