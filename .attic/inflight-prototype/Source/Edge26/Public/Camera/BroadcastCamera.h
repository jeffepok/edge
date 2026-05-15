// Copyright Edge26. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BroadcastCamera.generated.h"

class UCameraComponent;
class ASoccerBall;

/**
 * FC-style "Tele Broadcast Wide" camera.
 *
 * Anchored to a touchline X/Z; lerps Y to follow the ball so the pitch always
 * runs left/right across the screen. Camera always looks at a point on the
 * opposite touchline at the same Y as itself (pure lateral pan, no roll).
 *
 * Locks the controlling PlayerController's ControlRotation to the camera's
 * yaw each tick so WASD becomes screen-relative — pressing W moves the player
 * away from the camera (up the screen) regardless of where the ball is.
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

	/** Touchline X (sideline distance, world cm) and Z (height, world cm). Y is overwritten by ball tracking. */
	UPROPERTY(EditAnywhere, Category = "Broadcast")
	FVector TouchlineAnchor = FVector(-3500.0f, 0.0f, 2500.0f);

	/** X coordinate the camera looks toward (centre of pitch width = 0). */
	UPROPERTY(EditAnywhere, Category = "Broadcast")
	float LookAtX = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Broadcast", meta = (ClampMin = "20.0", ClampMax = "120.0"))
	float FieldOfView = 70.0f;

	/** Higher = snappier ball follow. FC-style is loose: 2.0–4.0. */
	UPROPERTY(EditAnywhere, Category = "Broadcast", meta = (ClampMin = "0.1"))
	float TrackInterpSpeed = 3.0f;

	/** Become the active view target on BeginPlay. Disable if you want to swap cams from BP. */
	UPROPERTY(EditAnywhere, Category = "Broadcast")
	bool bAutoActivateOnBeginPlay = true;

	UPROPERTY(EditAnywhere, Category = "Broadcast", meta = (ClampMin = "0.0"))
	float ActivateBlendTime = 0.5f;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Broadcast", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(Transient)
	TWeakObjectPtr<ASoccerBall> CachedBall;

	float CurrentY = 0.0f;
};
