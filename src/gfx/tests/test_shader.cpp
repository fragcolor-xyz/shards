#include <catch2/catch_all.hpp>
#include <cctype>
#include <gfx/shader/blocks.hpp>
#include <gfx/shader/evaluator.hpp>

using namespace gfx;
using namespace gfx::shader;
using String = std::string;

bool compareNoWhitespace(const String &a, const String &b) {
	using Iterator = String::const_iterator;
	using StringView = std::string_view;

	auto isWhitespace = [](const Iterator &it) { return *it == '\n' || *it == '\t' || *it == '\r' || *it == ' '; };
	auto isSymbol = [](const Iterator &it) { return !std::isalnum(*it); };

	// Skip whitespace, returns true on end of string
	auto skipWhitespace = [=](Iterator &it, const Iterator &end) {
		do {
			if (isWhitespace(it)) {
				it++;
			} else {
				return false;
			}
		} while (it != end);
		return true;
	};

	auto split = [=](StringView &sv, Iterator &it, const Iterator &end) {
		if (skipWhitespace(it, end))
			return false;

		auto itStart = it;
		size_t len = 0;
		bool hasSymbols = false;
		do {
			if (isSymbol(it)) {
				hasSymbols = true;
				if (len > 0)
					break;
			}
			it++;
			len++;
		} while (it != end && !hasSymbols && !isWhitespace(it));
		sv = StringView(&*itStart, len);
		return true;
	};

	auto itA = a.cbegin();
	auto itB = b.cbegin();
	auto shouldAbort = [&]() { return itA == a.end() || itB == b.end(); };

	while (!shouldAbort()) {
		std::string_view svA;
		std::string_view svB;
		if (!split(svA, itA, a.end()))
			break;
		if (!split(svB, itB, b.end()))
			break;
		if (svA != svB)
			return false;
	}

	bool endA = itA == a.end() ? true : skipWhitespace(itA, a.end());
	bool endB = itB == b.end() ? true : skipWhitespace(itB, b.end());
	return endA == endB;
}

TEST_CASE("Compare & ignore whitespace", "[Shader]") {
	String a = R"(string with whitespace)";
	String b = R"(
string 	with
	whitespace

)";
	CHECK(compareNoWhitespace(a, b));

	String c = "stringwithwhitespace";
	CHECK(!compareNoWhitespace(a, c));

	String d = "string\rwith\nwhitespace";
	CHECK(compareNoWhitespace(a, d));

	String e = "string\twith whitespace";
	CHECK(compareNoWhitespace(a, e));

	String f = "string\t";
	CHECK(!compareNoWhitespace(a, f));
}

TEST_CASE("Shader 1", "[Shader]") {
	Evaluator evaluator;
	evaluator.meshFormat.vertexAttributes = {
		MeshVertexAttribute("position", 3),
		MeshVertexAttribute("normal", 3),
		MeshVertexAttribute("texcoord0", 2),
		MeshVertexAttribute("texcoord1", 3),
	};

	auto root = blocks::makeCompoundBlock("void main() {", blocks::WithAttribute("position", "out.position = in.position;"), "}");
	String wgslSource = evaluator.build(*root.get());
	CHECK(compareNoWhitespace(wgslSource, "void main(){out.position=in.position;}"));
}