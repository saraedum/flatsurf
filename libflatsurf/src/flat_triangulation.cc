/**********************************************************************
 *  This file is part of flatsurf.
 *
 *        Copyright (C) 2019-2021 Julian Rüth
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

#include "../flatsurf/flat_triangulation.hpp"

#include <exact-real/arb.hpp>
#include <exact-real/integer_ring.hpp>
#include <exact-real/number_field.hpp>
#include <exact-real/rational_field.hpp>
#include <exact-real/yap/arb.hpp>
#include <functional>
#include <iosfwd>
#include <map>
#include <ostream>
#include <type_traits>
#include <vector>

#include "../flatsurf/bound.hpp"
#include "../flatsurf/ccw.hpp"
#include "../flatsurf/chain.hpp"
#include "../flatsurf/deformation.hpp"
#include "../flatsurf/delaunay.hpp"
#include "../flatsurf/edge.hpp"
#include "../flatsurf/edge_set.hpp"
#include "../flatsurf/half_edge.hpp"
#include "../flatsurf/half_edge_set.hpp"
#include "../flatsurf/isomorphism.hpp"
#include "../flatsurf/odd_half_edge_map.hpp"
#include "../flatsurf/orientation.hpp"
#include "../flatsurf/path.hpp"
#include "../flatsurf/path_iterator.hpp"
#include "../flatsurf/saddle_connection.hpp"
#include "../flatsurf/saddle_connections.hpp"
#include "../flatsurf/saddle_connections_iterator.hpp"
#include "../flatsurf/vector.hpp"
#include "../flatsurf/vertical.hpp"
#include "external/rx-ranges/include/rx/ranges.hpp"
#include "impl/approximation.hpp"
#include "impl/deformation.impl.hpp"
#include "impl/flat_triangulation.impl.hpp"
#include "impl/flat_triangulation_combinatorial.impl.hpp"
#include "impl/flip_deformation_relation.hpp"
#include "impl/linear_deformation_relation.hpp"
#include "impl/generic_retriangulation_deformation_relation.hpp"
#include "impl/retriangulation_deformation_relation.hpp"
#include "impl/shift_deformation_relation.hpp"
#include "impl/slit_deformation_relation.hpp"
#include "impl/quadratic_polynomial.hpp"
#include "impl/insert_marked_deformation_relation.hpp"
#include "util/assert.ipp"

namespace flatsurf {

using std::begin;
using std::end;

template <typename T>
Deformation<FlatTriangulation<T>> FlatTriangulation<T>::operator+(const OddHalfEdgeMap<Vector<T>> &shift) const {
  // Half edges that collapse at the end of the shift.
  EdgeSet collapsing;

  // Records that the half edge e needs to be flipped at a time t in (0, 1]
  // that is given by a solution to det(t) = a*t^2 + b*t + c = 0.
  struct Flip {
    HalfEdge flip;
    QuadraticPolynomial<T> det;
  };

  std::optional<Flip> flip;

  for (auto vertex : this->vertices()) {
    const auto outgoing = this->atVertex(vertex);

    // The x, y coordinates of the half edge he
    const auto x = [&](const HalfEdge he) { return fromHalfEdge(he).x(); };
    const auto y = [&](const HalfEdge he) { return fromHalfEdge(he).y(); };
    // The x, y shifts of the half edge he at time t = 1
    const auto u = [&](const HalfEdge he) { return shift.get(he).x(); };
    const auto v = [&](const HalfEdge he) { return shift.get(he).y(); };

    // One reason why the area of a triangle is zero for a time t in [0, 1] is
    // that two singularities were shifted into each other. We can make sense
    // of this when it happens at time t=1 by collapsing triangles.
    for (size_t i = 0; i < outgoing.size(); i++) {
      const auto he = outgoing.at(i);

      if (fromHalfEdge(he).ccw(shift.get(he)) == CCW::COLLINEAR) {
        switch (fromHalfEdge(he).orientation(fromHalfEdge(he) + shift.get(he))) {
          case ORIENTATION::SAME:
            // The critical time t is not in [0, 1]
            break;
          case ORIENTATION::OPPOSITE:
            throw std::invalid_argument("shift must not collapse half edges for a time t in (0, 1)");
          case ORIENTATION::ORTHOGONAL:
            collapsing.insert(he);
        }
      }
    }

    // The more common reason why the area of a triangle is zero is that a
    // singularity is shifted onto the interior of an edge. When this happens
    // we can flip that edge just before to make sure that our triangulation
    // remains valid at all times.
    for (size_t i = 0; i < outgoing.size(); i++) {
      const auto he = outgoing.at(i);
      const auto he_ = outgoing.at((i + 1) % outgoing.size());

      // The determinant of the vectors spanned by the edges he and he_ at time
      // t is given by a*t^2 - b*t + c.
      const auto det = QuadraticPolynomial<T>(
          u(he) * v(he_) - u(he_) * v(he),
          u(he) * y(he_) - u(he_) * y(he) + x(he) * v(he_) - x(he_) * v(he),
          x(he) * y(he_) - x(he_) * y(he));

      LIBFLATSURF_ASSERT(det(T()) > 0, "Original surface " << *this << " already had a triangle with non-positive area before applying any shift to it.");

      // If the determinant has a zero for any t in [0, 1], the area of a
      // triangle vanishes or becomes negative.
      // We handle the easiest case first: the area remains positive for all
      // times t in [0, 1].
      if (det.positive())
        continue;

      // We can now assume that the determinant is zero for some t in (0, 1].
      // We need to flip a half edge of this triangle if it has a vertex on its
      // interior at that critical time t.

      // But first we exclude the case that
      // the vertex ends up on the boundary of the half edge, i.e., a half edge
      // collapses.
      if (collapsing.contains(he) || collapsing.contains(he_))
        continue;

      // Determine whether our vertex moves onto the half edge opposite to it,
      // i.e., the one following he in this triangle.
      const auto vertex_hits_interior = [&]() {
        for (long prec = exactreal::ARB_PRECISION_FAST;; prec *= 2) {
          const auto t = det.root(prec);
          const auto arb = Approximation<T>::arb;
          LIBFLATSURF_ASSERT(t, "determinant " << det << " must have a root in [0, 1]");
          const auto et = Vector<exactreal::Arb>(
              (arb(fromHalfEdge(he).x(), prec) + *t * arb(shift.get(he).x(), prec))(prec),
              (arb(fromHalfEdge(he).y(), prec) + *t * arb(shift.get(he).y(), prec))(prec));
          const auto e_t = Vector<exactreal::Arb>(
              (arb(fromHalfEdge(he_).x(), prec) + *t * arb(shift.get(he_).x(), prec))(prec),
              (arb(fromHalfEdge(he_).y(), prec) + *t * arb(shift.get(he_).y(), prec))(prec));

          const auto orientation = et.orientation(e_t);

          if (orientation) {
            switch (*orientation) {
              case ORIENTATION::ORTHOGONAL:
                LIBFLATSURF_UNREACHABLE("vectors cannot be orthogonal when their determinant is vanishing");
              case ORIENTATION::SAME:
                // The half edges he and he_ meet but the vertex at their source
                // does not end up on the interior of the half edge opposite to
                // it. We can ignore this case as another vertex will take care
                // of this vanishing triangle.
                return false;
              case ORIENTATION::OPPOSITE:
                // The two edges attached to this vertex point in opposite
                // directions at time t so this vertex ends up on the interior
                // of the opposite edge.
                return true;
            }
          }
        }
      };

      if (!vertex_hits_interior())
        // The half edge following e does not need to be flipped.
        continue;

      const Flip proposed{this->nextInFace(he), det};

      // Record that a half edge needs to be flipped at time t. We'll later
      // actually flip the one that needs to be flipped first and recurse.
      if (!flip || proposed.det < flip->det)
        flip = proposed;
    }
  }

  if (flip) {
    // We want to flip the half edge that we found needs to be flipped first.
    // However, just flipping that edge right now might lead to infinite loops
    // where the same edges get flipped again and again without making any
    // progress. So instead we get a bit closer to the critical time and
    // perform the flip just then.
    // Note that this also solves the problem that the flip might not actually
    // be possibly as it might lead to a non-convex triangulation since
    // eventually, when we are close enough to the critical time, the flip will
    // be valid.
    // Note that this leads to quite some coefficient blow-up along the way;
    // every flip introduces a factor of two in the denominators. These
    // coefficients go away in the final surface. It would likely be more
    // efficient not to move before the flip if this still makes the critical
    // time t increase.
    const auto t = *flip->det.root(exactreal::ARB_PRECISION_FAST);

    for (auto s = mpq_class(1, 2);; s /= 2) {
      const auto lt = exactreal::Arb(s, exactreal::ARB_PRECISION_FAST) < t;
      if (lt && *lt) {
        const OddHalfEdgeMap<Vector<T>> partial(*this, [&](const HalfEdge he) { return shift.get(he) / s.get_den(); });
        auto deformation = (*this + partial);
        auto closer = deformation.codomain().clone();

        const Tracked<OddHalfEdgeMap<Vector<T>>> remaining(closer, OddHalfEdgeMap<Vector<T>>(closer, [&](const HalfEdge he) { return shift.get(he) - partial.get(he); }), ImplementationOf<FlatTriangulation>::updateAfterFlip);

        if (convex(flip->flip, true)) {
          closer.flip(flip->flip);
          deformation = ImplementationOf<Deformation<FlatTriangulation>>::make(std::make_unique<FlipDeformationRelation<FlatTriangulation>>(deformation.codomain(), closer, flip->flip)) * deformation;
        }

        return (deformation.codomain() + remaining) * deformation;
      }
    }
  } else {
    // We don't need to flip, so we perform shifts of half edges on a copy of
    // the surface's vector structure and collapse on a copy of the
    // combinatorial structure.
    auto combinatorial = static_cast<const FlatTriangulationCombinatorial &>(*this).clone();

    // When trivial half edges are collapsed, the other edges in a triangle
    // are identified. We keep track of such identifications here, mapping a
    // half edge to all its preimages under this identification.
    Tracked<HalfEdgeMap<std::unordered_set<HalfEdge>>> halfEdges(combinatorial, HalfEdgeMap<std::unordered_set<HalfEdge>>(combinatorial, [](const HalfEdge he) { return std::unordered_set{he}; }),
        Tracked<HalfEdgeMap<std::unordered_set<HalfEdge>>>::defaultFlip,
        [](HalfEdgeMap<std::unordered_set<HalfEdge>>& self, const FlatTriangulationCombinatorial& surface, Edge e) {
          const auto copy = [&](const HalfEdge from, const HalfEdge to) {
            for (const auto he : self[from])
              self[to].insert(he);
          };

          const auto equate = [&](const HalfEdge a, const HalfEdge b) {
            copy(a, b);
            copy(b, a);
          };

          for (const auto collapse : {e.positive(), e.negative()}) {
            equate(surface.nextInFace(collapse), -surface.previousInFace(collapse));
            equate(-surface.nextInFace(collapse), surface.previousInFace(collapse));
          }
        });

    Tracked<OddHalfEdgeMap<Vector<T>>> vectors(combinatorial, OddHalfEdgeMap<Vector<T>>(combinatorial, [&](const HalfEdge he) { return fromHalfEdge(he) + shift.get(he); }),
        Tracked<OddHalfEdgeMap<Vector<T>>>::defaultFlip,
        [](OddHalfEdgeMap<Vector<T>> &vectors, const FlatTriangulationCombinatorial &, Edge e) {
          LIBFLATSURF_ASSERT(!vectors.get(e.positive()), "can only collapse half edges that have become trivial");
        });

    Tracked<EdgeSet> collapsing_(combinatorial, collapsing,
        Tracked<EdgeSet>::defaultFlip,
        [](EdgeSet &self, const FlatTriangulationCombinatorial &, Edge e) {
          LIBFLATSURF_ASSERT(self.contains(e), "can only collapse edges that have been found to collapse at t=1");
        });

    while (!collapsing_->empty())
      combinatorial.collapse(begin(static_cast<const EdgeSet &>(collapsing_))->positive());

    const auto codomain = FlatTriangulation<T>(
        std::move(combinatorial),
        [&](const HalfEdge he) { return vectors->get(he); });

    // TODO
    return ImplementationOf<Deformation<FlatTriangulation>>::make(std::make_unique<ShiftDeformationRelation<FlatTriangulation>>(
      *this,
      codomain/*,
      [&]() {
        using Path = flatsurf::Path<FlatTriangulation>;

        std::vector<std::pair<Path, Path>> relation;

        for (const auto lhs : this->halfEdges()) {
          relation.push_back(std::pair{Path{SaddleConnection<FlatTriangulation>{*this, lhs}}, [&]() {
            for (const auto& image : codomain.halfEdges()) {
              for (const auto& preimage : halfEdges->operator[](image)) {
                if (preimage == lhs) {
                  LIBFLATSURF_ASSERT(vectors->get(image) == fromHalfEdge(lhs) + shift.get(lhs), "Half edge " << lhs << " should have been shifted from " << fromHalfEdge(lhs) << " to " << fromHalfEdge(lhs) + shift.get(lhs) << " but instead it became " << image << " which is " << vectors->get(image));
                  return Path{SaddleConnection<FlatTriangulation>(codomain, image)};
                }
              }
            }
            // This half edge has been collapsed.
            return Path{};
          }()});
        }

        return relation;
    }()*/));
  }
}

