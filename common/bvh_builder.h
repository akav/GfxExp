#pragma once

#include "common_shared.h"

namespace bvh {

template <uint32_t arity>
struct GeometryBVH {
    std::vector<shared::InternalNode_T<arity>> intNodes;
    std::vector<shared::PrimitiveReference> primRefs;
    std::vector<shared::TriangleStorage> triStorages;
};

struct Geometry {
    const uint8_t* vertices;
    uint32_t vertexStride;
    uint32_t numVertices;
    const uint8_t* triangles;
    uint32_t triangleStride;
    uint32_t numTriangles;
    Matrix4x4 preTransform;
};

struct GeometryBVHBuildConfig {
    float splittingBudget;
    float intNodeTravCost;
    float primIntersectCost;
    uint32_t minNumPrimsPerLeaf;
    uint32_t maxNumPrimsPerLeaf;
};

template <uint32_t arity>
void buildGeometryBVH(
    const Geometry* const geoms, const uint32_t numGeoms,
    const GeometryBVHBuildConfig &config, GeometryBVH<arity>* const bvh);

}
