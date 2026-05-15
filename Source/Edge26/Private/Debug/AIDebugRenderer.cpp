// Copyright Edge26. All Rights Reserved.
#include "Debug/AIDebugRenderer.h"

#if !UE_BUILD_SHIPPING
#include "Adapter/SimHostSubsystem.h"
#include "AI/SpatialValueModel.h"
#include "AI/Intents.h"
#include "AI/Roles.h"
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

    // --- Intent Arrows ---
    if (bShowIntentArrows)
    {
        static const FColor IntentColors[(int)edge26::EIntent::Count] = {
            FColor::Yellow,     // HoldPosition
            FColor::Green,      // MakeRunForward
            FColor::Cyan,       // DropToReceive
            FColor::Blue,       // ProvideWidth
            FColor::Red,        // Press
            FColor::Orange,     // TrackRunner
            FColor::Purple,     // HoldDefensiveLine
            FColor(255,200,0),  // Pass
            FColor(255,80,80),  // Shoot
            FColor(180,100,255),// Dribble
            FColor::White,      // Hold
            FColor(255,140,0),  // Clear
        };
        static const TCHAR* IntentNames[(int)edge26::EIntent::Count] = {
            TEXT("Hold"), TEXT("RunFwd"), TEXT("Drop"), TEXT("Width"),
            TEXT("Press"), TEXT("Track"), TEXT("Line"),
            TEXT("Pass"), TEXT("Shoot"), TEXT("Drib"), TEXT("Hold"), TEXT("Clear"),
        };
        static const TCHAR* RoleNames[(int)edge26::ERole::Count] = {
            TEXT("GK"), TEXT("CB"), TEXT("FBL"), TEXT("FBR"),
            TEXT("CDM"), TEXT("CM"), TEXT("CAM"),
            TEXT("WL"), TEXT("WR"), TEXT("ST")
        };

        for (int i = 0; i < edge26::kSimPlayerCount; ++i)
        {
            const auto& p = s.Players[i];
            FVector from{
                Edge26FixedToFloat(p.Position.X),
                Edge26FixedToFloat(p.Position.Y),
                120.f
            };
            FVector to{
                Edge26FixedToFloat(p.AITargetPosition.X),
                Edge26FixedToFloat(p.AITargetPosition.Y),
                120.f
            };
            const uint8 ix = p.CurrentIntent;
            FColor col = (ix < (uint8)edge26::EIntent::Count) ? IntentColors[ix] : FColor::Black;
            DrawDebugDirectionalArrow(GetWorld(), from, to, 60.f, col, false, -1.f, 0, 2.f);

            FString label = FString::Printf(TEXT("%d %s %s"),
                i,
                (p.RoleId < (uint8)edge26::ERole::Count) ? RoleNames[p.RoleId] : TEXT("?"),
                (ix < (uint8)edge26::EIntent::Count)     ? IntentNames[ix]     : TEXT("?"));
            DrawDebugString(GetWorld(),
                from + FVector(0, 0, 60),
                label, nullptr, col, 0.0f, true);
        }
    }

    // --- Offside Lines ---
    if (bShowOffsideLines)
    {
        // Pitch half-width = 34m = 3400 cm.
        const float halfWidthCm = 3400.f;
        for (int team = 0; team < 2; ++team)
        {
            float xCm = Edge26FixedToFloat(s.Match.OffsideLineY[team]);
            FVector a{ xCm, -halfWidthCm, 30.f };
            FVector b{ xCm,  halfWidthCm, 30.f };
            FColor col = (team == 0) ? FColor::Cyan : FColor::Red;
            DrawDebugLine(GetWorld(), a, b, col, false, -1.f, 0, 6.f);
        }
    }
#endif
}
