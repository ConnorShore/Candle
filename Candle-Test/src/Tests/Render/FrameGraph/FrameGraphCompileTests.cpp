// Compile turns each pass's declared accesses into the plan Execute records from: each pass's barriers and attachments,
// then the barriers that leave every import in its final state. Each test pins one rule, and a failing test prints the
// whole plan under it. Imports need a real TexturePool and transients real images, so these are Gpu tests.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "../RenderTestHelpers.h"

#include <Candle/Render/FrameGraph/FrameGraph.h>
#include <Candle/Render/RenderConstants.h>
#include <Candle/Render/Resources/TexturePool.h>
#include <Candle/Render/Vulkan/SwapChainImage.h>

#include <algorithm>
#include <exception>
#include <format>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

namespace {

	const glm::uvec2 kOutputExtent{ 8, 4 };
	constexpr vk::Format kColorFormat = vk::Format::eR8G8B8A8Unorm;
	constexpr vk::Format kDepthFormat = vk::Format::eD16Unorm;	// The one depth format every device must support as an attachment
	const vk::ClearColorValue kClearColor(0.25f, 0.5f, 0.75f, 1.0f);

	constexpr uint32_t kFirstFrameGraphSlot = kMaxTextures - kMaxFrameGraphTextures;

	// Where SwapChain::AquireNextImage leaves an image
	constexpr ImageState kAcquiredState{
		.Layout = vk::ImageLayout::eUndefined,
		.Access = vk::AccessFlagBits2::eNone,
		.Stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput };

	constexpr auto kNoExecute = [](const auto&, FrameGraphContext&) {};

	struct TestFrameGraph : TestTextures
	{
		FrameGraph Graph{ { .Allocator = Allocator, .BindlessTable = Bindless, .TexturePool = Pool } };

		// A fresh pool render target imported into this frame. name must outlive the frame, e.g. a string literal.
		FGTextureHandle ImportTarget(std::string_view name,
			vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled)
		{
			const GPUTextureHandle target = Pool.CreateRenderTarget({ .Extent = kOutputExtent, .Format = kColorFormat, .Usage = usage, .DebugName = std::string(name) });
			return Graph.ImportTexture(name, target);
		}

		// The pool keeps a storage target in General, and samples it there too
		FGTextureHandle ImportStorageTarget(std::string_view name)
		{
			return ImportTarget(name, vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled);
		}
	};

	// Stands in for an acquired swapchain image; the test owns the VkImage the swapchain would. The view is declared
	// after the image, so it's destroyed first.
	struct TestSwapChainImage
	{
		OwnedImage Owned;
		SwapChainImage Image;

		explicit TestSwapChainImage(RenderDevice& device)
			: Owned(CreateOwnedImage(device, kColorFormat, { kOutputExtent.x, kOutputExtent.y }, 1, vk::ImageUsageFlagBits::eColorAttachment))
			, Image(*Owned.Handle,
				vk::raii::ImageView(device.GetDevice(), vk::ImageViewCreateInfo{
					.image = *Owned.Handle, .viewType = vk::ImageViewType::e2D, .format = kColorFormat, .subresourceRange = SwapChainImage::kSubresourceRange }),
				ImageDesc{ .Extent = kOutputExtent, .Format = kColorFormat, .Usage = vk::ImageUsageFlagBits::eColorAttachment })
		{
			Image.SetState(kAcquiredState);
		}
	};

	FGTextureSpec OutputSized(vk::Format format, float scale = 1.0f)
	{
		return { .Size = { .Mode = FGSizeMode::RelativeToOutput, .Scale = scale }, .Format = format };
	}

	// A pass that only declares accesses, since compile never runs execute
	template<typename SetupFunc>
	void DeclarePass(FrameGraph& graph, std::string_view name, SetupFunc setup)
	{
		graph.AddRasterPass(name, [&setup](FrameGraphBuilder& builder, FGNoData&) { setup(builder); }, kNoExecute);
	}

	// One frame with a single pass clearing an output-sized transient, for tests that span frames
	FGTextureHandle RecordClearFrame(FrameGraph& graph, uint64_t frame)
	{
		graph.Reset(kOutputExtent);
		FGTextureHandle transient;
		DeclarePass(graph, "Draw", [&](FrameGraphBuilder& builder)
			{
				transient = builder.CreateTexture("Transient", OutputSized(kColorFormat));
				builder.ColorAttachment(transient, FGLoad::Clear, kClearColor);
			});
		graph.Compile(frame);
		return transient;
	}

	template<typename T>
	std::span<const T> Range(const std::vector<T>& items, uint32_t first, uint32_t count)
	{
		CDL_CHECK_MSG(static_cast<size_t>(first) + count <= items.size(),
			std::format("range [{}, {}) runs past the {} compiled entries", first, first + count, items.size()));
		return std::span(items).subspan(first, count);
	}

	std::span<const FGBarrier> BarriersOf(const FGCompiledGraph& plan, uint32_t pass)
	{
		CDL_CHECK(pass < plan.Passes.size());
		return Range(plan.Barriers, plan.Passes[pass].BarrierStartIndex, plan.Passes[pass].BarrierCount);
	}

	std::span<const FGCompiledAttachment> AttachmentsOf(const FGCompiledGraph& plan, uint32_t pass)
	{
		CDL_CHECK(pass < plan.Passes.size());
		return Range(plan.Attachments, plan.Passes[pass].AttachmentStartIndex, plan.Passes[pass].AttachmentCount);
	}