template <typename T>
Deformation<FlatTriangulation<T>> FlatTriangulation<T>::eliminateMarkedPoints() const {
  std::optional<HalfEdge> collapse;

  for (const auto &vertex : this->vertices()) {
    if (angle(vertex) == 1) {

      // TODO: Why is this necessary? It is in the 1,2,3 example, by what's the underlying reason that this helps? And does it help in general?
      if (collapse && vertex.outgoing().size() > Vertex::source(*collapse, *this).outgoing().size())
        continue;

      for (const auto &outgoing : this->atVertex(vertex)) {
        const auto neighbour = Vertex::target(outgoing, *this);
        if (neighbour != vertex) {
          if (collapse && (fromHalfEdge(*collapse) * fromHalfEdge(*collapse)) < fromHalfEdge(outgoing) * fromHalfEdge(outgoing))
            continue;
          collapse = outgoing;
        }
      }
    }
  }

  if (!collapse)
    return Deformation(*this);

  const auto marked = Vertex::source(*collapse, *this);

  const auto delta = OddHalfEdgeMap<Vector<T>>(*this, [&](const HalfEdge he) {
    if (Vertex::source(he, *this) == marked && Vertex::target(he, *this) == marked)
      return Vector<T>();
    if (Vertex::source(he, *this) == marked)
      return -fromHalfEdge(*collapse);
    if (Vertex::target(he, *this) == marked)
      return fromHalfEdge(*collapse);

    return Vector<T>();
  });

  const auto shift = *this + delta;

  LIBFLATSURF_ASSERT(shift.codomain().vertices().size() < this->vertices().size(), "the numbers of vertices is reduced in each step but " << *this << " was simplified to " << shift.codomain());

  // The shift deformation moves around the saddle connections. We need to
  // rebuild the deformation here so we can pull back saddle connections in the
  // codomain to the equivalent saddle connection(s) in the domain.
  using Path = flatsurf::Path<FlatTriangulation>;
  std::vector<std::pair<Path, Path>> relation;

  // The mapping provided by shift is correct on half edges that have not moved.
  // The half edges that have moved have no image under the mapping anymore
  // since one of their endpoints has disappeared. However, they might show up
  // in the preimage if two collinear half edges met at the marked point.
  // TODO: There is not really a point in doing all this for all half edges
  // (signed).  Also we could probably postpone this to when somebody actually
  // asks for this functionality.
  for (auto preimage : this->halfEdges()) {
    if (Vertex::source(preimage, *this) == marked || Vertex::target(preimage, *this) == marked)
      continue;

    const auto image = shift(SaddleConnection<FlatTriangulation>(*this, preimage));

    LIBFLATSURF_ASSERT(image, "Edge " << preimage << " is not next to the marked point we just eliminated but it can not be found after the shift " << shift << " by " << delta);

    LIBFLATSURF_ASSERT(!image->empty(), "Edge " << preimage << " has not been collapsed so it should be non-trivial in the image.");

    // TODO: What did we mean to check for here?
    // LIBFLATSURF_ASSERT(this->angle(Vertex::source(preimage, *this)) == shift.codomain().angle(Vertex::source(image->at(0).source(), shift.codomain())), "TODO");

    relation.emplace_back(std::pair{std::vector{SaddleConnection<FlatTriangulation>(*this, preimage)}, image.value()});
  }

  // Reconstructing the preimage of half edges is much more subtle. The idea
  // is that the source vertex of a half edge already existed in the preimage.
  // We find some saddle connection starting at this vertex and represent it
  // in the domain and codomain of `shift`. Once we have this saddle
  // connection in both, we can reconstruct any other saddle connection
  // relative to that.
  // TODO: This is not particular to eliminateMarkedPoints() and could be pulled into a Deformation.
  for (const auto image_ : shift.codomain().halfEdges()) {
    const auto image = SaddleConnection<FlatTriangulation>(shift.codomain(), image_);

    const auto preimage = [&]() {
      for (const auto& rel : relation) {
        if (Vertex::source(rel.second.begin()->source(), shift.codomain()) != Vertex::source(image.source(), shift.codomain()))
          continue;

        // We found a saddle connection leaving at the same vertex in direction basepreimage/baseimage.
        const auto& basepreimage = *rel.first.begin();
        const auto& baseimage =  *rel.second.begin();

        LIBFLATSURF_ASSERT(basepreimage.vector().ccw(baseimage.vector()) == CCW::COLLINEAR, "shift did not preserve vector directions on the saddle connections that it did not move.");

        // To reconstruct `image` in shift.domain(), we need to determine the
        // angle from `baseimage` to `image` and then reproduce the same angle in
        // shift.domain() from `basepreimage`.
        int angle = baseimage.angle(image);

        // Perform `angle` full turns of basepreimage.
        HalfEdge source = basepreimage.source();
        Vector<T> turned = basepreimage.vector();

        LIBFLATSURF_ASSERT(this->angle(Vertex::source(source, *this)) == shift.codomain().angle(Vertex::source(baseimage.source(), shift.codomain())), "Total angle at vertex must not change.");

        for (int turn = 0; turn < angle; turn++) {
          while(turned.ccw(fromHalfEdge(source)) != CCW::COUNTERCLOCKWISE)
            source = this->nextAtVertex(source);
          while(turned.ccw(fromHalfEdge(source)) != CCW::CLOCKWISE)
            source = this->nextAtVertex(source);
          while(turned.ccw(fromHalfEdge(source)) != CCW::COUNTERCLOCKWISE)
            source = this->nextAtVertex(source);
          source = this->previousAtVertex(source);
        }

        // Now `basepreimage.vector()` is in the sector next to `source` which
        // has been turned `angle` times from its original value. Now we
        // perform the turn <2π from `baseimage` to `image` in the preimage.
        while(turned.ccw(image) != CCW::COLLINEAR || turned.orientation(image) != ORIENTATION::SAME) {
          if (turned.ccw(image) == CCW::COUNTERCLOCKWISE) {
            if (fromHalfEdge(this->nextAtVertex(source)).ccw(image) == CCW::CLOCKWISE) {
              // The image is in the same sector as turned, i.e., next to source.
              turned = image.vector();
              break;
            }
          }
          source = this->nextAtVertex(source);
          turned = fromHalfEdge(source);
        }

        auto preimage = std::vector{SaddleConnection<FlatTriangulation>::inSector(*this, source, Vertical<FlatTriangulation>(*this, turned))};
        LIBFLATSURF_ASSERT(this->angle(Vertex::source(preimage.at(0).source(), *this)) == shift.codomain().angle(Vertex::source(image.source(), shift.codomain())), "TODO");

        if (preimage.at(0).vector() != image.vector()) {
          LIBFLATSURF_ASSERT((image.vector() - preimage.at(0).vector()).orientation(image.vector()) == ORIENTATION::SAME, "Partial saddle connection " << preimage.at(0) << " was longer than " << image << " but it can only be shorter.");

          preimage.push_back(SaddleConnection<FlatTriangulation>::inPlane(*this, this->nextAtVertex(preimage.at(0).target()), image.vector() - preimage.at(0).vector()));
        }
        
        return preimage;
      }

      LIBFLATSURF_UNREACHABLE("Could not pull back " << image << " from " << shift.codomain() << " to " << *this << " after shift of " << delta << " since all its half edges are connected to the collapsed vertex.");
    }();

    relation.emplace_back(std::pair{preimage, std::vector{image}});
  }

  // Eliminate all the other marked points.
  auto elimination = shift.codomain().eliminateMarkedPoints() * ImplementationOf<Deformation<FlatTriangulation>>::make(std::make_unique<GenericRetriangulationDeformationRelation<FlatTriangulation>>(*this, shift.codomain(), relation));

  /* TODO
  // ;
  LIBFLATSURF_ASSERTIONS([&]() {
    const auto section = elimination.section();
    for (const auto& image : elimination.codomain().halfEdges()) {
      const auto preimage = section(std::vector{SaddleConnection<FlatTriangulation>(elimination.codomain(), image)});
      LIBFLATSURF_ASSERT(preimage, "Half edge " << image << " in surface " << elimination.codomain() << " which was created from " << *this << " by removing a marked point could not be pulled back to the original surface.");
      LIBFLATSURF_ASSERT(preimage->size(), "Half edge " << image << " in surface " << elimination.codomain() << " which was created from " << *this << " by removing a marked point pulled back to a trivial connection.");

      // TODO
      throw std::logic_error("not implemented: assertions");
      LIBFLATSURF_ASSERT(elimination.codomain().angle(Vertex::source(image, elimination.codomain())) == this->angle(Vertex::source(preimage->at(0).source(), *this)), "Total angle at vertex changed. " << image << " in " << elimination.codomain() << " pulls back to [" << preimage->at(0) << ", ...] in " << *this << " but the the total turns at the source of the former is " << elimination.codomain().angle(Vertex::source(image, elimination.codomain())) << " and the total turn at the source of the latter is " << this->angle(Vertex::source(preimage->at(0).source(), *this)));

      Vector<T> vector;
      for (const auto& segment : *preimage)
        vector += segment;

      LIBFLATSURF_ASSERT(vector == elimination.codomain().fromHalfEdge(image), "Half edge " << image << " in surface " << elimination.codomain() << " pulled back to " << vector << " but it should have pulled back to " << elimination.codomain().fromHalfEdge(image));
    }
  });
  */
  
  return elimination;
}

