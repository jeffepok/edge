// Copyright Edge26. All Rights Reserved.

#include "Pitch/PitchMarkings.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Game/GoalTrigger.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Edge26.h"

APitchMarkings::APitchMarkings()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Lines = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Lines"));
	Lines->SetupAttachment(Root);
	Lines->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Lines->SetGenerateOverlapEvents(false);
	Lines->CastShadow = false;
	Lines->SetMobility(EComponentMobility::Movable);

	// Engine basic cube — the unit-size primitive we'll scale into each line/spot.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube"));
	if (CubeFinder.Succeeded())
	{
		LineMesh = CubeFinder.Object;
	}

	// Engine basic-shape material has a "Color" parameter we MID-set per actor.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
	if (MatFinder.Succeeded())
	{
		LineMaterial = MatFinder.Object;
	}
}

void APitchMarkings::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// OnConstruction runs in editor (on placement, on property edit, on level load) AND
	// at runtime (on spawn). Building here means lines exist as soon as the actor is
	// placed in the editor — no Play needed — and they get baked into the .umap save.
	if (!Lines || !LineMesh)
	{
		return;
	}

	Lines->ClearInstances(); // editor edits & reload re-run OnConstruction; avoid stacking.
	Lines->SetStaticMesh(LineMesh);

	if (LineMaterial)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(LineMaterial, this);
		MID->SetVectorParameterValue(TEXT("Color"), LineColor);
		Lines->SetMaterial(0, MID);
	}

	BuildMarkings();
}

void APitchMarkings::BeginPlay()
{
	Super::BeginPlay();

	// Lines were already built in OnConstruction — only do runtime-only work here.
	if (bAlignGoalsToMarkings)
	{
		AlignGoals();
	}
}

void APitchMarkings::AlignGoals()
{
	const float HalfL = PitchLength * 0.5f;
	int32 Aligned = 0;

	for (TActorIterator<AGoalTrigger> It(GetWorld()); It; ++It)
	{
		AGoalTrigger* Goal = *It;
		if (!Goal) { continue; }

		// DefendingTeamId 0 -> -Y end goal line, 1 -> +Y end goal line.
		// Push GoalBackOffset behind the line so the trigger box's front face sits on the line.
		const float Sign = (Goal->DefendingTeamId == 0) ? -1.0f : +1.0f;
		const FVector NewLocation(0.0f, Sign * (HalfL + GoalBackOffset), Goal->GetActorLocation().Z);

		// Rotation untouched on purpose — the BP_GoalTrigger frame mesh facing is set up
		// in the Blueprint and we don't know the convention from C++. Adjust manually if
		// the frame ends up backwards.
		Goal->SetActorLocation(NewLocation, false, nullptr, ETeleportType::ResetPhysics);
		++Aligned;
		UE_LOG(LogEdge26, Log, TEXT("Aligned %s (defends team %d) to %s"),
			*Goal->GetName(), Goal->DefendingTeamId, *NewLocation.ToString());
	}

	UE_LOG(LogEdge26, Log, TEXT("PitchMarkings aligned %d goal(s) to goal lines."), Aligned);
}