	// Null when there's none, so a test can tell a missing barrier from a wrong one
	const FGBarrier* FindBarrier(std::span<const FGBarrier> barriers, FGTextureHandle texture)
	{
		const auto found = std::ranges::find(barriers, texture, &FGBarrier::Texture);
		return found == barriers.end() ? nullptr : &*found;
	}

	template<typename Bits>
	bool Covers(vk::Flags<Bits> flags, std::type_identity_t<vk::Flags<Bits>> required)
	{
		return (flags & required) == required;
	}

	std::string DescribeState(const ImageState& state)
	{
		return std::format("{} access {} stage {}", vk::to_string(state.Layout), vk::to_string(state.Access), vk::to_string(state.Stage));
	}

	void ExpectState(const ImageState& actual, const ImageState& expected, std::string_view label)
	{
		const bool same = actual.Layout == expected.Layout && actual.Access == expected.Access && actual.Stage == expected.Stage;
		CDL_EXPECT_MSG(same, std::format("{} is {}, expected {}", label, DescribeState(actual), DescribeState(expected)));
	}

	// At least these access and stage bits, since a correct compile may merge in more
	void ExpectWaitsFor(const ImageState& actual, vk::AccessFlags2 access, vk::PipelineStageFlags2 stage, std::string_view label)
	{
		CDL_EXPECT_MSG(Covers(actual.Access, access) && Covers(actual.Stage, stage),
			std::format("{} is {}, expected at least access {} stage {}", label, DescribeState(actual), vk::to_string(access), vk::to_string(stage)));
	}

	void ExpectCovers(const ImageState& actual, vk::ImageLayout layout, vk::AccessFlags2 access, vk::PipelineStageFlags2 stage, std::string_view label)
	{
		CDL_EXPECT_MSG(actual.Layout == layout,
			std::format("{} is {}, expected layout {}", label, DescribeState(actual), vk::to_string(layout)));
		ExpectWaitsFor(actual, access, stage, label);
	}

	// Clamps rather than checks, since it runs while a failed CDL_CHECK unwinds and must not throw
	template<typename T>
	std::span<const T> ClampedRange(const std::vector<T>& items, uint32_t first, uint32_t count)
	{
		const size_t begin = std::min<size_t>(first, items.size());
		return std::span(items).subspan(begin, std::min<size_t>(count, items.size() - begin));
	}

	std::vector<std::string> DescribePlan(const FrameGraph& graph)
	{
		const FGCompiledGraph& plan = graph.GetCompiledGraph();
		const auto describeBarrier = [&graph](const FGBarrier& barrier)
			{
				return std::format("    barrier {}: {}  ->  {}", graph.GetTexture(barrier.Texture).Name, DescribeState(barrier.FromState), DescribeState(barrier.ToState));
			};

		std::vector<std::string> lines{ "Compiled plan:" };
		for (size_t i = 0; i < plan.Passes.size(); ++i)
		{
			const FGCompiledPass& pass = plan.Passes[i];
			lines.push_back(std::format("  pass {} '{}'{}", i, pass.Name, pass.HasDepthAttachment ? " (has depth)" : ""));
			for (const FGBarrier& barrier : ClampedRange(plan.Barriers, pass.BarrierStartIndex, pass.BarrierCount))
				lines.push_back(describeBarrier(barrier));
			for (const FGCompiledAttachment& attachment : ClampedRange(plan.Attachments, pass.AttachmentStartIndex, pass.AttachmentCount))
			{
				lines.push_back(std::format("    attachment {}: {} load {} store {}", graph.GetTexture(attachment.Texture).Name,
					vk::to_string(attachment.Layout), vk::to_string(attachment.LoadOp), vk::to_string(attachment.StoreOp)));
			}
		}

		lines.push_back("  final");
		for (const FGBarrier& barrier : plan.FinalBarriers)
			lines.push_back(describeBarrier(barrier));
		return lines;
	}

	// Prints the plan under a failing test, since one wrong barrier is easiest to read next to the rest
	class NotePlanOnFailure
	{
	public:
		explicit NotePlanOnFailure(const FrameGraph& graph) : m_Graph(graph), m_UncaughtOnEntry(std::uncaught_exceptions()) {}

		~NotePlanOnFailure()
		{
			const bool failed = !Candle::Test::Context::Get().SoftFailures.empty() || std::uncaught_exceptions() > m_UncaughtOnEntry;
			if (!failed)
				return;

			for (std::string& line : DescribePlan(m_Graph))
				CDL_NOTE(std::move(line));
		}

	private:
		const FrameGraph& m_Graph;
		int m_UncaughtOnEntry;
	};

}

CDL_TEST_CASE(FrameGraphCompile, CompilesOnePassPerAddedPassInOrder, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		NotePlanOnFailure dump(test.Graph);
		test.Graph.Reset(kOutputExtent);

		DeclarePass(test.Graph, "First", [](FrameGraphBuilder&) {});
		DeclarePass(test.Graph, "Second", [](FrameGraphBuilder&) {});
		DeclarePass(test.Graph, "Third", [](FrameGraphBuilder&) {});
		test.Graph.Compile(1);

		const FGCompiledGraph& plan = test.Graph.GetCompiledGraph();
		CDL_CHECK_EQ(plan.Passes.size(), 3u);
		CDL_EXPECT(plan.Passes[0].Name == "First");
		CDL_EXPECT(plan.Passes[1].Name == "Second");
		CDL_EXPECT(plan.Passes[2].Name == "Third");
		for (const FGCompiledPass& pass : plan.Passes)
		{
			CDL_EXPECT_EQ(pass.BarrierCount, 0u);
			CDL_EXPECT_EQ(pass.AttachmentCount, 0u);
			CDL_EXPECT_FALSE(pass.HasDepthAttachment);
		}
		CDL_EXPECT(plan.FinalBarriers.empty());
	}
	ExpectNoVulkanDebugMessages(logs);
}

