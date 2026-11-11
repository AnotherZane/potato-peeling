#include "internal/cabello/visibility.hpp"

#include <algorithm>
#include <cstddef>

#include <CGAL/Bbox_2.h>
#include <CGAL/intersections.h>

namespace potato_peeling::internal::cabello {
    // A static bounding-box hierarchy over P's edges. A query visits only edges whose
    // boxes overlap the segment's box and decides each with the exact do_intersect.
    struct VisibilityOracle::Impl {
        struct Node {
            CGAL::Bbox_2 box;
            std::size_t first, count;  // leaf: edges[first, first + count)
            std::size_t left, right;   // inner: child node indices (count == 0)
        };
        static constexpr std::size_t leaf_size = 4;

        std::vector<Seg> edges;
        std::vector<CGAL::Bbox_2> boxes;
        std::vector<Node> nodes;

        std::size_t build(const std::size_t first, const std::size_t count) {
            CGAL::Bbox_2 box = boxes[first];
            for (std::size_t i = first + 1; i < first + count; ++i) box += boxes[i];
            const std::size_t id = nodes.size();
            nodes.push_back({box, first, count, 0, 0});
            if (count <= leaf_size) return id;

            // Split at the median along the box's longer axis.
            const int axis = (box.xmax() - box.xmin() >= box.ymax() - box.ymin()) ? 0 : 1;
            const auto mid = [axis](const CGAL::Bbox_2 &b) {
                return axis == 0 ? b.xmin() + b.xmax() : b.ymin() + b.ymax();
            };
            std::vector<std::size_t> order(count);
            for (std::size_t i = 0; i < count; ++i) order[i] = first + i;
            const std::size_t half = count / 2;
            std::nth_element(
                order.begin(), order.begin() + half, order.end(),
                [&](std::size_t i, std::size_t j) { return mid(boxes[i]) < mid(boxes[j]); });
            std::vector<Seg> e(count);
            std::vector<CGAL::Bbox_2> bx(count);
            for (std::size_t i = 0; i < count; ++i) {
                e[i] = edges[order[i]];
                bx[i] = boxes[order[i]];
            }
            std::copy(e.begin(), e.end(), edges.begin() + first);
            std::copy(bx.begin(), bx.end(), boxes.begin() + first);

            const std::size_t left = build(first, half);
            const std::size_t right = build(first + half, count - half);
            nodes[id].count = 0;
            nodes[id].left = left;
            nodes[id].right = right;
            return id;
        }

        bool intersects(const Seg &s) const {
            const CGAL::Bbox_2 sb = s.bbox();
            std::vector<std::size_t> stack{0};
            while (!stack.empty()) {
                const Node &n = nodes[stack.back()];
                stack.pop_back();
                if (!CGAL::do_overlap(n.box, sb)) continue;
                if (n.count == 0) {
                    stack.push_back(n.left);
                    stack.push_back(n.right);
                    continue;
                }
                for (std::size_t i = n.first; i < n.first + n.count; ++i)
                    if (CGAL::do_overlap(boxes[i], sb) && CGAL::do_intersect(edges[i], s))
                        return true;
            }
            return false;
        }
    };

    VisibilityOracle::VisibilityOracle(const Poly &p) : poly_(p), impl_(std::make_unique<Impl>()) {
        for (auto e = p.edges_begin(); e != p.edges_end(); ++e) {
            impl_->edges.push_back(*e);
            impl_->boxes.push_back(e->bbox());
        }
        if (!impl_->edges.empty()) impl_->build(0, impl_->edges.size());
    }

    VisibilityOracle::VisibilityOracle(VisibilityOracle &&) noexcept = default;

    VisibilityOracle &VisibilityOracle::operator=(VisibilityOracle &&) noexcept = default;

    VisibilityOracle::~VisibilityOracle() = default;

    bool VisibilityOracle::inside(const Pt &q) const {
        // Strictly inside; on the boundary is not inside. Exact under EPICK for
        // double coordinates -- bounded_side is a predicate, not a construction.
        return poly_.bounded_side(q) == CGAL::ON_BOUNDED_SIDE;
    }

    bool VisibilityOracle::visible(const Pt &a, const Pt &b) const {
        if (!inside(a) || !inside(b)) return false;
        if (a == b) return true;

        // do_intersect only: no intersection point is ever constructed, so this
        // stays exact on double coordinates.
        return !impl_->intersects(Seg(a, b));
    }

    PointGraph visibility_graph(const VisibilityOracle &oracle, const std::vector<Pt> &points,
                                const StopFn &stop) {
        PointGraph g(points);
        // A row is |points| visibility tests, seconds at the paper's sample sizes,
        // so poll within rows too.
        std::size_t tests = 0;
        for (std::size_t i = 0; i < points.size(); ++i) {
            for (std::size_t j = i + 1; j < points.size(); ++j) {
                if (stop && tests++ % 1024 == 0 && stop()) return g;
                if (oracle.visible(points[i], points[j])) g.add_edge(i, j);
            }
        }
        return g;
    }

}  // namespace potato_peeling::internal::cabello
