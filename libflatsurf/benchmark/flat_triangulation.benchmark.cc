/**********************************************************************
 *  This file is part of flatsurf.
 *
 *        Copyright (C) 2019-2020 Vincent Delecroix
 *        Copyright (C) 2019-2020 Julian Rüth
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

#include <benchmark/benchmark.h>

#include "../flatsurf/half_edge.hpp"
#include "../flatsurf/saddle_connection.hpp"
#include "../flatsurf/saddle_connections.hpp"
#include "../flatsurf/saddle_connections_iterator.hpp"
#include "../flatsurf/vertical.hpp"
#include "../test/surfaces.hpp"

using benchmark::State;

namespace flatsurf::benchmark {
using namespace flatsurf::test;

template <typename R2>
void FlatTriangulationFlip(State& state) {
  auto L = makeL<R2>();

  auto vertical = Vertical(*L, R2(1000000007, 1));

  for (auto _ : state) {
    for (auto e : L->halfEdges())
      if (vertical.large(e)) {
        L->flip(e);
        break;
      }
  }
}
BENCHMARK_TEMPLATE(FlatTriangulationFlip, Vector<long long>);
BENCHMARK_TEMPLATE(FlatTriangulationFlip, Vector<mpq_class>);
BENCHMARK_TEMPLATE(FlatTriangulationFlip, Vector<eantic::renf_elem_class>);
BENCHMARK_TEMPLATE(FlatTriangulationFlip, Vector<exactreal::Element<exactreal::IntegerRing>>);

template <typename R2>
void FlatTriangulationTrivialEquality(State& state) {
  auto L = makeL<R2>();

  auto connection = *std::begin(L->connections());

  for (auto _ : state)
    ::benchmark::DoNotOptimize(*L == connection.surface());
}
BENCHMARK_TEMPLATE(FlatTriangulationTrivialEquality, Vector<eantic::renf_elem_class>);

template <typename R2>
void FlatTriangulationEquality(State& state) {
  auto L = makeL<R2>();
  auto LL = L->clone();

  for (auto _ : state)
    ::benchmark::DoNotOptimize(*L == LL);
}
BENCHMARK_TEMPLATE(FlatTriangulationEquality, Vector<long long>);
BENCHMARK_TEMPLATE(FlatTriangulationEquality, Vector<mpq_class>);
BENCHMARK_TEMPLATE(FlatTriangulationEquality, Vector<eantic::renf_elem_class>);
BENCHMARK_TEMPLATE(FlatTriangulationEquality, Vector<exactreal::Element<exactreal::IntegerRing>>);

}  // namespace flatsurf::benchmark
