#include "test_data.hpp"
#include <gfx/context.hpp>
#include <gfx/context_data.hpp>

using namespace gfx;

struct TestContextData : public ContextData {
	WGPUBuffer buffer;

	~TestContextData() { releaseConditional(); }
	void release() { WGPU_SAFE_RELEASE(wgpuBufferRelease, buffer); }
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
	gfx::Context context;
	context.init();

	SECTION("Weak reference by context") {
		std::weak_ptr<TestContextData> weakTestContextData;
		{
			TestObject a;
			a.createContextDataConditional(context);
			weakTestContextData = a.contextData;

			CHECK(!weakTestContextData.expired());
			CHECK(weakTestContextData.lock()->buffer);
		}
		CHECK(weakTestContextData.expired());
	}
}

TEST_CASE("Context data release", "[Context]") {
	std::shared_ptr<TestContextData> testContextData;
	{
		gfx::Context context;
		context.init();

		TestObject a;
		a.createContextDataConditional(context);
		testContextData = a.contextData;

		CHECK(testContextData->buffer);
	}

	CHECK(testContextData);
	CHECK(testContextData->buffer == nullptr);
}
