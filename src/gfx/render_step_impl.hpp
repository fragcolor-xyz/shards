#ifndef F5756E4F_1B24_4D04_801D_E6D0D0563099
#define F5756E4F_1B24_4D04_801D_E6D0D0563099

#include "renderer_cache.hpp"
#include "texture_cache.hpp"
#include "render_graph.hpp"

namespace gfx::detail {
void allocateNodeEdges(detail::RenderGraphBuilder &builder, RenderGraphBuilder::NodeBuildData &data,
                       const RenderFullscreenStep &step);
void setupRenderGraphNode(CachedRenderGraph &out, size_t index, const ViewData &viewData, const RenderFullscreenStep &step);

void allocateNodeEdges(detail::RenderGraphBuilder &builder, RenderGraphBuilder::NodeBuildData &data, const ClearStep &step);
void setupRenderGraphNode(CachedRenderGraph &out, size_t index, const ViewData &viewData, const ClearStep &step);

void allocateNodeEdges(detail::RenderGraphBuilder &builder, RenderGraphBuilder::NodeBuildData &data,
                       const RenderDrawablesStep &step);
void setupRenderGraphNode(CachedRenderGraph &out, size_t index, const ViewData &viewData, const RenderDrawablesStep &step);
} // namespace gfx::detail

#endif /* F5756E4F_1B24_4D04_801D_E6D0D0563099 */