template <typename T>
Vector<T> FlatTriangulation<T>::shortest() const {
  const auto edges = this->edges();
  Edge shortest = *std::min_element(begin(edges), end(edges), [&](const auto &a, const auto &b) {
    const Vector x = fromHalfEdge(a.positive());
    const Vector y = fromHalfEdge(b.positive());
    return x * x < y * y;
  });
  return fromHalfEdge(shortest.positive());
}

template <typename T>
Vector<T> FlatTriangulation<T>::shortest(const Vector<T> &direction) const {
  const auto edges = this->edges();
  Edge shortest = *std::min_element(begin(edges), end(edges), [&](const auto &a, const auto &b) {
    const Vector<T> av = fromHalfEdge(a.positive());
    const Vector<T> bv = fromHalfEdge(b.positive());

    const auto alen = av * direction;
    const auto blen = bv * direction;

    if (!alen) return false;
    if (!blen) return true;

    const auto abs = [](const auto &x) { return x < 0 ? -x : x; };

    return abs(alen) < abs(blen);
  });
  return fromHalfEdge(shortest.positive());
}

template <typename T>
const Vector<T> &FlatTriangulation<T>::fromHalfEdge(const HalfEdge e) const {
  return self->vectors->get(e);
}

template <typename T>
const flatsurf::Vector<exactreal::Arb> &FlatTriangulation<T>::fromHalfEdgeApproximate(HalfEdge e) const {
  return self->approximations->get(e);
}

