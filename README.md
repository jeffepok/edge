# Edge 26

Realistic next-gen football prototype, built in Unreal Engine 5.7 with C++.

## Status

Phase 1 — Playable Prototype. Milestones 1–5 implemented (project scaffold, physics ball, player character with Enhanced Input, broadcast camera, goal/game-mode/HUD).

## Quick start

See [`RUNBOOK.md`](RUNBOOK.md) for end-to-end instructions: first-time compile, asset creation in the editor, animation pipeline, and how to extend toward milestones 6 (AI) and 7 (animation polish).

## Source layout

```
Edge26/
├── Edge26.uproject              UE5 project descriptor
├── Config/                      DefaultEngine/Game/Input.ini
├── Content/                     (editor-created assets — empty in source)
└── Source/
    ├── Edge26.Target.cs         Game target
    ├── Edge26Editor.Target.cs   Editor target
    └── Edge26/                  Primary game module
        ├── Edge26.Build.cs
        ├── Public/
        │   ├── Edge26.h
        │   ├── Ball/SoccerBall.h
        │   ├── Camera/BroadcastSpringArmComponent.h
        │   ├── Game/{SoccerGameMode,SoccerHUD,GoalTrigger}.h
        │   └── Player/{FootballerCharacter,FootballerAnimInstance}.h
        └── Private/             Mirrored .cpp tree
```

## Engine version

UE **5.7** (`/Users/Shared/Epic Games/UE_5.7`). To switch versions, edit `EngineAssociation` in `Edge26.uproject`.

## C++ classes you will subclass in Blueprints

| C++ class | Suggested BP name | Purpose |
|---|---|---|
| `AFootballerCharacter` | `BP_Footballer` | Player + AI base; assign skeletal mesh, AnimBP, IMC/IA refs |
| `ASoccerBall` | `BP_SoccerBall` | Ball mesh, kick/bounce sounds |
| `AGoalTrigger` | `BP_GoalTrigger` | Goal mesh + trigger box per goal |
| `ASoccerGameMode` | `BP_SoccerGameMode` | Match defaults, level-specific overrides |
| `UFootballerAnimInstance` | `ABP_Footballer` | Animation Blueprint backing the Blend Spaces |
