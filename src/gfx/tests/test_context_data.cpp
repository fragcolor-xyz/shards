#include "./context.hpp"
#include "./data.hpp"
#include <gfx/context.hpp>
#include <gfx/context_data.hpp>
#include <catch2/catch_all.hpp>

using namespace gfx;

struct TestContextData : public ContextData {
  WGPUBuffer buffer;

  ~TestContextData() { releaseContextDataConditional(); }
  void releaseContextData() override { WGPU_SAFE_RELEASE(wgpuBufferRelease, buffer); }
};
struct TestObject : public TWithContextData<TestContextData> {
protected:
  void initContextData(Context &context, TestContextData &d) override {
    WGPUBufferDescriptor desc{};
    desc.size = 64;
    desc.usage = WGPUBufferUsage_CopyDst;
    d.buffer = wgpuDeviceCreateBuffer(context.wgpuDevice, &desc);
  }
};

TEST_CASE("Context data", "[Context]") {
  std::shared_ptr<TestContext> context = createTestContext();

  SECTION("Weak reference by context") {
    std::weak_ptr<TestContextData> weakTestContextData;
    {
      TestObject a;
      a.createContextDataConditional(*context.get());
      weakTestContextData = a.contextData;

      CHECK(!weakTestContextData.expired());
      CHECK(weakTestContextData.lock()->buffer);
    }
    CHECK(weakTestContextData.expired());
  }
}

TEST_CASE("Context data releaseContextData", "[Context]") {
  std::shared_ptr<TestContextData> testContextData;
  {
    std::shared_ptr<TestContext> context = createTestContext();

    TestObject a;
    a.createContextDataConditional(*context.get());
    testContextData = a.contextData;

    CHECK(testContextData->buffer);
  }

  CHECK(testContextData);
  CHECK(testContextData->buffer == nullptr);
}