template <typename T>
FlatTriangulation<T>::FlatTriangulation() noexcept :
  FlatTriangulation(FlatTriangulationCombinatorial(), std::vector<Vector<T>>{}) {}

template <typename T>
FlatTriangulation<T>::FlatTriangulation(FlatTriangulationCombinatorial &&combinatorial, const std::vector<Vector<T>> &vectors) :
  FlatTriangulation(std::move(combinatorial), [&](const HalfEdge he) {
    Edge e = he;
    if (he == e.positive())
      return vectors.at(e.index());
    else
      return -vectors.at(e.index());
  }) {
  LIBFLATSURF_CHECK_ARGUMENT(vectors.size() == this->edges().size(), "there must be exactly one vector for each edge");
}

template <typename T>
FlatTriangulation<T>::FlatTriangulation(FlatTriangulationCombinatorial &&combinatorial, const std::function<Vector<T>(HalfEdge)> &vectors) :
  FlatTriangulationCombinatorics<FlatTriangulation>(ProtectedConstructor{}, std::make_shared<ImplementationOf<FlatTriangulation<T>>>(std::move(combinatorial), vectors)) {
  self->check();
}

template <typename T>
FlatTriangulation<T> FlatTriangulation<T>::clone() const {
  return FlatTriangulation(static_cast<const FlatTriangulationCombinatorial &>(*this).clone(), [&](HalfEdge e) { return fromHalfEdge(e); });
}

template <typename T>
Deformation<FlatTriangulation<T>> FlatTriangulation<T>::slit(HalfEdge slit) const {
  const auto codomain = FlatTriangulation(
      static_cast<const FlatTriangulationCombinatorial &>(*this).slit(slit),
      [&](HalfEdge e) {
        HalfEdge newEdge = HalfEdge(static_cast<int>(this->halfEdges().size()) / 2 + 1);
        if (e == newEdge) return fromHalfEdge(slit);
        if (e == -newEdge) return -fromHalfEdge(slit);
        return fromHalfEdge(e);
      });

  return ImplementationOf<Deformation<FlatTriangulation>>::make(std::make_unique<SlitDeformationRelation<FlatTriangulation>>(*this, codomain));
}

