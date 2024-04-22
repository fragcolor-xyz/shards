#ifndef B7638520_BA4D_4989_BDC2_7F3533FE84B5
#define B7638520_BA4D_4989_BDC2_7F3533FE84B5

#include <shards/shards.h>
#include <string.h>

namespace shards {
void freeTraitVariable(SHTraitVariable &ivar);
SHTraitVariable cloneTraitVariable(const SHTraitVariable &other);
void freeTrait(SHTrait &trait_);
SHTrait cloneTrait(const SHTrait &other);

struct TraitMatcher {
  std::string error;

  bool operator()(SHExposedTypesInfo exposed, const SHTrait &trait);
};

struct Trait : public SHTrait {
  ~Trait() { reset(); }
  Trait() { memset(this, 0, sizeof(SHTrait)); }
  Trait(const SHTrait &other) : SHTrait(cloneTrait(other)) {}
  Trait(const Trait &other) : SHTrait(cloneTrait(other)) {}
  Trait &operator=(const SHTrait &other) {
    if (this != &other) {
      reset();
      (SHTrait &)*this = cloneTrait(other);
    }
    return *this;
  }
  Trait &operator=(const Trait &other) {
    if (this != &other) {
      reset();
      (SHTrait &)*this = cloneTrait(other);
    }
    return *this;
  }
  Trait(Trait &&other) noexcept : SHTrait(other) { (SHTrait &)other = SHTrait{}; }
  Trait &operator=(Trait &&other) noexcept {
    if (this != &other) {
      reset();
      (SHTrait &)*this = (SHTrait &)other;
      (SHTrait &)other = SHTrait{};
    }
    return *this;
  }

  bool sameIdAs(const SHTrait &other) const { return id[0] == other.id[0] && id[1] == other.id[1]; }

  void reset() { freeTrait(*this); }
};
} // namespace shards

#endif /* B7638520_BA4D_4989_BDC2_7F3533FE84B5 */
