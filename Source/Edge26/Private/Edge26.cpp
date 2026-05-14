// Copyright Edge26. All Rights Reserved.

#include "Edge26.h"

DEFINE_LOG_CATEGORY(LogEdge26);

void FEdge26Module::StartupModule()
{
	UE_LOG(LogEdge26, Log, TEXT("Edge26 module loaded"));
}

void FEdge26Module::ShutdownModule()
{
	UE_LOG(LogEdge26, Log, TEXT("Edge26 module unloaded"));
}

IMPLEMENT_PRIMARY_GAME_MODULE(FEdge26Module, Edge26, "Edge26");
