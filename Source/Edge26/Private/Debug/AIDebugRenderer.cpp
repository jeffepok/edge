// Copyright Edge26. All Rights Reserved.
#include "Debug/AIDebugRenderer.h"

#if !UE_BUILD_SHIPPING
#include "Adapter/SimHostSubsystem.h"
#include "AI/SpatialValueModel.h"
#include "Sim/WorldState.h"
#include "DrawDebugHelpers.h"

// Render-only helper: convert Fixed64 to float. Safe because this code
// never runs in the deterministic sim path.
// NOTE: Named Edge26FixedToFloat to avoid conflict with macOS FixMath.h macro.
static float Edge26FixedToFloat(edge26::Fixed64 v)
{
    return (float)v.Raw / (float)edge26::Fixed64::One;
}
#endif

AAIDebugRenderer::AAIDebugRenderer()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AAIDebugRenderer::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
#if !UE_BUILD_SHIPPING
    auto* Host = GetWorld() ? GetWorld()->GetSubsystem<USimHostSubsystem>() : nullptr;
    if (!Host) return;
    const edge26::FSimWorldState& s = Host->GetState();

    // --- Heatmap ---
    if (ActiveField != ESpatialFieldDebug::None)
    {
        const int teamPick = (TeamPerspective < 0) ? 0 : TeamPerspective;
        const int fieldIdx = (int)ActiveField - 1;   // None=0 in enum → shift
        const auto& cells = s.Spatial.Cells[teamPick][fieldIdx];

        // Compute max value for normalization (cheap; 1768 cells).
        int32 maxRaw = 1;
        for (int c = 0; c < edge26::kPitchCells; ++c)
            if (cells[c].Raw > maxRaw) maxRaw = cells[c].Raw;
        if (maxRaw < 1) maxRaw = 1;

        // kPitchCellsX=52, kPitchCellsY=34. Cell sizes from SpatialValueModel.h:
        // kCellSizeX_cm = 10500/52 ≈ 201 cm, kCellSizeY_cm = 6800/34 = 200 cm.
        const float cellW = (float)edge26::kCellSizeX_cm;
        const float cellH = (float)edge26::kCellSizeY_cm;
        // Pitch origin: world X in [-5250, 5250], Y in [-3400, 3400] (cm).
        const FVector pitchOrigin{ -5250.f, -3400.f, 5.f };

        for (int c = 0; c < edge26::kPitchCells; ++c)
        {
            int cx = c % edge26::kPitchCellsX;
            int cy = c / edge26::kPitchCellsX;
            FVector center{
                pitchOrigin.X + (cx + 0.5f) * cellW,
                pitchOrigin.Y + (cy + 0.5f) * cellH,
                pitchOrigin.Z
            };
            float v = (float)cells[c].Raw / (float)maxRaw;  // 0..1
            if (v < 0) v = 0;
            if (v > 1) v = 1;
            FColor col = FColor((uint8)(0), (uint8)(255 * v), (uint8)(0), 96);
            DrawDebugBox(GetWorld(), center,
                         FVector(cellW * 0.45f, cellH * 0.45f, 1.0f),
                         col, false, -1.f, 0, 1.f);
        }
    }
#endif
}
