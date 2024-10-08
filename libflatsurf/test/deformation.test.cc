/**********************************************************************
 *  This file is part of flatsurf.
 *
 *        Copyright (C) 2021-2022 Julian Rüth
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

#include <exact-real/element.hpp>

#include "../flatsurf/deformation.hpp"
#include "../flatsurf/edge.hpp"
#include "../flatsurf/odd_half_edge_map.hpp"
#include "../flatsurf/path.hpp"
#include "../flatsurf/path_iterator.hpp"
#include "../flatsurf/tracked.hpp"
#include "external/catch2/single_include/catch2/catch.hpp"
#include "generators/deformation_generator.hpp"
#include "generators/half_edge_generator.hpp"
#include "generators/point_generator.hpp"
#include "generators/saddle_connection_generator.hpp"
#include "generators/surface_generator.hpp"

namespace flatsurf::test {

TEMPLATE_TEST_CASE("Deformations", "[Deformation]", (long long), (mpq_class), (renf_elem_class), (exactreal::Element<exactreal::IntegerRing>), (exactreal::Element<exactreal::NumberField>)) {
  using T = TestType;

  const auto surface = GENERATE_SURFACES(T);
  CAPTURE(surface);

  SECTION("Deformations can be Copied and Moved") {
    auto trivial = Deformation(*surface);
    auto copy = trivial;
    trivial = copy;

    auto move = std::move(trivial);
    trivial = std::move(move);
  }

  SECTION("Trivial Deformations Follow Flips") {
    auto deformed = surface->clone();
    auto track = Tracked(deformed, Deformation(*surface));

    for (auto edge : surface->edges()) {
      if (deformed.convex(edge.positive(), true))
        deformed.flip(edge.positive());
    }

    REQUIRE(track->domain() == *surface);
    REQUIRE(track->codomain() == deformed);

    deformed.delaunay();

    REQUIRE(track->domain() == *surface);
    REQUIRE(track->codomain() == deformed);
  }
}

TEMPLATE_TEST_CASE("Mapping Points Across Deformations", "[Deformation][operator()]", (long long), (mpq_class), (renf_elem_class), (exactreal::Element<exactreal::IntegerRing>), (exactreal::Element<exactreal::NumberField>)) {
  using T = TestType;
  using Surface = FlatTriangulation<T>;

  const auto surface = GENERATE_SURFACES(T);
  CAPTURE(surface);

  const auto deformation = GENERATE_COPY(deformations(surface));
  CAPTURE(deformation);

  SECTION("Map Random Points") {
    const auto face = GENERATE_COPY(halfEdges(surface));
    const auto point = GENERATE_COPY(points(surface, face));
    CAPTURE(point);

    const auto image = deformation(point);
    CAPTURE(image);

    const auto section = [&]() -> std::optional<Deformation<Surface>> {
      try {
        return deformation.section();
      } catch (const std::logic_error&) {
        return std::nullopt;
      }
    }();

    if (section) {
      const auto preimage = (*section)(image);
      CAPTURE(preimage);

      REQUIRE(preimage.surface() == point.surface());

      REQUIRE(preimage == point);
    }
  }
}

TEMPLATE_TEST_CASE("Mapping Paths Across Deformation", "[Deformation][operator()]", (long long), (mpq_class), (renf_elem_class), (exactreal::Element<exactreal::IntegerRing>), (exactreal::Element<exactreal::NumberField>)) {
  using T = TestType;
  using Surface = FlatTriangulation<T>;

  const auto surface = GENERATE_SURFACES(T);
  CAPTURE(surface);

  const auto deformation = GENERATE_COPY(deformations(surface));

  SECTION("Map Random Paths") {
    const auto saddleConnection = GENERATE_REF(saddleConnections(surface));
    CAPTURE(saddleConnection);

    const auto image = deformation(saddleConnection);
    REQUIRE(image);
    CAPTURE(*image);

    const auto section = [&]() -> std::optional<Deformation<Surface>> {
      try {
        return deformation.section();
      } catch (const std::logic_error&) {
        return std::nullopt;
      }
    }();

    if (section) {
      const auto preimage = (*section)(*image);
      REQUIRE(preimage);
      CAPTURE(*preimage);

      REQUIRE(!preimage->empty());

      REQUIRE(preimage->begin()->surface() == saddleConnection.surface());
      REQUIRE(preimage == saddleConnection);
    }
  }
}

TEMPLATE_TEST_CASE("Deform a Flat Triangulation", "[Deformation]", (long long), (mpz_class), (mpq_class), (renf_elem_class), (exactreal::Element<exactreal::IntegerRing>), (exactreal::Element<exactreal::RationalField>), (exactreal::Element<exactreal::NumberField>)) {
  using T = TestType;
  using R2 = Vector<T>;

  const auto surface = makeL<R2>();

  SECTION("Trivially deform an L") {
    const auto shift = OddHalfEdgeMap<R2>(*surface);
    REQUIRE(surface->operator+(shift).codomain() == *surface);
  }

  SECTION("Stretch an L") {
    auto shift = OddHalfEdgeMap<R2>(*surface);

    shift.set(HalfEdge(8), R2(0, 1));
    shift.set(HalfEdge(7), R2(0, 1));

    const auto shifted = surface->operator+(shift);
    CAPTURE(shifted);

    REQUIRE(shifted.codomain() != *surface);

    SECTION("Half Edges can be Mapped") {
      for (auto he : surface->halfEdges())
        REQUIRE(shifted(Path(SaddleConnection(*surface, he))).has_value());

      REQUIRE(shifted(SaddleConnection(*surface, HalfEdge(8)))->size() == 1);
      REQUIRE(shifted(SaddleConnection(*surface, HalfEdge(8)))->begin()->vector() == surface->fromHalfEdge(HalfEdge(8)) + R2(0, 1));

      REQUIRE(shifted(SaddleConnection(*surface, HalfEdge(7)))->size() == 1);
      REQUIRE(shifted(SaddleConnection(*surface, HalfEdge(7)))->begin()->vector() == surface->fromHalfEdge(HalfEdge(7)) + R2(0, 1));
    }

    SECTION("Saddle Connections can be Mapped") {
      const auto connection = GENERATE_REF(saddleConnections<T>(surface));

      const auto image = shifted(connection);
      REQUIRE(image.has_value());
    }
  }

  SECTION("Scale an L") {
    auto scaled = surface->applyMatrix(T(2), T(), T(), T(2));

    REQUIRE(scaled.codomain() != *surface);

    if constexpr (hasFractions<T>) {
      auto unscaled = scaled.codomain().applyMatrix(T(1) / 2, T(), T(), T(1) / 2);
      REQUIRE(unscaled.codomain() == *surface);
    }
  }
}

}  // namespace flatsurf::test
