// RenderDevice: queue-family selection as a pure function over synthetic driver layouts (Unit), and a real
// device created and destroyed without validation output (Gpu).

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Render/RenderDevice.h>
#include <Candle/Render/RenderInstance.h>

#include <format>
#include <initializer_list>
#include <optional>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;
using Candle::Test::Type::Unit;

namespace {

	using enum vk::QueueFlagBits;

	// One family per entry, in the order a driver reports them.
	std::vector<vk::QueueFamilyProperties> Families(std::initializer_list<vk::QueueFlags> flags)
	{
		std::vector<vk::QueueFamilyProperties> families;
		for (vk::QueueFlags familyFlags : flags)
			families.push_back({ .queueFlags = familyFlags, .queueCount = 1 });
		return families;
	}

}

// NVIDIA's layout: a pure DMA family ahead of the compute and video families that also report transfer.
CDL_TEST_CASE(RenderDevice, PrefersADedicatedTransferFamily, Unit)
{
	const auto families = Families({
		eGraphics | eCompute | eTransfer | eSparseBinding,
		eTransfer | eSparseBinding,
		eCompute | eTransfer | eSparseBinding,
		eTransfer | eSparseBinding | eVideoDecodeKHR });

	const std::optional<QueueFamilyIndices> indices = FindQueueFamilies(families);
	CDL_CHECK(indices.has_value());
	CDL_EXPECT_EQ(indices->Graphics, 0u);
	CDL_EXPECT_EQ(indices->Transfer, 1u);
}

// AMD's layout: the async compute family comes before the DMA family and must not be taken for transfer.
CDL_TEST_CASE(RenderDevice, SkipsComputeFamiliesForTransfer, Unit)
{
	const auto families = Families({
		eGraphics | eCompute | eTransfer | eSparseBinding,
		eCompute | eTransfer | eSparseBinding,
		eTransfer | eSparseBinding });

	const std::optional<QueueFamilyIndices> indices = FindQueueFamilies(families);
	CDL_CHECK(indices.has_value());
	CDL_EXPECT_EQ(indices->Transfer, 2u);
}

// Video families report transfer too, but they are a different engine and usually expose a single queue.
CDL_TEST_CASE(RenderDevice, SkipsVideoFamiliesForTransfer, Unit)
{
	const auto families = Families({
		eGraphics | eCompute | eTransfer,
		eTransfer | eVideoDecodeKHR,
		eTransfer });

	const std::optional<QueueFamilyIndices> indices = FindQueueFamilies(families);
	CDL_CHECK(indices.has_value());
	CDL_EXPECT_EQ(indices->Transfer, 2u);
}

// Older Intel iGPUs expose one family for everything, so Transfer has to alias Graphics.
CDL_TEST_CASE(RenderDevice, TransferAliasesGraphicsWithoutADedicatedFamily, Unit)
{
	const std::optional<QueueFamilyIndices> single = FindQueueFamilies(Families({ eGraphics | eCompute | eTransfer }));
	CDL_CHECK(single.has_value());
	CDL_EXPECT_EQ(single->Graphics, 0u);
	CDL_EXPECT_EQ(single->Transfer, 0u);

	const std::optional<QueueFamilyIndices> withCompute = FindQueueFamilies(Families({ eGraphics | eCompute | eTransfer, eCompute | eTransfer }));
	CDL_CHECK(withCompute.has_value());
	CDL_EXPECT_EQ(withCompute->Transfer, withCompute->Graphics);
}

// The spec makes reporting the transfer bit optional on graphics and compute families, since both imply it.
CDL_TEST_CASE(RenderDevice, GraphicsFamilyNeedNotReportTransfer, Unit)
{
	const std::optional<QueueFamilyIndices> indices = FindQueueFamilies(Families({ eGraphics | eCompute }));
	CDL_CHECK(indices.has_value());
	CDL_EXPECT_EQ(indices->Graphics, 0u);
	CDL_EXPECT_EQ(indices->Transfer, 0u);
}

CDL_TEST_CASE(RenderDevice, FindsAGraphicsFamilyThatIsNotFirst, Unit)
{
	const std::optional<QueueFamilyIndices> indices = FindQueueFamilies(Families({ eCompute | eTransfer, eTransfer, eGraphics | eCompute | eTransfer }));
	CDL_CHECK(indices.has_value());
	CDL_EXPECT_EQ(indices->Graphics, 2u);
	CDL_EXPECT_EQ(indices->Transfer, 1u);
}

// Graphics and compute must share a family; split across two, the device is unsuitable.
CDL_TEST_CASE(RenderDevice, RejectsADeviceWithoutAGraphicsComputeFamily, Unit)
{
	CDL_EXPECT_FALSE(FindQueueFamilies(Families({ eGraphics | eTransfer, eCompute | eTransfer })).has_value());
	CDL_EXPECT_FALSE(FindQueueFamilies({}).has_value());
}

// Selection, logical device and queue retrieval on whatever GPU the machine picks, checked by validation.
CDL_TEST_CASE(RenderDevice, CreatesAndDestroysWithoutValidationMessages, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		WindowingBackend backend;
		const RenderSpecification spec = StrictRenderSpec();
		RenderInstance instance(TestInstanceSpec(spec));
		RenderDevice device(instance, spec);

		CDL_EXPECT(*device.GetDevice());
		CDL_NOTE(std::format("graphics family {}, transfer family {}",
			device.GetQueueFamily(QueueType::Graphics), device.GetQueueFamily(QueueType::Transfer)));
	}
	ExpectNoVulkanDebugMessages(logs);
}