CDL_TEST_CASE(FrameGraphCompile, EachPassOwnsTheBarriersAndAttachmentsItDeclared, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		NotePlanOnFailure dump(test.Graph);
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle scene = test.ImportTarget("Scene");
		const FGTextureHandle gui = test.ImportTarget("Gui");
		DeclarePass(test.Graph, "Scene", [&](FrameGraphBuilder& builder) { builder.ColorAttachment(scene, FGLoad::Clear, kClearColor); });
		DeclarePass(test.Graph, "Gui", [&](FrameGraphBuilder& builder)
			{
				builder.ReadTexture(scene, FGUsage::SampledFragment);
				builder.ColorAttachment(gui, FGLoad::Clear, kClearColor);
			});
		test.Graph.Compile(1);

		const FGCompiledGraph& plan = test.Graph.GetCompiledGraph();
		CDL_CHECK_EQ(plan.Passes.size(), 2u);
		const FGCompiledPass& first = plan.Passes[0];
		const FGCompiledPass& second = plan.Passes[1];
		CDL_EXPECT_EQ(second.BarrierStartIndex, first.BarrierStartIndex + first.BarrierCount);
		CDL_EXPECT_EQ(second.AttachmentStartIndex, first.AttachmentStartIndex + first.AttachmentCount);

		// Every access here changes its texture's layout, so each one needs a barrier of its own
		CDL_EXPECT_EQ(first.BarrierCount, 1u);
		CDL_EXPECT(FindBarrier(BarriersOf(plan, 0), scene));
		CDL_EXPECT_EQ(second.BarrierCount, 2u);
		CDL_EXPECT(FindBarrier(BarriersOf(plan, 1), scene));
		CDL_EXPECT(FindBarrier(BarriersOf(plan, 1), gui));

		const std::span<const FGCompiledAttachment> firstAttachments = AttachmentsOf(plan, 0);
		const std::span<const FGCompiledAttachment> secondAttachments = AttachmentsOf(plan, 1);
		CDL_CHECK_EQ(firstAttachments.size(), 1u);
		CDL_CHECK_EQ(secondAttachments.size(), 1u);
		CDL_EXPECT(firstAttachments[0].Texture == scene);
		CDL_EXPECT(secondAttachments[0].Texture == gui);
	}
	ExpectNoVulkanDebugMessages(logs);
}