template <typename T>
Deformation<FlatTriangulation<T>> FlatTriangulation<T>::insertAt(HalfEdge &nextTo, const Vector<T> &vector) const {
  LIBFLATSURF_CHECK_ARGUMENT(inSector(nextTo, vector), "vector must be contained in the sector next to the half edge");

  Deformation<FlatTriangulation<T>> deformation{*this};

  auto check_orientation = [&](const Vector<T> &saddle_connection) {
    auto orient = (saddle_connection - vector).orientation(vector);
    LIBFLATSURF_CHECK_ARGUMENT(orient != ORIENTATION::OPPOSITE, "cannot insert half edge that crosses over an existing vertex");
    if (orient == ORIENTATION::ORTHOGONAL) {
      // It is a bit unclear what to do if the new edge should end at a
      // vertex, in particular if it is collinear with an existing half
      // edge (after fliping.)
      throw std::logic_error("not implemented: insertion of half edges that end at an existing vertex");
    }
  };

  // Search for half edges that vector would be crossing and flip them.
  // We should replace all this with a simple call to operator+, see #183.
  [&]() {
    while (true) {
      if (deformation.codomain().fromHalfEdge(nextTo).ccw(vector) == CCW::COLLINEAR) {
        check_orientation(deformation.codomain().fromHalfEdge(nextTo));
        // vector is on an existing HalfEdge but does not cross a vertex.
        return;
      }
      assert(deformation.codomain().fromHalfEdge(nextTo).ccw(vector) == CCW::COUNTERCLOCKWISE);

      // The half edge that vector is potentially crossing
      const HalfEdge crossing = deformation.codomain().nextInFace(nextTo);
      // The base point of crossing half edge
      const Vector<T> base = deformation.codomain().fromHalfEdge(nextTo);

      // Check whether vector is actually crossing crossing. It would be enough
      // to check whether this is != CLOCKWISE. However, we do not allow vector
      // to end on an edge other than nextTo. So we perform one additional
      // flip in that case so vector is actually inside of a face.
      if (deformation.codomain().fromHalfEdge(crossing).ccw(vector - base) == CCW::COUNTERCLOCKWISE)
        return;

      std::function<void(HalfEdge)> flip = [&](HalfEdge e) {
        assert(e != nextTo && e != -nextTo && e != deformation.codomain().nextAtVertex(nextTo) && e != -deformation.codomain().nextAtVertex(nextTo));

        auto canFlip = [&](HalfEdge g) {
          return e != nextTo &&
            e != -nextTo &&
            e != deformation.codomain().nextAtVertex(nextTo) &&
            e != -deformation.codomain().nextAtVertex(nextTo) &&
            deformation.codomain().fromHalfEdge(deformation.codomain().previousAtVertex(g)).ccw(deformation.codomain().fromHalfEdge(deformation.codomain().nextAtVertex(g))) == CCW::COUNTERCLOCKWISE &&
            deformation.codomain().fromHalfEdge(deformation.codomain().previousAtVertex(-g)).ccw(deformation.codomain().fromHalfEdge(deformation.codomain().nextAtVertex(-g))) == CCW::COUNTERCLOCKWISE;
        };

        while (!canFlip(e)) {
          // f is blocked by a forward triangle on top of it so we flip its top edge.
          if (vector.ccw(deformation.codomain().fromHalfEdge(deformation.codomain().previousAtVertex(e))) != CCW::COUNTERCLOCKWISE) {
            flip(-deformation.codomain().nextAtVertex(-e));
            continue;
          } else {
            assert(vector.ccw(deformation.codomain().fromHalfEdge(deformation.codomain().nextAtVertex(-e))) != CCW::CLOCKWISE);
            flip(deformation.codomain().previousAtVertex(e));
            continue;
          }
          
        }

        {
          auto codomain = deformation.codomain().clone();
          codomain.flip(e);
          deformation = ImplementationOf<Deformation<FlatTriangulation>>::make(std::make_unique<FlipDeformationRelation<FlatTriangulation>>(deformation.codomain(), codomain, e)) * deformation;
        }
      };

      // vector crosses crossing, so flip it and replace nextTo if vector is then not next to nextTo anymore.
      flip(crossing);

      assert(deformation.codomain().fromHalfEdge(nextTo).ccw(vector) == CCW::COUNTERCLOCKWISE);
      while (deformation.codomain().fromHalfEdge(deformation.codomain().nextAtVertex(nextTo)).ccw(vector) != CCW::CLOCKWISE)
        nextTo = deformation.codomain().nextAtVertex(nextTo);
    }
  }();

  auto symmetric = [](HalfEdge x, HalfEdge e, const Vector<T> &v) {
    assert(x == e || x == -e);
    return x == e ? v : -v;
  };

  if (deformation.codomain().fromHalfEdge(nextTo).ccw(vector) != CCW::COLLINEAR) {
    // After the flips we did, v is now completely inside a face.
    assert(deformation.codomain().fromHalfEdge(nextTo).ccw(vector) == CCW::COUNTERCLOCKWISE);

    auto combinatorial = static_cast<FlatTriangulationCombinatorial &>(deformation.codomain().clone()).insertAt(nextTo);

    // The newly inserted half edges.
    HalfEdge a = -combinatorial.nextAtVertex(nextTo);
    HalfEdge b = combinatorial.nextAtVertex(a);
    HalfEdge c = combinatorial.nextAtVertex(b);

    const auto codomain = FlatTriangulation(combinatorial.clone(), [&](const HalfEdge e) {
      if (Edge(e) == a) return symmetric(e, a, -vector);
      if (Edge(e) == b) return symmetric(e, b, deformation.codomain().fromHalfEdge(nextTo) - vector);
      if (Edge(e) == c) return symmetric(e, c, deformation.codomain().fromHalfEdge(deformation.codomain().nextAtVertex(nextTo)) - vector);
      return deformation.codomain().fromHalfEdge(e);
    });

    return ImplementationOf<Deformation<FlatTriangulation>>::make(std::make_unique<InsertMarkedDeformationRelation<FlatTriangulation>>(deformation.codomain(), codomain, Vertex::source(a, codomain))) * deformation;
  } else {
    // After the flips we did, vector is collinear with the half edge nextTo (but shorter.)

    // Combinatorially insert a half edge a next to nextTo. We then
    // flip the edge nextTo to get combinatorially the right picture.
    // Another way to think of this: Insert our half edge -a next to nextTo and
    // then flip nextTo. Then move the inserted vertex to where it must be so
    // that -a corresponds to vector.
    auto combinatorial = static_cast<FlatTriangulationCombinatorial &>(deformation.codomain().clone()).insertAt(nextTo);
    auto a = -combinatorial.nextAtVertex(nextTo);
    combinatorial.flip(nextTo);

    // Let a, b, c, d be the half edges going out of the inserted vertex.
    const auto b = combinatorial.nextAtVertex(a);
    const auto c = combinatorial.nextAtVertex(b);
    const auto d = combinatorial.nextAtVertex(c);

    // The combinatorics are correct now, but we still need to patch up the
    // vectors, namely the four half edges meeting at the new vertex.
    const auto codomain = FlatTriangulation(combinatorial.clone(), [&](const HalfEdge e) {
      if (Edge(e) == a) return symmetric(e, -a, vector);
      if (Edge(e) == b) return symmetric(e, b, deformation.codomain().fromHalfEdge(deformation.codomain().previousAtVertex(nextTo)) - vector);
      if (Edge(e) == c) return symmetric(e, c, deformation.codomain().fromHalfEdge(nextTo) - vector);
      if (Edge(e) == d) return symmetric(e, d, deformation.codomain().fromHalfEdge(deformation.codomain().nextAtVertex(nextTo)) - vector);
      return deformation.codomain().fromHalfEdge(e);
    });

    nextTo = deformation.codomain().previousAtVertex(nextTo);

    return ImplementationOf<Deformation<FlatTriangulation>>::make(std::make_unique<InsertMarkedDeformationRelation<FlatTriangulation>>(deformation.codomain(), codomain, Vertex::source(a, codomain), nextTo, -a, c)) * deformation;
  }
}

