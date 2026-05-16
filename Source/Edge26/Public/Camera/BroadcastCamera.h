// Copyright Edge26. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BroadcastCamera.generated.h"

class UCameraComponent;
class ASoccerBallVisual;

/**
 * FC-style "Tele Broadcast Wide" camera.
 *
 * Anchored to a touchline X/Z; lerps Y to follow the ball so the pitch always
 * runs left/right across the screen. Camera always looks at a point on the
 * opposite touchline at the same Y as itself (pure lateral pan, no roll).
 *
 * Re-asserts itself as the player controller's view target each tick because
 * auto-switch's PC->Possess steals the view target to the new pawn. We don't
 * want that during M12 PIE soak — user wants to OBSERVE the AI playing.
 *
 * Auto-spawned by ASoccerGameMode if none is placed in the level.
 */
UCLASS()
class EDGE26_API ABroadcastCamera : public AActor
{
	GENERATED_BODY()

public:
	ABroadcastCamera();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Touchline X (sideline, world cm) and Z (height, world cm). Y is overwritten by ball tracking. */
	UPROPERTY(EditAnywhere, Category = "Broadcast")
	FVector TouchlineAnchor = FVector(0.0f, -4500.0f, 2500.0f);

	/** Y coordinate the camera looks toward (opposite touchline = +3400 if -Y anchor). */
	UPROPERTY(EditAnywhere, Category = "Broadcast")
	float LookAtY = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Broadcast", meta = (ClampMin = "20.0", ClampMax = "120.0"))
	float FieldOfView = 70.0f;

	/** Higher = snappier ball follow. FC-style is loose: 2.0–4.0. */
	UPROPERTY(EditAnywhere, Category = "Broadcast", meta = (ClampMin = "0.1"))
	float TrackInterpSpeed = 3.0f;

	/** Become the active view target on BeginPlay. */
	UPROPERTY(EditAnywhere, Category = "Broadcast")
	bool bAutoActivateOnBeginPlay = true;

	/** Force-re-assert as view target every tick (defends against auto-switch's Possess). */
	UPROPERTY(EditAnywhere, Category = "Broadcast")
	bool bForceViewTarget = true;

	UPROPERTY(EditAnywhere, Category = "Broadcast", meta = (ClampMin = "0.0"))
	float ActivateBlendTime = 0.5f;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Broadcast", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(Transient)
	TWeakObjectPtr<ASoccerBallVisual> CachedBall;

	float CurrentX = 0.0f;
};
