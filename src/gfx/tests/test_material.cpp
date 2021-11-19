#include "gfx/material.hpp"
#include <catch2/catch_all.hpp>
#include <gfx/material.hpp>

using namespace gfx;

TEST_CASE("Material hash", "[material]") {
	Material matA, matB;
	CHECK(matA.getHash() == matB.getHash());
	CHECK(matA.getHash().low != 0);
	CHECK(matA.getHash().high != 0);

	matB.modify([](MaterialData &mat) {
		mat.flags = MaterialStaticFlags::Lit;
		mat.vectorParameters["baseColor"] = float4(0, 1, 0, 1);
	});
	CHECK(matA.getHash() != matB.getHash());

	Material matC = matB;
	CHECK(matC.getHash() == matB.getHash());

	auto f = [](MaterialData &mat) {
		MaterialTextureSlot &textureSlot = mat.textureSlots["tex"];
		textureSlot.texCoordIndex = 1;
	};
	matC.modify(f);
	CHECK(matC.getHash() != matB.getHash());

	matB.modify(f);
	CHECK(matC.getHash() == matB.getHash());
}