template <typename T>
void FlatTriangulation<T>::delaunay() {
  bool isDelaunay;
  do {
    isDelaunay = true;
    for (auto edge : this->edges()) {
      if (delaunay(edge) == DELAUNAY::NON_DELAUNAY) {
        isDelaunay = false;
        this->flip(edge.positive());
      }
    }
  } while (!isDelaunay);
}

template <typename T>
DELAUNAY FlatTriangulation<T>::delaunay(const Edge edge) const {
  // We could eventually use Vector::insideCircumcircle() so vectors can
  // provide optimized implementations of this. However, at the moment, it
  // does not seem worth it.

  // We use the condition described in Wikipedia (whether a certain
  // determinant is positive.) Using the notation there, the face attached to
  // this half edge is the triangle (a, b, c), and the face attached to the
  // reversed half edge is (a, c, d). We use a coordinate system where
  // d=(0,0).
  const auto ca = this->fromHalfEdge(edge.positive());
  const auto cb = this->fromHalfEdge(this->nextAtVertex(edge.positive()));
  const auto dc = this->fromHalfEdge(-this->nextInFace(edge.negative()));

  const auto a = dc + ca;
  const auto b = dc + cb;
  const auto c = dc;

  const auto det = [](const auto &x00, const auto &x01, const auto &x02, const auto &x10, const auto &x11, const auto &x12, const auto &x20, const auto &x21, const auto &x22) -> T {
    return x00 * (x11 * x22 - x12 * x21) - x10 * (x01 * x22 - x21 * x02) + x20 * (x01 * x12 - x11 * x02);
  };

  const auto del = det(a.x(), a.y(), a.x() * a.x() + a.y() * a.y(), b.x(), b.y(), b.x() * b.x() + b.y() * b.y(), c.x(), c.y(), c.x() * c.x() + c.y() * c.y());

  if (del < 0)
    return DELAUNAY::DELAUNAY;
  else if (del == 0)
    return DELAUNAY::AMBIGUOUS;
  else
    return DELAUNAY::NON_DELAUNAY;
}

template <typename T>
T FlatTriangulation<T>::area() const {
  T area = T();
  for (auto e : this->halfEdges()) {
    if (this->boundary(e)) continue;

    // Do not count every triangle three times.
    if (e.index() > this->nextInFace(e).index()) continue;
    if (e.index() > this->previousInFace(e).index()) continue;

    area += Vector<T>::area({fromHalfEdge(e), fromHalfEdge(this->nextInFace(e)), fromHalfEdge(this->nextInFace(this->nextInFace(e)))});
  }
  return area;
}

template <typename T>
FlatTriangulation<T> FlatTriangulation<T>::scale(const mpz_class &scalar) const {
  return FlatTriangulation(static_cast<const FlatTriangulationCombinatorial &>(*this).clone(), [&](HalfEdge e) {
    return scalar * fromHalfEdge(e);
  });
}

template <typename T>
bool FlatTriangulation<T>::convex(HalfEdge e, bool strict) const {
  if (strict)
    return fromHalfEdge(this->previousAtVertex(e)).ccw(fromHalfEdge(this->nextAtVertex(e))) == CCW::COUNTERCLOCKWISE &&
           fromHalfEdge(this->previousAtVertex(-e)).ccw(fromHalfEdge(this->nextAtVertex(-e))) == CCW::COUNTERCLOCKWISE;
  else
    return fromHalfEdge(this->previousAtVertex(e)).ccw(fromHalfEdge(this->nextAtVertex(e))) != CCW::CLOCKWISE &&
           fromHalfEdge(this->previousAtVertex(-e)).ccw(fromHalfEdge(this->nextAtVertex(-e))) != CCW::CLOCKWISE;
}

template <typename T>
bool FlatTriangulation<T>::inSector(const HalfEdge sector, const Vector<T> &vector) const {
  return fromHalfEdge(sector).ccw(vector) != CCW::CLOCKWISE && fromHalfEdge(this->nextAtVertex(sector)).ccw(vector) == CCW::CLOCKWISE;
}

template <typename T>
bool FlatTriangulation<T>::inSector(const HalfEdge sector, const Vertical<FlatTriangulation<T>> &vector) const {
  return inSector(sector, vector.vertical());
}

template <typename T>
SaddleConnections<FlatTriangulation<T>> FlatTriangulation<T>::connections() const {
  return SaddleConnections(*this);
}

template <typename T>
bool FlatTriangulation<T>::operator==(const FlatTriangulation<T> &rhs) const {
  if (this == &rhs)
    return true;

  if (static_cast<const FlatTriangulationCombinatorial &>(*this) != static_cast<const FlatTriangulationCombinatorial &>(rhs))
    return false;
  for (auto &edge : this->halfEdges()) {
    if (this->self->vectors->get(edge) != rhs.self->vectors->get(edge))
      return false;
  }
  return true;
}

template <typename T>
int FlatTriangulation<T>::angle(const Vertex &vertex) const {
  int angle = 0;

  const HalfEdge first = *begin(this->atVertex(vertex));
  HalfEdge current = first;
  do {
    const HalfEdge next = this->nextAtVertex(current);

    if (fromHalfEdge(current).x() >= 0 && fromHalfEdge(next).x() < 0)
      angle++;

    current = next;
  } while (current != first);

  LIBFLATSURF_ASSERT(angle >= 1, "Total angle at vertex cannot be less than 2π");

  return angle;
}

