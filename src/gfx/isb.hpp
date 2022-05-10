#ifndef GFX_ISB
#define GFX_ISB

#include <functional>
#include <stdint.h>
#include <string.h>
#include <vector>

namespace gfx {

struct ImmutableSharedBuffer {
private:
  struct IContainer {
    virtual ~IContainer() = default;
    virtual size_t getLength() = 0;
    virtual const uint8_t *getData() = 0;
  };

  struct OwnedContainer : public IContainer {
    std::vector<uint8_t> data;

    OwnedContainer() = default;
    OwnedContainer(std::vector<uint8_t> &&data) : data(std::move(data)) {}

    virtual size_t getLength() { return data.size(); }
    virtual const uint8_t *getData() { return data.data(); }
  };

  struct ForeignContainer : public IContainer {
    uint8_t *data{};
    size_t dataLength{};
    void *userData{};
    std::function<void(uint8_t *data, void *userData)> freeCallback;

    ForeignContainer() = default;
    ~ForeignContainer() { freeCallback(data, userData); }
    virtual size_t getLength() { return dataLength; }
    virtual const uint8_t *getData() { return data; }
  };

  std::shared_ptr<IContainer> container;

public:
  ImmutableSharedBuffer() = default;

  ImmutableSharedBuffer(const void *inData, size_t inDataLength) {
    container = std::make_shared<OwnedContainer>();
    auto &owned = *(OwnedContainer *)container.get();
    owned.data.resize(inDataLength);
    memcpy(owned.data.data(), inData, inDataLength);
  }

  ImmutableSharedBuffer(const void *inData, size_t inDataLength, std::function<void(void *, void *)> freeCallback,
                        void *userData = nullptr) {
    container = std::make_shared<ForeignContainer>();
    auto &foreign = *(ForeignContainer *)container.get();
    foreign.data = (uint8_t *)inData;
    foreign.dataLength = inDataLength;
    foreign.freeCallback = freeCallback;
    foreign.userData = userData;
  }

  ImmutableSharedBuffer(std::vector<uint8_t> &&data) { container = std::make_shared<OwnedContainer>(std::move(data)); }

  size_t getLength() const { return container ? container->getLength() : 0; }
  const uint8_t *getData() const { return container ? container->getData() : nullptr; }
  operator bool() const { return (bool)container; }
};

} // namespace gfx

#endif // GFX_ISB