void APitchMarkings::BuildMarkings()
{
	const float HalfL = PitchLength * 0.5f;
	const float HalfW = PitchWidth * 0.5f;
	const float Z = LineHeightOffset;
	const float H = LineProfileHeight;

	// Outer boundary: touchlines (along Y at X=±HalfW), goal lines (along X at Y=±HalfL).
	AddLineSegment(FVector(+HalfW, 0.0f, Z), FVector(LineThickness, PitchLength, H));
	AddLineSegment(FVector(-HalfW, 0.0f, Z), FVector(LineThickness, PitchLength, H));
	AddLineSegment(FVector(0.0f, +HalfL, Z), FVector(PitchWidth,    LineThickness, H));
	AddLineSegment(FVector(0.0f, -HalfL, Z), FVector(PitchWidth,    LineThickness, H));

	// Halfway line.
	AddLineSegment(FVector(0.0f, 0.0f, Z), FVector(PitchWidth, LineThickness, H));

	// Centre circle and centre spot.
	AddCircle(FVector(0.0f, 0.0f, Z), CenterCircleRadius);
	AddDot(FVector(0.0f, 0.0f, Z), SpotSize);

	// Per-end markings: penalty area + goal area + penalty spot.
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float Sign = (Side == 0) ? +1.0f : -1.0f;
		const float GoalLineY = Sign * HalfL;

		// Penalty area
		const float PA_FarY = GoalLineY - Sign * PenaltyAreaDepth;
		const float PA_SideX = PenaltyAreaWidth * 0.5f;
		AddLineSegment(FVector(0.0f, PA_FarY, Z), FVector(PenaltyAreaWidth, LineThickness, H));
		AddLineSegment(FVector(+PA_SideX, (GoalLineY + PA_FarY) * 0.5f, Z), FVector(LineThickness, PenaltyAreaDepth, H));
		AddLineSegment(FVector(-PA_SideX, (GoalLineY + PA_FarY) * 0.5f, Z), FVector(LineThickness, PenaltyAreaDepth, H));

		// Goal area
		const float GA_FarY = GoalLineY - Sign * GoalAreaDepth;
		const float GA_SideX = GoalAreaWidth * 0.5f;
		AddLineSegment(FVector(0.0f, GA_FarY, Z), FVector(GoalAreaWidth, LineThickness, H));
		AddLineSegment(FVector(+GA_SideX, (GoalLineY + GA_FarY) * 0.5f, Z), FVector(LineThickness, GoalAreaDepth, H));
		AddLineSegment(FVector(-GA_SideX, (GoalLineY + GA_FarY) * 0.5f, Z), FVector(LineThickness, GoalAreaDepth, H));

		// Penalty spot
		const float SpotY = GoalLineY - Sign * PenaltySpotDistance;
		AddDot(FVector(0.0f, SpotY, Z), SpotSize);
	}
}

void APitchMarkings::AddLineSegment(const FVector& LocalCenter, const FVector& Size, float YawDeg)
{
	// Engine cube is 100x100x100 cm; scale by Size/100 for desired dimensions.
	FTransform InstanceXform;
	InstanceXform.SetLocation(LocalCenter);
	InstanceXform.SetRotation(FRotator(0.0f, YawDeg, 0.0f).Quaternion());
	InstanceXform.SetScale3D(FVector(Size.X / 100.0f, Size.Y / 100.0f, Size.Z / 100.0f));
	Lines->AddInstance(InstanceXform);
}

void APitchMarkings::AddCircle(const FVector& Center, float Radius)
{
	const int32 N = FMath::Max(8, CircleSegments);
	const float StepRad = (2.0f * PI) / N;
	// Each chord length, plus a 5% overlap so adjacent segments butt together cleanly.
	const float SegLength = 2.0f * Radius * FMath::Sin(StepRad * 0.5f) * 1.05f;

	for (int32 i = 0; i < N; ++i)
	{
		const float MidAngle = (i + 0.5f) * StepRad;
		const FVector SegCenter = Center + FVector(FMath::Cos(MidAngle) * Radius,
		                                           FMath::Sin(MidAngle) * Radius,
		                                           0.0f);
		// Segment's long axis (local +Y) must align with the tangent direction
		// (-sin θ, cos θ). Yaw on local +Y produces world (-sin yaw, cos yaw),
		// so yaw = θ (in degrees), not θ + 90.
		const float SegmentYaw = FMath::RadiansToDegrees(MidAngle);
		AddLineSegment(SegCenter, FVector(LineThickness, SegLength, LineProfileHeight), SegmentYaw);
	}
}

void APitchMarkings::AddDot(const FVector& Center, float Size)
{
	AddLineSegment(Center, FVector(Size, Size, LineProfileHeight));
}