template <typename T>
std::optional<Deformation<FlatTriangulation<T>>> FlatTriangulation<T>::isomorphism(const FlatTriangulation<T> &other, ISOMORPHISM kind, std::function<bool(const T &, const T &, const T &, const T &)> filterMatrix, std::function<bool(HalfEdge, HalfEdge)> filterHalfEdgeMap) const {
  if (this->hasBoundary() != other.hasBoundary())
    return std::nullopt;

  if (this->halfEdges().size() != other.halfEdges().size())
    return std::nullopt;

  if (this->hasBoundary())
    throw std::logic_error("not implemented: isomorphism() not implemented for surfaces with boundary");

  const auto ignore = [&](HalfEdge he) {
    return kind == ISOMORPHISM::FACES ? false : this->delaunay(he.edge()) == DELAUNAY::AMBIGUOUS;
  };
  const auto ignoreImage = [&](HalfEdge he) {
    return kind == ISOMORPHISM::FACES ? false : this->delaunay(he.edge()) == DELAUNAY::AMBIGUOUS;
  };

  if (kind == ISOMORPHISM::DELAUNAY_CELLS) {
    LIBFLATSURF_ASSERT(this->edges() | rx::all_of([&](const auto e) { return this->delaunay(e) != DELAUNAY::NON_DELAUNAY; }), "source surface not Delaunay triangulated");
    LIBFLATSURF_ASSERT(other.edges() | rx::all_of([&](const auto e) { return other.delaunay(e) != DELAUNAY::NON_DELAUNAY; }), "target surface not Delaunay triangulated");
  }

  // We pick a fixed half edge of this surfaces and try to map it to every
  // other half edge in the other surface. Taking into account another half
  // edge in the same face, we get a single possible 2×2 transformation matrix.
  // (Or rather two possible matrices, if we allow reflections.)
  HalfEdge preimage = [&]() {
    for (HalfEdge he : this->halfEdges())
      if (!ignore(he))
        return he;
    throw std::logic_error("cannot detect isomorphism in surface without Delaunay cells");
  }();

  for (auto image : other.halfEdges()) {
    if (ignoreImage(image))
      continue;

    for (int sgn : {1, -1}) {
      const auto nextInCell = [&](auto e) {
        e = -e;
        do {
          e = this->previousAtVertex(e);
        } while (ignore(e));
        return e;
      };

      const auto nextInImageCell = [&](auto e) {
        e = -e;
        if (sgn == 1) {
          do {
            e = other.previousAtVertex(e);
          } while (ignoreImage(e));
        } else {
          do {
            e = other.nextAtVertex(e);
          } while (ignoreImage(e));
        }
        return e;
      };

      auto v = this->fromHalfEdge(preimage);
      auto w = this->fromHalfEdge(nextInCell(preimage));
      auto v_ = other.fromHalfEdge(image);
      auto w_ = other.fromHalfEdge(nextInImageCell(image));

      // To determine the 2×2 matrix (a b c d) that sends v to v_ and w to w_ note that:
      // ┌ v.x v.y   0   0 ┐ ┌ a ┐   ┌ v_.x ┐
      // | w.x w.y   0   0 | | b |   | w_.x |
      // |   0   0 v.x v.y | | c | = | v_.y |
      // └   0   0 w.x w.y ┘ └ d ┘   └ w_.y ┘
      // Hence, we can determine (a b) and (c d) by solving a 2×2 system for each.
      const T denominator = v.x() * w.y() - v.y() * w.x();
      T a = v_.x() * w.y() - v.y() * w_.x();
      T b = v.x() * w_.x() - v_.x() * w.x();
      T c = v_.y() * w.y() - v.y() * w_.y();
      T d = v.x() * w_.y() - v_.y() * w.x();

      if (!LinearDeformationRelation<FlatTriangulation>::truediv(a, denominator)) continue;
      if (!LinearDeformationRelation<FlatTriangulation>::truediv(b, denominator)) continue;
      if (!LinearDeformationRelation<FlatTriangulation>::truediv(c, denominator)) continue;
      if (!LinearDeformationRelation<FlatTriangulation>::truediv(d, denominator)) continue;

      if ((sgn == 1) != (a * d > b * c))
        continue;

      if (!filterMatrix(a, b, c, d))
        continue;

      // The isomorphism of half edges can now be determined by starting with
      // the half edges of this face/cell and depth-first searching through all
      // the adjacent faces/cells until we find a contradiction.
      auto isomorphism = HalfEdgeMap<HalfEdge>(*this);

      const std::function<bool(HalfEdge, HalfEdge)> match = [&](const HalfEdge from, const HalfEdge to) {
        if (isomorphism[from] != HalfEdge())
          return isomorphism[from] == to;

        if (!filterHalfEdgeMap(from, to))
          return false;

        if (a * this->fromHalfEdge(from).x() + b * this->fromHalfEdge(from).y() != other.fromHalfEdge(to).x())
          return false;

        if (c * this->fromHalfEdge(from).x() + d * this->fromHalfEdge(from).y() != other.fromHalfEdge(to).y())
          return false;

        isomorphism[from] = to;

        // We found that this half edge is mapped in a consistent way. Now
        // check its negative.
        if (!match(-from, -to))
          return false;

        // And check that every other edge in its face/cell maps consistently
        if (!match(nextInCell(from), nextInImageCell(to)))
          return false;

        return true;
      };

      if (match(preimage, image)) {
        auto linear = ImplementationOf<Deformation<FlatTriangulation>>::make(std::make_unique<LinearDeformationRelation<FlatTriangulation>>(
          *this,
          FlatTriangulation(
            // TODO: Should there be an operator~ on FlatTriangulationCombinatorial?
            FlatTriangulationCombinatorial(
              [&]() {
              auto faces = this->faces();
              if (sgn == -1) {
                for (auto& face : faces) {
                  std::swap(std::get<0>(face), std::get<1>(face));
                  std::get<0>(face) = -std::get<0>(face);
                  std::get<1>(face) = -std::get<1>(face);
                  std::get<2>(face) = -std::get<2>(face);
                }
              }
              return faces;
            }()),
            [&](HalfEdge he) {
              return Vector<T>(
                fromHalfEdge(he).x() * a + fromHalfEdge(he).y() * b,
                fromHalfEdge(he).x() * c + fromHalfEdge(he).y() * d);
          }),
          a, b, c, d));

        std::vector<std::pair<Path<FlatTriangulation>, Path<FlatTriangulation>>> relation;
        for (auto he: this->halfEdges()) {
          if (isomorphism[he] == HalfEdge{})
            continue;

          relation.push_back({
            SaddleConnection<FlatTriangulation>(linear.codomain(), he),
            SaddleConnection<FlatTriangulation>(other, isomorphism[he])});
        }
 
        return ImplementationOf<Deformation<FlatTriangulation>>::make(std::make_unique<GenericRetriangulationDeformationRelation<FlatTriangulation>>(linear.codomain(), other, relation)) * linear;
      }
    }
  }

  return std::nullopt;
}

