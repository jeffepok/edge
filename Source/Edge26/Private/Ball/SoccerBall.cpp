// Copyright Edge26. All Rights Reserved.

#include "Ball/SoccerBall.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Edge26.h"

ASoccerBall::ASoccerBall()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(Radius);
	Collision->SetCollisionProfileName(TEXT("PhysicsActor"));
	Collision->SetSimulatePhysics(true);
	Collision->SetEnableGravity(true);
	Collision->SetLinearDamping(LinearDamping);
	Collision->SetAngularDamping(AngularDamping);
	Collision->SetMassOverrideInKg(NAME_None, MassKg, true);
	Collision->SetNotifyRigidBodyCollision(true);
	// CCD off by default. Small light dynamic bodies under CCD can produce
	// explosive depenetration impulses when struck by kinematic capsules.
	// Tunneling is rare at our kick speeds; toggle on per-BP if it appears.
	Collision->BodyInstance.bUseCCD = false;
	Collision->BodyInstance.PositionSolverIterationCount = 8;
	Collision->BodyInstance.VelocitySolverIterationCount = 4;
	Collision->BodyInstance.SleepFamily = ESleepFamily::Sensitive;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);

	AudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComp"));
	AudioComp->SetupAttachment(Collision);
	AudioComp->bAutoActivate = false;
}

void ASoccerBall::BeginPlay()
{
	Super::BeginPlay();

	Collision->SetSphereRadius(Radius, true);
	Collision->SetLinearDamping(LinearDamping);
	Collision->SetAngularDamping(AngularDamping);
	Collision->SetMassOverrideInKg(NAME_None, MassKg, true);

	UE_LOG(LogEdge26, Log, TEXT("Ball ready @ %s, mass=%.2fkg, r=%.1fcm"), *GetActorLocation().ToString(), MassKg, Radius);
}

void ASoccerBall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Out-of-bounds recovery: tunneling through the pitch or flying past stadium
	// bounds shouldn't lose the ball. Snap back to center.
	const FVector Loc = GetActorLocation();
	if (Loc.Z < -300.0f || FMath::Abs(Loc.X) > 8000.0f || FMath::Abs(Loc.Y) > 12000.0f)
	{
		UE_LOG(LogEdge26, Warning, TEXT("Ball OOB at %s — resetting to center."), *Loc.ToString());
		ResetTo(FVector(0.0f, 0.0f, 30.0f));
		return;
	}

	FVector Vel = Collision->GetPhysicsLinearVelocity();

	// Horizontal cap — kicks travel along the ground, anything past this is unphysical.
	const FVector Vel2D = FVector(Vel.X, Vel.Y, 0.0f);
	const float Speed2D = Vel2D.Size();
	if (Speed2D > MaxSpeed2D)
	{
		const FVector Clamped = Vel2D.GetSafeNormal() * MaxSpeed2D;
		Vel = FVector(Clamped.X, Clamped.Y, Vel.Z);
		Collision->SetPhysicsLinearVelocity(Vel);
	}

	// Total speed cap — catches vertical-launch glitches the 2D cap misses.
	const float Speed3D = Vel.Size();
	if (Speed3D > MaxSpeed3D)
	{
		Collision->SetPhysicsLinearVelocity(Vel.GetSafeNormal() * MaxSpeed3D);
	}
}

void ASoccerBall::ApplyKick(const FVector& WorldDirection, float Strength, float LiftRatio, const FVector& SpinAxis, float SpinMagnitude)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastKickTime < KickCooldown)
	{
		return;
	}
	LastKickTime = Now;

	FVector Dir = WorldDirection;
	Dir.Z = 0.0f;
	if (!Dir.Normalize())
	{
		return;
	}

	const float ClampedLift = FMath::Clamp(LiftRatio, 0.0f, 1.0f);
	// Decompose: horizontal component shrinks as lift grows, but total energy is preserved.
	const float HorizScale = FMath::Sqrt(FMath::Max(1.0f - ClampedLift * ClampedLift, KINDA_SMALL_NUMBER));
	const FVector TargetVel = Dir * (Strength * HorizScale) + FVector::UpVector * (Strength * ClampedLift);

	// Replace existing velocity rather than adding — kicks are decisive, not additive.
	Collision->SetPhysicsLinearVelocity(TargetVel);

	if (!SpinAxis.IsNearlyZero() && SpinMagnitude > KINDA_SMALL_NUMBER)
	{
		Collision->SetPhysicsAngularVelocityInRadians(SpinAxis.GetSafeNormal() * SpinMagnitude);
	}
	else
	{
		// Natural rolling spin: angular velocity perpendicular to motion.
		const FVector RollAxis = FVector::CrossProduct(FVector::UpVector, Dir).GetSafeNormal();
		const float AngSpeed = (Strength * HorizScale) / FMath::Max(Radius, 1.0f);
		Collision->SetPhysicsAngularVelocityInRadians(RollAxis * AngSpeed * 0.6f);
	}

	if (KickSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, KickSound, GetActorLocation());
	}
}

void ASoccerBall::ResetTo(const FVector& WorldLocation)
{
	Collision->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Collision->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	SetActorLocation(WorldLocation, false, nullptr, ETeleportType::ResetPhysics);
	LastKickTime = -100.0f;
}

float ASoccerBall::GetSpeed2D() const
{
	const FVector V = Collision->GetPhysicsLinearVelocity();
	return FVector(V.X, V.Y, 0.0f).Size();
}

FVector ASoccerBall::GetVelocity2D() const
{
	const FVector V = Collision->GetPhysicsLinearVelocity();
	return FVector(V.X, V.Y, 0.0f);
}

void ASoccerBall::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

	// Pawn-bump clamp: physical contact with a pawn capsule must never launch the
	// ball faster than ~120% of the pawn's speed. Real kicks go through ApplyKick;
	// any "kick" produced by raw collision is a depenetration glitch and gets
	// suppressed here. Without this, glancing capsule hits send the ball orbital.
	//
	// EXCEPTION: if a real kick was just applied (within KickInFlightWindow), the
	// ball is in genuine flight — pawns running through the kick path must not
	// dampen its velocity. Otherwise the AI standing between player and goal
	// neutralises every shot by passive collision.
	if (const APawn* OtherPawn = Cast<APawn>(Other))
	{
		const float TimeSinceKick = GetWorld()->GetTimeSeconds() - LastKickTime;
		const bool bRecentlyKicked = TimeSinceKick < KickInFlightWindow;

		if (!bRecentlyKicked)
		{
			const float PawnSpeed = OtherPawn->GetVelocity().Size();
			const float MaxBumpSpeed = FMath::Max(PawnSpeed * PawnBumpSpeedMult, PawnBumpMinSpeed);

			const FVector PostVel = Collision->GetPhysicsLinearVelocity();
			const float PostSpeed = PostVel.Size();
			if (PostSpeed > MaxBumpSpeed)
			{
				const FVector Clamped = PostVel.GetSafeNormal() * MaxBumpSpeed;
				Collision->SetPhysicsLinearVelocity(Clamped);
			}
		}
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastBounceTime < 0.05f)
	{
		return;
	}
	LastBounceTime = Now;

	if (BounceSound && NormalImpulse.SizeSquared() > 100.0f * 100.0f)
	{
		const float Vol = FMath::Clamp(NormalImpulse.Size() / 4000.0f, 0.1f, 1.0f);
		UGameplayStatics::PlaySoundAtLocation(this, BounceSound, HitLocation, Vol);
	}
}
