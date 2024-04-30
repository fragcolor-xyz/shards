#ifndef B7638520_BA4D_4989_BDC2_7F3533FE84B5
#define B7638520_BA4D_4989_BDC2_7F3533FE84B5

#include "hash.hpp"
#include <shards/shards.h>
#include <string.h>
#include <map>
#include <shared_mutex>

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

  XXH128_hash_t hash128() const {
    XXH128_hash_t r;
    memcpy(&r, id, sizeof(r));
    return r;
  }

  void reset() { freeTrait(*this); }
};

struct TraitRegister {
private:
  std::map<XXH128_hash_t, shards::Trait> traits;
  std::shared_mutex mutex;

public:
  TraitRegister() = default;
  TraitRegister(const TraitRegister &) = delete;
  TraitRegister &operator=(const TraitRegister &) = delete;

  const shards::Trait &insertUnique(const SHTrait &trait) {
    auto hash = ((Trait &)trait).hash128();
    std::shared_lock<decltype(mutex)> lock(mutex);
    auto it = traits.find(hash);
    if (it == traits.end()) {
      lock.unlock();
      std::unique_lock<decltype(mutex)> lock(mutex);
      it = traits.find(hash); // Need to double check after re-lock
      if (it != traits.end()) {
        return it->second;
      }

      it = traits.emplace(hash, trait).first;
      return it->second;
    }
    return it->second;
  }

  const shards::Trait *findTrait(const SHTrait &trait) {
    auto hash = ((Trait &)trait).hash128();
    std::shared_lock<decltype(mutex)> lock(mutex);
    auto it = traits.find(hash);
    if (it == traits.end()) {
      return nullptr;
    }
    return &it->second;
  }

  static TraitRegister &instance();
};

} // namespace shards

#endif /* B7638520_BA4D_4989_BDC2_7F3533FE84B5 */
