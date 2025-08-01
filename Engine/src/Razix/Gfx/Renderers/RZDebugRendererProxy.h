#pragma once

// Modified from Source: https://github.com/jmorton06/Lumos/blob/main/Lumos/Source/Lumos/Graphics/Renderers/DebugRenderer.cpp // MIT License

#include "Razix/Core/RZSTL/smart_pointers.h"

#include "Razix/Gfx/Renderers/IRZRendererProxy.h"

#include "Razix/Gfx/RHI/API/RZIndexBuffer.h"
#include "Razix/Gfx/RHI/API/RZVertexBuffer.h"

#include "Razix/Math/AABB.h"
#include "Razix/Math/Frustum.h"

#include "Razix/Utilities/TRZSingleton.h"

namespace Razix {
    namespace Gfx {

        class RZLight;
        class RZDescriptorSet;

        struct Line
        {
            float3 p1;
            float3 p2;
            float4 col;

            Line(const float3& pos1, const float3& pos2, const float4& colour)
            {
                p1  = pos1;
                p2  = pos2;
                col = colour;
            }
        };

        struct Point
        {
            float3 p1;
            float4 col;
            float  size;

            Point(const float3& pos1, float s, const float4& colour)
            {
                p1   = pos1;
                size = s;
                col  = colour;
            }
        };

        /**
         * Draws debug geometry in the scene to help with visualization and debugging
         * 
         * Note: Uses Batched rendering to draw points/lines in a single draw call
         */
        class RAZIX_API RZDebugRendererProxy : public IRZRendererProxy, public RZSingleton<RZDebugRendererProxy>
        {
        public:
            //-------------------------------------------------------------
            // IRZRendererProxy

            void Init() override;
            void Begin(RZScene* scene) override;
            void Draw(RZDrawCommandBufferHandle cmdBuffer) override;
            void End() override;
            void Resize(u32 width, u32 height) override {}
            void Destroy() override;

            //-------------------------------------------------------------
            // Debug Draw Public API

            //Draw Point (very small circle)
            static void DrawPoint(const float3& pos, f32 point_radius, const float3& colour);
            static void DrawPoint(const float3& pos, f32 point_radius, const float4& colour = float4(1.0f, 1.0f, 1.0f, 1.0f));
            static void DrawPointDT(const float3& pos, f32 point_radius, const float3& colour);
            static void DrawPointDT(const float3& pos, f32 point_radius, const float4& colour = float4(1.0f, 1.0f, 1.0f, 1.0f));

            //Draw Line with a given thickness
            static void DrawThickLine(const float3& start, const float3& end, f32 line_width, const float3& colour);
            static void DrawThickLine(const float3& start, const float3& end, f32 line_width, const float4& colour = float4(1.0f, 1.0f, 1.0f, 1.0f));
            static void DrawThickLineDT(const float3& start, const float3& end, f32 line_width, const float3& colour);
            static void DrawThickLineDT(const float3& start, const float3& end, f32 line_width, const float4& colour = float4(1.0f, 1.0f, 1.0f, 1.0f));

            //Draw line with thickness of 1 screen pixel regardless of distance from camera
            static void DrawLine(const float3& start, const float3& end, const float3& colour);
            static void DrawLine(const float3& start, const float3& end, const float4& colour = float4(1.0f, 1.0f, 1.0f, 1.0f));
            static void DrawLineDT(const float3& start, const float3& end, const float3& colour);
            static void DrawLineDT(const float3& start, const float3& end, const float4& colour = float4(1.0f, 1.0f, 1.0f, 1.0f));

            static void DrawAABB(const Maths::AABB& box, const float4& edgeColour, bool cornersOnly = false, f32 width = 0.02f);
            static void DrawGrid(u32 dimension, const float4& colour);

            static void DrawLight(Gfx::RZLight* light, const float4& colour);
            static void DrawFrustum(const Maths::RZFrustum& frustum, const float4& colour);
            static void DrawFrustum(const float4x4& mat, const float4& colour);
            static void DrawCylinder(const float3& position, const float3& eulerRotation, float height, float radius, const float4& colour);
            static void DrawCapsule(const float3& position, const float3& eulerRotation, float height, float radius, const float4& colour);

            static void DrawArc(int numVerts, float radius, const float3& start, const float3& end, const float3& eulerRotation, const float4& colour);

            static void DrawSphere(f32 radius, const float3& position, const float4& colour);
            static void DrawCircle(int numVerts, f32 radius, const float3& position, const float3& eulerRotation, const float4& colour);
            static void DrawCone(int numCircleVerts, int numLinesToCircle, f32 angle, f32 length, const float3& position, const float3& rotation, const float4& colour);

        private:
            //Actual functions managing data parsing to save code bloat - called by public functions
            static void PopulatePointsDrawList(bool dt, const float3& pos, f32 point_radius, const float4& colour);
            static void PopulateThickLinesDrawList(bool dt, const float3& start, const float3& end, f32 line_width, const float4& colour);
            static void PopulateLinesDrawList(bool dt, const float3& start, const float3& end, const float4& colour);

        private:
            struct DebugDrawList
            {
                std::vector<Line>  m_DebugLines;
                std::vector<Point> m_DebugPoints;
                std::vector<Line>  m_DebugThickLines;
            };

            DebugDrawList m_DrawList;
            DebugDrawList m_DrawListNDT;

            RZPipelineHandle m_LinePipeline  = {};
            RZPipelineHandle m_PointPipeline = {};
            RZShaderHandle   m_LineShader    = {};
            RZShaderHandle   m_PointShader   = {};

            u32                  m_LineIndexCount  = 0;
            RZIndexBufferHandle  m_LineIB          = {};
            RZVertexBufferHandle m_LinePosition_VB = {};
            RZVertexBufferHandle m_LineColor_VB    = {};

            u32                  m_PointIndexCount  = 0;
            RZIndexBufferHandle  m_PointIB          = {};
            RZVertexBufferHandle m_PointPosition_VB = {};
            RZVertexBufferHandle m_PointColor_VB    = {};

        private:
            void createPipelines();
            void createPointBufferResources();
            void createLineBufferResources();
        };
    }    // namespace Gfx
}    // namespace Razix
