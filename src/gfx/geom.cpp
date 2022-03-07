// The MIT License
//
// Copyright © 2010-2021 three.js authors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "geom.hpp"
#include <algorithm>

namespace gfx {
namespace geom {

std::vector<MeshVertexAttribute> VertexPNT::getAttributes() {
  std::vector<MeshVertexAttribute> attribs;
  attribs.emplace_back("position", 3, VertexAttributeType::Float32);
  attribs.emplace_back("normal", 3, VertexAttributeType::Float32);
  attribs.emplace_back("texCoord0", 2, VertexAttributeType::Float32);
  return attribs;
}

// https://github.com/mrdoob/three.js/blob/master/src/geometries/SphereGeometry.js#L87
void SphereGenerator::generate() {
  reset();

  size_t widthSegments = std::max<size_t>(3, this->widthSegments);
  size_t heightSegments = std::max<size_t>(2, this->heightSegments);

  auto thetaEnd = std::min(thetaStart + thetaLength, pi);

  auto index = 0;
  std::vector<std::vector<index_t>> grid;

  for (size_t iy = 0; iy <= heightSegments; iy++) {
    std::vector<index_t> verticesRow;

    auto v = float(iy) / heightSegments;

    // special case for the poles

    auto uOffset = 0;

    if (iy == 0 && thetaStart == 0) {
      uOffset = 0.5 / widthSegments;
    } else if (iy == heightSegments && thetaEnd == pi) {
      uOffset = -0.5 / widthSegments;
    }

    for (size_t ix = 0; ix <= widthSegments; ix++) {
      auto u = float(ix) / widthSegments;

      VertexPNT &vertex = vertices.emplace_back();

      float3 position;
      position.x = -radius * std::cos(phiStart + u * phiLength) * std::sin(thetaStart + v * thetaLength);
      position.y = radius * std::cos(thetaStart + v * thetaLength);
      position.z = radius * std::sin(phiStart + u * phiLength) * std::sin(thetaStart + v * thetaLength);
      vertex.setPosition(position);

      // normal
      vertex.setNormal(linalg::normalize(position));

      vertex.setTexCoord(float2(u + uOffset, 1 - v));

      verticesRow.push_back(index++);
    }

    grid.push_back(verticesRow);
  }

  for (size_t iy = 0; iy < heightSegments; iy++) {
    for (size_t ix = 0; ix < widthSegments; ix++) {
      auto a = grid[iy][ix + 1];
      auto b = grid[iy][ix];
      auto c = grid[iy + 1][ix];
      auto d = grid[iy + 1][ix + 1];

      if (iy != 0 || thetaStart > 0) {
        indices.push_back(a);
        indices.push_back(b);
        indices.push_back(d);
      }

      if (iy != heightSegments - 1 || thetaEnd < pi) {
        indices.push_back(b);
        indices.push_back(c);
        indices.push_back(d);
      }
    }
  }
}

// https://github.com/mrdoob/three.js/blob/master/src/geometries/PlaneGeometry.js
void PlaneGenerator::generate() {
  reset();

  auto width_half = width / 2.0f;
  auto height_half = height / 2.0f;

  auto gridX = widthSegments;
  auto gridY = heightSegments;

  auto gridX1 = gridX + 1;
  auto gridY1 = gridY + 1;

  auto segment_width = width / gridX;
  auto segment_height = height / gridY;

  for (size_t iy = 0; iy < gridY1; iy++) {
    auto y = float(iy) * segment_height - height_half;

    for (size_t ix = 0; ix < gridX1; ix++) {
      auto x = float(ix) * segment_width - width_half;

      VertexPNT &vertex = vertices.emplace_back();
      vertex.setPosition(float3(x, -y, 0));
      vertex.setNormal(float3(0, 0, 1));
      vertex.setTexCoord(float2(float(ix) / gridX, 1 - (float(iy) / gridY)));
    }
  }

  for (size_t iy = 0; iy < gridY; iy++) {
    for (size_t ix = 0; ix < gridX; ix++) {
      auto a = ix + gridX1 * iy;
      auto b = ix + gridX1 * (iy + 1);
      auto c = (ix + 1) + gridX1 * (iy + 1);
      auto d = (ix + 1) + gridX1 * iy;

      indices.push_back(a);
      indices.push_back(b);
      indices.push_back(d);
      indices.push_back(b);
      indices.push_back(c);
      indices.push_back(d);
    }
  }
}

} // namespace geom
} // namespace gfx
