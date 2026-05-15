// Copyright Edge26. All Rights Reserved.
#include "Debug/AIDebugRenderer.h"

AAIDebugRenderer::AAIDebugRenderer()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AAIDebugRenderer::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
#if !UE_BUILD_SHIPPING
    // Filled in T10.2/T10.3/T10.4.
#endif
}