CDL_TEST_CASE(FrameGraphCompile, ColorAttachmentKeepsItsLoadOpAndClearValue, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		NotePlanOnFailure dump(test.Graph);
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle target = test.ImportTarget("Target");
		DeclarePass(test.Graph, "Clears", [&](FrameGraphBuilder& builder) { builder.ColorAttachment(target, FGLoad::Clear, kClearColor); });
		DeclarePass(test.Graph, "Loads", [&](FrameGraphBuilder& builder) { builder.ColorAttachment(target, FGLoad::Load, kClearColor); });
		DeclarePass(test.Graph, "DontCares", [&](FrameGraphBuilder& builder) { builder.ColorAttachment(target, FGLoad::DontCare, kClearColor); });
		test.Graph.Compile(1);

		const FGCompiledGraph& plan = test.Graph.GetCompiledGraph();
		const vk::AttachmentLoadOp expected[] = { vk::AttachmentLoadOp::eClear, vk::AttachmentLoadOp::eLoad, vk::AttachmentLoadOp::eDontCare };
		for (uint32_t pass = 0; pass < std::size(expected); ++pass)
		{
			const std::span<const FGCompiledAttachment> attachments = AttachmentsOf(plan, pass);
			CDL_CHECK_EQ(attachments.size(), 1u);
			CDL_EXPECT_EQ(attachments[0].LoadOp, expected[pass]);
			CDL_EXPECT_EQ(attachments[0].Layout, vk::ImageLayout::eColorAttachmentOptimal);
		}

		CDL_EXPECT(std::ranges::equal(AttachmentsOf(plan, 0)[0].ClearValue.color.float32, kClearColor.float32));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Their contents leave the graph: the pool's samplers and presentation read them after the frame
CDL_TEST_CASE(FrameGraphCompile, AttachmentsToImportsAreAlwaysStored, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		TestSwapChainImage swapChain(test.Device);
		NotePlanOnFailure dump(test.Graph);
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle target = test.ImportTarget("Target");
		const FGTextureHandle backBuffer = test.Graph.ImportSwapChainImage(swapChain.Image);
		DeclarePass(test.Graph, "Draw", [&](FrameGraphBuilder& builder)
			{
				builder.ColorAttachment(target, FGLoad::Clear, kClearColor);
				builder.ColorAttachment(backBuffer, FGLoad::DontCare, kClearColor);
			});
		test.Graph.Compile(1);

		const std::span<const FGCompiledAttachment> attachments = AttachmentsOf(test.Graph.GetCompiledGraph(), 0);
		CDL_CHECK_EQ(attachments.size(), 2u);
		for (const FGCompiledAttachment& attachment : attachments)
			CDL_EXPECT_EQ(attachment.StoreOp, vk::AttachmentStoreOp::eStore);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Execute takes the last attachment as depth when HasDepthAttachment is set, whatever order setup declared them in
CDL_TEST_CASE(FrameGraphCompile, DepthAttachmentIsFlaggedAndComesAfterTheColors, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		NotePlanOnFailure dump(test.Graph);
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle color = test.ImportTarget("Color");
		FGTextureHandle depth;
		DeclarePass(test.Graph, "Scene", [&](FrameGraphBuilder& builder)
			{
				depth = builder.CreateTexture("Depth", OutputSized(kDepthFormat));
				builder.DepthAttachment(depth, FGLoad::Clear);
				builder.ColorAttachment(color, FGLoad::Clear, kClearColor);
			});
		test.Graph.Compile(1);

		const FGCompiledGraph& plan = test.Graph.GetCompiledGraph();
		CDL_CHECK_EQ(plan.Passes.size(), 1u);
		CDL_EXPECT(plan.Passes[0].HasDepthAttachment);

		const std::span<const FGCompiledAttachment> attachments = AttachmentsOf(plan, 0);
		CDL_CHECK_EQ(attachments.size(), 2u);
		CDL_EXPECT(attachments[0].Texture == color);
		CDL_EXPECT(attachments[1].Texture == depth);
		CDL_EXPECT_EQ(attachments[1].Layout, vk::ImageLayout::eDepthStencilAttachmentOptimal);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Imports were last used outside the graph, so the first barrier has to start from the state the import recorded.
// For the swapchain that's acquire's wait stage, which is what chains the barrier with the semaphore wait.
CDL_TEST_CASE(FrameGraphCompile, FirstUseOfAnImportStartsFromItsInitialState, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		TestSwapChainImage swapChain(test.Device);
		NotePlanOnFailure dump(test.Graph);
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle target = test.ImportTarget("Target");
		const FGTextureHandle backBuffer = test.Graph.ImportSwapChainImage(swapChain.Image);
		DeclarePass(test.Graph, "Draw", [&](FrameGraphBuilder& builder)
			{
				builder.ColorAttachment(target, FGLoad::Clear, kClearColor);
				builder.ColorAttachment(backBuffer, FGLoad::Clear, kClearColor);
			});
		test.Graph.Compile(1);

		const std::span<const FGBarrier> barriers = BarriersOf(test.Graph.GetCompiledGraph(), 0);
		const FGBarrier* targetBarrier = FindBarrier(barriers, target);
		const FGBarrier* backBufferBarrier = FindBarrier(barriers, backBuffer);
		CDL_CHECK(targetBarrier);
		CDL_CHECK(backBufferBarrier);

		ExpectState(targetBarrier->FromState, test.Graph.GetTexture(target).InitialState, "Target's first barrier source");
		ExpectState(backBufferBarrier->FromState, kAcquiredState, "Back buffer's first barrier source");
		ExpectCovers(targetBarrier->ToState, vk::ImageLayout::eColorAttachmentOptimal,
			vk::AccessFlagBits2::eColorAttachmentWrite, vk::PipelineStageFlagBits2::eColorAttachmentOutput, "Target's first barrier destination");
		ExpectCovers(backBufferBarrier->ToState, vk::ImageLayout::eColorAttachmentOptimal,
			vk::AccessFlagBits2::eColorAttachmentWrite, vk::PipelineStageFlagBits2::eColorAttachmentOutput, "Back buffer's first barrier destination");
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The scene pass draws the output and the GUI samples it, as when the scene renders into a window. The GUI's barrier
// must start from the scene's writes, or it could sample before they land.
CDL_TEST_CASE(FrameGraphCompile, SampledReadWaitsForTheEarlierPassesWrites, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		NotePlanOnFailure dump(test.Graph);
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle output = test.ImportTarget("Output");
		DeclarePass(test.Graph, "Scene", [&](FrameGraphBuilder& builder) { builder.ColorAttachment(output, FGLoad::Clear, kClearColor); });
		DeclarePass(test.Graph, "Gui", [&](FrameGraphBuilder& builder) { builder.ReadTexture(output, FGUsage::SampledFragment); });
		test.Graph.Compile(1);

		const FGBarrier* barrier = FindBarrier(BarriersOf(test.Graph.GetCompiledGraph(), 1), output);
		CDL_CHECK(barrier);
		ExpectCovers(barrier->FromState, vk::ImageLayout::eColorAttachmentOptimal,
			vk::AccessFlagBits2::eColorAttachmentWrite, vk::PipelineStageFlagBits2::eColorAttachmentOutput, "The GUI's barrier source");
		ExpectCovers(barrier->ToState, vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::AccessFlagBits2::eShaderSampledRead, vk::PipelineStageFlagBits2::eFragmentShader, "The GUI's barrier destination");
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The layout doesn't change, but the debug pass loads what the scene pass wrote, so it still needs a barrier, and one
// from Undefined would throw the scene away
CDL_TEST_CASE(FrameGraphCompile, LoadedAttachmentWaitsForTheEarlierPassesWrites, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		TestSwapChainImage swapChain(test.Device);
		NotePlanOnFailure dump(test.Graph);
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle backBuffer = test.Graph.ImportSwapChainImage(swapChain.Image);
		DeclarePass(test.Graph, "Scene", [&](FrameGraphBuilder& builder) { builder.ColorAttachment(backBuffer, FGLoad::Clear, kClearColor); });
		DeclarePass(test.Graph, "DebugDraw", [&](FrameGraphBuilder& builder) { builder.ColorAttachment(backBuffer, FGLoad::Load, kClearColor); });
		test.Graph.Compile(1);

		const FGBarrier* barrier = FindBarrier(BarriersOf(test.Graph.GetCompiledGraph(), 1), backBuffer);
		CDL_CHECK_MSG(barrier, "a load after a write needs a barrier even when the layout stays the same");
		ExpectCovers(barrier->FromState, vk::ImageLayout::eColorAttachmentOptimal,
			vk::AccessFlagBits2::eColorAttachmentWrite, vk::PipelineStageFlagBits2::eColorAttachmentOutput, "The debug pass's barrier source");
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The frame OldRender draws to the swapchain: scene, debug draw over it, GUI over that, then present. Each barrier has to
// start where the one before left the image, all the way through to the present.
CDL_TEST_CASE(FrameGraphCompile, SwapChainPassesChainTheirBarriersThroughToPresent, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		TestSwapChainImage swapChain(test.Device);
		NotePlanOnFailure dump(test.Graph);
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle backBuffer = test.Graph.ImportSwapChainImage(swapChain.Image);
		DeclarePass(test.Graph, "Scene", [&](FrameGraphBuilder& builder) { builder.ColorAttachment(backBuffer, FGLoad::Clear, kClearColor); });
		DeclarePass(test.Graph, "DebugDraw", [&](FrameGraphBuilder& builder) { builder.ColorAttachment(backBuffer, FGLoad::Load, kClearColor); });
		DeclarePass(test.Graph, "Gui", [&](FrameGraphBuilder& builder) { builder.ColorAttachment(backBuffer, FGLoad::Load, kClearColor); });
		test.Graph.Compile(1);

		const FGCompiledGraph& plan = test.Graph.GetCompiledGraph();
		CDL_CHECK_EQ(plan.Passes.size(), 3u);
		std::vector<const FGBarrier*> chain;
		for (uint32_t pass = 0; pass < plan.Passes.size(); ++pass)
		{
			const FGBarrier* barrier = FindBarrier(BarriersOf(plan, pass), backBuffer);
			CDL_CHECK_MSG(barrier, std::format("pass {} has no barrier for the back buffer", pass));
			chain.push_back(barrier);
		}
		const FGBarrier* present = FindBarrier(plan.FinalBarriers, backBuffer);
		CDL_CHECK_MSG(present, "the back buffer has no final barrier");
		chain.push_back(present);

		ExpectState(chain.front()->FromState, kAcquiredState, "The first barrier's source");
		for (size_t i = 1; i < chain.size(); ++i)
		{
			const ImageState& previous = chain[i - 1]->ToState;
			const ImageState& from = chain[i]->FromState;
			CDL_EXPECT_MSG(from.Layout == previous.Layout && Covers(from.Stage, previous.Stage),
				std::format("barrier {} starts from {}, but the one before left it {}", i, DescribeState(from), DescribeState(previous)));
		}
		CDL_EXPECT_EQ(present->ToState.Layout, vk::ImageLayout::ePresentSrcKHR);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// From the import's initial state instead, the final barrier would discard everything the frame drew into it
CDL_TEST_CASE(FrameGraphCompile, FinalBarrierStartsWhereTheLastPassLeftTheImport, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		NotePlanOnFailure dump(test.Graph);
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle target = test.ImportTarget("Target");
		DeclarePass(test.Graph, "Draw", [&](FrameGraphBuilder& builder) { builder.ColorAttachment(target, FGLoad::Clear, kClearColor); });
		test.Graph.Compile(1);

		const FGBarrier* barrier = FindBarrier(test.Graph.GetCompiledGraph().FinalBarriers, target);
		CDL_CHECK(barrier);
		ExpectCovers(barrier->FromState, vk::ImageLayout::eColorAttachmentOptimal,
			vk::AccessFlagBits2::eColorAttachmentWrite, vk::PipelineStageFlagBits2::eColorAttachmentOutput, "The final barrier's source");
		ExpectState(barrier->ToState, test.Graph.GetTexture(target).FinalState, "The final barrier's destination");
	}
	ExpectNoVulkanDebugMessages(logs);
}

CDL_TEST_CASE(FrameGraphCompile, EveryWrittenImportGetsOneFinalBarrierToItsFinalState, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		TestSwapChainImage swapChain(test.Device);
		NotePlanOnFailure dump(test.Graph);
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle target = test.ImportTarget("Target");
		const FGTextureHandle backBuffer = test.Graph.ImportSwapChainImage(swapChain.Image);
		DeclarePass(test.Graph, "Draw", [&](FrameGraphBuilder& builder)
			{
				builder.ColorAttachment(target, FGLoad::Clear, kClearColor);
				builder.ColorAttachment(backBuffer, FGLoad::Clear, kClearColor);
			});
		test.Graph.Compile(1);

		const FGCompiledGraph& plan = test.Graph.GetCompiledGraph();
		for (const FGTextureHandle imported : { target, backBuffer })
		{
			const FGTextureEntry& entry = test.Graph.GetTexture(imported);
			CDL_EXPECT_MSG(std::ranges::count(plan.FinalBarriers, imported, &FGBarrier::Texture) == 1, std::format("{} needs exactly one final barrier", entry.Name));

			const FGBarrier* barrier = FindBarrier(plan.FinalBarriers, imported);
			if (barrier)
				ExpectState(barrier->ToState, entry.FinalState, std::format("{}'s final barrier destination", entry.Name));
		}
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A transient's contents die with the frame, so nothing outside the graph waits on it
CDL_TEST_CASE(FrameGraphCompile, TransientsGetNoFinalBarrier, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		NotePlanOnFailure dump(test.Graph);
		test.Graph.Reset(kOutputExtent);

		FGTextureHandle transient;
		DeclarePass(test.Graph, "Draw", [&](FrameGraphBuilder& builder)
			{
				transient = builder.CreateTexture("Transient", OutputSized(kColorFormat));
				builder.ColorAttachment(transient, FGLoad::Clear, kClearColor);
			});
		DeclarePass(test.Graph, "Sample", [&](FrameGraphBuilder& builder) { builder.ReadTexture(transient, FGUsage::SampledFragment); });
		test.Graph.Compile(1);

		CDL_EXPECT(!FindBarrier(test.Graph.GetCompiledGraph().FinalBarriers, transient));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A write after a read needs only an execution dependency, but it does need one, or the redraw could overwrite the
// image while the blur is still sampling it
CDL_TEST_CASE(FrameGraphCompile, WriteAfterReadWaitsForTheRead, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		NotePlanOnFailure dump(test.Graph);
		test.Graph.Reset(kOutputExtent);

		FGTextureHandle transient;
		DeclarePass(test.Graph, "Draw", [&](FrameGraphBuilder& builder)
			{
				transient = builder.CreateTexture("Transient", OutputSized(kColorFormat));
				builder.ColorAttachment(transient, FGLoad::Clear, kClearColor);
			});
		DeclarePass(test.Graph, "Blur", [&](FrameGraphBuilder& builder) { builder.ReadTexture(transient, FGUsage::SampledCompute); });
		DeclarePass(test.Graph, "Redraw", [&](FrameGraphBuilder& builder) { builder.ColorAttachment(transient, FGLoad::Clear, kClearColor); });
		test.Graph.Compile(1);

		const FGBarrier* barrier = FindBarrier(BarriersOf(test.Graph.GetCompiledGraph(), 2), transient);
		CDL_CHECK(barrier);
		ExpectWaitsFor(barrier->FromState, {}, vk::PipelineStageFlagBits2::eComputeShader, "The redraw's barrier source");
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Reads don't conflict, so a second read in the same layout needs no barrier. This one is an optimisation rather than
// a correctness rule; WriteAfterSeveralReadsWaitsForEveryOne is what keeps skipping it safe.
CDL_TEST_CASE(FrameGraphCompile, ReadAfterReadInTheSameLayoutAddsNoBarrier, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		NotePlanOnFailure dump(test.Graph);
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle lookup = test.ImportTarget("Lookup");
		DeclarePass(test.Graph, "FragmentRead", [&](FrameGraphBuilder& builder) { builder.ReadTexture(lookup, FGUsage::SampledFragment); });
		DeclarePass(test.Graph, "ComputeRead", [&](FrameGraphBuilder& builder) { builder.ReadTexture(lookup, FGUsage::SampledCompute); });
		test.Graph.Compile(1);

		CDL_EXPECT(!FindBarrier(BarriersOf(test.Graph.GetCompiledGraph(), 1), lookup));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// With no barrier between the two reads, the second reader's stage has to be merged into the texture's state, or the
// write would wait for only one of them
CDL_TEST_CASE(FrameGraphCompile, WriteAfterSeveralReadsWaitsForEveryOne, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		NotePlanOnFailure dump(test.Graph);
		test.Graph.Reset(kOutputExtent);

		FGTextureHandle transient;
		DeclarePass(test.Graph, "Draw", [&](FrameGraphBuilder& builder)
			{
				transient = builder.CreateTexture("Transient", OutputSized(kColorFormat));
				builder.ColorAttachment(transient, FGLoad::Clear, kClearColor);
			});
		DeclarePass(test.Graph, "FragmentRead", [&](FrameGraphBuilder& builder) { builder.ReadTexture(transient, FGUsage::SampledFragment); });
		DeclarePass(test.Graph, "ComputeRead", [&](FrameGraphBuilder& builder) { builder.ReadTexture(transient, FGUsage::SampledCompute); });
		DeclarePass(test.Graph, "Redraw", [&](FrameGraphBuilder& builder) { builder.ColorAttachment(transient, FGLoad::Clear, kClearColor); });
		test.Graph.Compile(1);

		const FGBarrier* barrier = FindBarrier(BarriersOf(test.Graph.GetCompiledGraph(), 3), transient);
		CDL_CHECK(barrier);
		ExpectWaitsFor(barrier->FromState, {}, vk::PipelineStageFlagBits2::eFragmentShader | vk::PipelineStageFlagBits2::eComputeShader,
			"The redraw's barrier source");
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A shader samples it through a descriptor written with General, and the image has to be in that layout when it does
CDL_TEST_CASE(FrameGraphCompile, SampledReadUsesTheLayoutItsDescriptorWasWrittenWith, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		NotePlanOnFailure dump(test.Graph);
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle storage = test.ImportStorageTarget("Storage");
		DeclarePass(test.Graph, "Compute", [&](FrameGraphBuilder& builder) { builder.WriteTexture(storage, FGUsage::StorageCompute); });
		DeclarePass(test.Graph, "Scene", [&](FrameGraphBuilder& builder) { builder.ReadTexture(storage, FGUsage::SampledFragment); });
		test.Graph.Compile(1);

		const FGBarrier* barrier = FindBarrier(BarriersOf(test.Graph.GetCompiledGraph(), 1), storage);
		CDL_CHECK(barrier);
		CDL_EXPECT_EQ(barrier->ToState.Layout, vk::ImageLayout::eGeneral);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The cache writes every transient's descriptor with ShaderReadOnly
CDL_TEST_CASE(FrameGraphCompile, SampledReadOfATransientIsInShaderReadOnly, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		NotePlanOnFailure dump(test.Graph);
		test.Graph.Reset(kOutputExtent);

		FGTextureHandle transient;
		DeclarePass(test.Graph, "Draw", [&](FrameGraphBuilder& builder)
			{
				transient = builder.CreateTexture("Transient", OutputSized(kColorFormat));
				builder.ColorAttachment(transient, FGLoad::Clear, kClearColor);
			});
		DeclarePass(test.Graph, "Sample", [&](FrameGraphBuilder& builder) { builder.ReadTexture(transient, FGUsage::SampledFragment); });
		test.Graph.Compile(1);

		const FGBarrier* barrier = FindBarrier(BarriersOf(test.Graph.GetCompiledGraph(), 1), transient);
		CDL_CHECK(barrier);
		CDL_EXPECT_EQ(barrier->ToState.Layout, vk::ImageLayout::eShaderReadOnlyOptimal);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Nothing has to change layout, but skipping the barrier would let the read run before the compute writes land
CDL_TEST_CASE(FrameGraphCompile, ReadAfterAWriteInTheSameLayoutStillWaits, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		NotePlanOnFailure dump(test.Graph);
		test.Graph.Reset(kOutputExtent);

		const FGTextureHandle storage = test.ImportStorageTarget("Storage");
		DeclarePass(test.Graph, "Compute", [&](FrameGraphBuilder& builder) { builder.WriteTexture(storage, FGUsage::StorageCompute); });
		DeclarePass(test.Graph, "Fragment", [&](FrameGraphBuilder& builder) { builder.ReadTexture(storage, FGUsage::StorageFragment); });
		test.Graph.Compile(1);

		const FGBarrier* barrier = FindBarrier(BarriersOf(test.Graph.GetCompiledGraph(), 1), storage);
		CDL_CHECK(barrier);
		ExpectWaitsFor(barrier->FromState, vk::AccessFlagBits2::eShaderStorageWrite, vk::PipelineStageFlagBits2::eComputeShader, "The read's barrier source");
	}
	ExpectNoVulkanDebugMessages(logs);
}

CDL_TEST_CASE(FrameGraphCompile, TransientIsSizedFromItsSpecAndGetsARealImage, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		NotePlanOnFailure dump(test.Graph);
		test.Graph.Reset(kOutputExtent);

		FGTextureHandle half, fixed;
		DeclarePass(test.Graph, "Half", [&](FrameGraphBuilder& builder)
			{
				half = builder.CreateTexture("Half", OutputSized(kColorFormat, 0.5f));
				builder.ColorAttachment(half, FGLoad::Clear, kClearColor);
			});
		DeclarePass(test.Graph, "Fixed", [&](FrameGraphBuilder& builder)
			{
				fixed = builder.CreateTexture("Fixed", { .Size = { .Mode = FGSizeMode::Absolute, .Extent = { 3, 5 } }, .Format = kColorFormat });
				builder.ColorAttachment(fixed, FGLoad::Clear, kClearColor);
			});
		test.Graph.Compile(1);

		CDL_EXPECT(test.Graph.GetTexture(half).Desc.Extent == glm::uvec2(4, 2));
		CDL_EXPECT(test.Graph.GetTexture(fixed).Desc.Extent == glm::uvec2(3, 5));
		for (const FGTextureHandle transient : { half, fixed })
		{
			const FGTextureEntry& entry = test.Graph.GetTexture(transient);
			CDL_EXPECT_FALSE(entry.Imported);
			CDL_EXPECT_EQ(entry.Desc.Format, kColorFormat);
			CDL_EXPECT(entry.Image);
			CDL_EXPECT(entry.ImageView);
			CDL_EXPECT_NE(entry.CacheIndex, std::numeric_limits<uint32_t>::max());
		}
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Anything more makes the cache key differ from an identical texture's, and asks the driver for usage nothing needs
CDL_TEST_CASE(FrameGraphCompile, TransientUsageIsExactlyWhatItsPassesDeclared, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		NotePlanOnFailure dump(test.Graph);
		test.Graph.Reset(kOutputExtent);

		FGTextureHandle transient;
		DeclarePass(test.Graph, "Draw", [&](FrameGraphBuilder& builder)
			{
				transient = builder.CreateTexture("Transient", OutputSized(kColorFormat));
				builder.ColorAttachment(transient, FGLoad::Clear, kClearColor);
			});
		DeclarePass(test.Graph, "Sample", [&](FrameGraphBuilder& builder) { builder.ReadTexture(transient, FGUsage::SampledFragment); });
		test.Graph.Compile(1);

		const vk::ImageUsageFlags usage = test.Graph.GetTexture(transient).Desc.Usage;
		const vk::ImageUsageFlags expected = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
		CDL_EXPECT_MSG(usage == expected, std::format("usage is {}, expected {}", vk::to_string(usage), vk::to_string(expected)));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A depth format can't be a color attachment, so a usage that assumes color fails validation when the image is made
CDL_TEST_CASE(FrameGraphCompile, DepthTransientIsCreatedAsADepthImage, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		NotePlanOnFailure dump(test.Graph);
		test.Graph.Reset(kOutputExtent);

		FGTextureHandle depth;
		DeclarePass(test.Graph, "Scene", [&](FrameGraphBuilder& builder)
			{
				depth = builder.CreateTexture("Depth", OutputSized(kDepthFormat));
				builder.DepthAttachment(depth, FGLoad::Clear);
			});
		test.Graph.Compile(1);

		const FGTextureEntry& entry = test.Graph.GetTexture(depth);
		CDL_EXPECT(entry.Image);
		CDL_EXPECT(Covers(entry.Desc.Usage, vk::ImageUsageFlagBits::eDepthStencilAttachment));
		CDL_EXPECT_MSG(!(entry.Desc.Usage & vk::ImageUsageFlagBits::eColorAttachment), std::format("usage is {}", vk::to_string(entry.Desc.Usage)));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Both are alive at once, the blur reading one while it writes the other, so sharing an image would have it read what
// it writes
CDL_TEST_CASE(FrameGraphCompile, TransientsWithTheSameDescGetTheirOwnImages, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		NotePlanOnFailure dump(test.Graph);
		test.Graph.Reset(kOutputExtent);

		FGTextureHandle ping, pong;
		DeclarePass(test.Graph, "Draw", [&](FrameGraphBuilder& builder)
			{
				ping = builder.CreateTexture("Ping", OutputSized(kColorFormat));
				builder.ColorAttachment(ping, FGLoad::Clear, kClearColor);
			});
		DeclarePass(test.Graph, "Blur", [&](FrameGraphBuilder& builder)
			{
				pong = builder.CreateTexture("Pong", OutputSized(kColorFormat));
				builder.ReadTexture(ping, FGUsage::SampledFragment);
				builder.ColorAttachment(pong, FGLoad::Clear, kClearColor);
			});
		test.Graph.Compile(1);

		const FGTextureEntry& pingEntry = test.Graph.GetTexture(ping);
		const FGTextureEntry& pongEntry = test.Graph.GetTexture(pong);
		CDL_EXPECT(pingEntry.Image != pongEntry.Image);
		CDL_EXPECT_NE(pingEntry.CacheIndex, pongEntry.CacheIndex);
		CDL_EXPECT_NE(pingEntry.BindlessIndex, pongEntry.BindlessIndex);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The point of the cache: the same frame next frame costs no allocation
CDL_TEST_CASE(FrameGraphCompile, TransientKeepsItsImageFromFrameToFrame, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		NotePlanOnFailure dump(test.Graph);

		const FGTextureEntry first = test.Graph.GetTexture(RecordClearFrame(test.Graph, 1));
		const FGTextureEntry second = test.Graph.GetTexture(RecordClearFrame(test.Graph, 2));

		CDL_EXPECT(first.Image == second.Image);
		CDL_EXPECT(first.ImageView == second.ImageView);
		CDL_EXPECT_EQ(first.CacheIndex, second.CacheIndex);
		CDL_EXPECT_EQ(first.BindlessIndex, second.BindlessIndex);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Below the range are the pool's textures, so a shader handed a lower index samples one of them instead
CDL_TEST_CASE(FrameGraphCompile, TransientBindlessIndexIsInTheFrameGraphRange, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		NotePlanOnFailure dump(test.Graph);
		test.Graph.Reset(kOutputExtent);

		FGTextureHandle transient;
		DeclarePass(test.Graph, "Draw", [&](FrameGraphBuilder& builder)
			{
				transient = builder.CreateTexture("Transient", OutputSized(kColorFormat));
				builder.ColorAttachment(transient, FGLoad::Clear, kClearColor);
			});
		DeclarePass(test.Graph, "Sample", [&](FrameGraphBuilder& builder) { builder.ReadTexture(transient, FGUsage::SampledFragment); });
		test.Graph.Compile(1);

		const uint32_t index = test.Graph.GetTexture(transient).BindlessIndex;
		CDL_EXPECT_GE(index, kFirstFrameGraphSlot);
		CDL_EXPECT_LT(index, kMaxTextures);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Every frame in flight shares the cached image, as they share the Renderer's depth image, so the first barrier has to
// wait for last frame's writes or this frame's clear races them. Undefined is still fine: the clear replaces the contents.
CDL_TEST_CASE(FrameGraphCompile, TransientWaitsForLastFramesUseOfItsImage, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestFrameGraph test;
		NotePlanOnFailure dump(test.Graph);

		const vk::Image lastFrames = test.Graph.GetTexture(RecordClearFrame(test.Graph, 1)).Image;
		const FGTextureHandle transient = RecordClearFrame(test.Graph, 2);
		CDL_CHECK_MSG(test.Graph.GetTexture(transient).Image == lastFrames, "the cache has to reuse last frame's image for this to test anything");

		const FGBarrier* barrier = FindBarrier(BarriersOf(test.Graph.GetCompiledGraph(), 0), transient);
		CDL_CHECK(barrier);
		ExpectWaitsFor(barrier->FromState, vk::AccessFlagBits2::eColorAttachmentWrite, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			"This frame's first barrier source");
	}
	ExpectNoVulkanDebugMessages(logs);
}