template <typename T>
ImplementationOf<FlatTriangulation<T>>::ImplementationOf(FlatTriangulationCombinatorial &&combinatorial, const std::function<Vector<T>(HalfEdge)> &vectors) :
  ImplementationOf<FlatTriangulationCombinatorial>(
      ImplementationOf<FlatTriangulationCombinatorial>::self(combinatorial)->vertices,
      combinatorial.halfEdges() | rx::filter([&](HalfEdge he) { return combinatorial.boundary(he); }) | rx::to_vector()),
  vectors([&]() {
    // We keep track of the vectors attached to the half edges in a Tracked<>
    // object. To construct such an object, we need the surface it is tracking.
    // However, that surface is the surface we are in the process of building.
    // In particular it is not referenced by shared pointer yet. Therefore, we
    // now build a combinatorial triangulation from the data we are building
    // and wrap it in a shared pointer that does *not* free its memory when it
    // goes out of scope.
    auto self = from_this(std::shared_ptr<ImplementationOf>(this, [](auto *) {}));
    auto ret = Tracked<OddHalfEdgeMap<Vector<T>>>(
        self,
        OddHalfEdgeMap<Vector<T>>(self, vectors),
        ImplementationOf::updateAfterFlip);
    // The shared pointer we used to build the Tracked is not going to remain
    // valid so we assert that noone else is holding on to it because it won't
    // work for other use cases than Tracked<>.
    LIBFLATSURF_ASSERT(self.self.state.use_count() == 1, "Something is holding to an short lived shared pointer to a surface. This shared pointer is not actually valid and should not be used outside of Tracked<>.");
    return ret;
  }()),
  approximations([&]() {
    // We keep track of the vectors attached to the half edges in a Tracked<>
    // object. To construct such an object, we need the surface it is tracking.
    // However, that surface is the surface we are in the process of building.
    // In particular it is not referenced by shared pointer yet. Therefore, we
    // now build a combinatorial triangulation from the data we are building
    // and wrap it in a shared pointer that does *not* free its memory when it
    // goes out of scope.
    auto self = from_this(std::shared_ptr<ImplementationOf>(this, [](auto *) {}));
    auto ret = Tracked<OddHalfEdgeMap<Vector<exactreal::Arb>>>(
        self,
        OddHalfEdgeMap<Vector<exactreal::Arb>>(
            self,
            [&](const HalfEdge e) {
              return static_cast<flatsurf::Vector<exactreal::Arb>>(this->vectors->get(e));
            }),
        ImplementationOf::updateApproximationAfterFlip);
    // The shared pointer we used to build the Tracked is not going to remain
    // valid so we assert that noone else is holding on to it because it won't
    // work for other use cases than Tracked<>.
    LIBFLATSURF_ASSERT(self.self.state.use_count() == 1, "Something is holding to an short lived shared pointer to a surface. This shared pointer is not actually valid and should not be used outside of Tracked<>.");
    return ret;
  }()) {}

template <typename T>
void ImplementationOf<FlatTriangulation<T>>::updateAfterFlip(OddHalfEdgeMap<Vector<T>> &vectors, const FlatTriangulationCombinatorial &parent, HalfEdge flip) {
  vectors.set(flip, vectors.get(-parent.nextInFace(flip)) + vectors.get(-parent.previousInFace(flip)));
}

template <typename T>
void ImplementationOf<FlatTriangulation<T>>::updateApproximationAfterFlip(OddHalfEdgeMap<flatsurf::Vector<exactreal::Arb>> &vectors, const FlatTriangulationCombinatorial &combinatorial, HalfEdge flip) {
  const auto &surface = reinterpret_cast<const FlatTriangulation<T> &>(combinatorial);
  vectors.set(flip, static_cast<flatsurf::Vector<exactreal::Arb>>(surface.fromHalfEdge(-surface.nextInFace(flip)) + surface.fromHalfEdge(-surface.previousInFace(flip))));
}

template <typename T>
void ImplementationOf<FlatTriangulation<T>>::flip(HalfEdge e) {
  const auto self = from_this();

  LIBFLATSURF_CHECK_ARGUMENT(self.convex(e, true), "cannot flip this edge as a resulting face would not be strictly convex");

  ImplementationOf<FlatTriangulationCombinatorial>::flip(e);

  check();
}

template <typename T>
void ImplementationOf<FlatTriangulation<T>>::check() {
  const auto self = from_this();

  // check that faces are closed
  for (auto edge : self.halfEdges()) {
    if (self.boundary(edge)) continue;
    auto zero = self.fromHalfEdge(edge);
    LIBFLATSURF_CHECK_ARGUMENT(zero, "edges must not be trivial but " << edge << " is zero in " << self);
    edge = self.nextInFace(edge);
    zero += self.fromHalfEdge(edge);
    edge = self.nextInFace(edge);
    zero += self.fromHalfEdge(edge);
    LIBFLATSURF_CHECK_ARGUMENT(!zero, "face at " << edge << " is not closed in " << self);
  }
  // check that faces are oriented correctly
  for (auto edge : self.halfEdges()) {
    if (self.boundary(edge)) continue;
    auto next = self.nextInFace(edge);
    LIBFLATSURF_CHECK_ARGUMENT(self.fromHalfEdge(edge).ccw(self.fromHalfEdge(next)) != CCW::COLLINEAR, "face at " << edge << " has vanishing area in " << self);
    LIBFLATSURF_CHECK_ARGUMENT(self.fromHalfEdge(edge).ccw(self.fromHalfEdge(next)) == CCW::COUNTERCLOCKWISE, "face at " << edge << " is not oriented correctly in " << self);
  }
}

template <typename T>
std::ostream &operator<<(std::ostream &os, const FlatTriangulation<T> &self) {
  return os << static_cast<const FlatTriangulationCombinatorial &>(self)
            << " with vectors " << self.self->vectors;
}

}  // namespace flatsurf

// Instantiations of templates so implementations are generated for the linker
#include "util/instantiate.ipp"

LIBFLATSURF_INSTANTIATE_MANY_WRAPPED((LIBFLATSURF_INSTANTIATE_WITH_IMPLEMENTATION), FlatTriangulation, LIBFLATSURF_REAL_TYPES)
