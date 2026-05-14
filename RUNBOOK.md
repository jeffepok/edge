# Edge 26 — Runbook

This document is the editor-side companion to the C++ source. Follow it top-to-bottom for the first run; later sessions only need the relevant section.

> **Convention**: Anything tagged **[Editor]** must be done with the UE5 editor open. **[CLI]** can be done from a terminal.

---

## 1. First-time compile

### 1.1 Generate the IDE project [CLI]

```bash
cd /Users/jeffersonaddai-poku/Desktop/projects/Games/Edge26
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/GenerateProjectFiles.sh" \
  -project="$PWD/Edge26.uproject" -game -engine
```

This creates `Edge26.xcworkspace`. (Rider users: use Rider's "Open .uproject" instead — it triggers the same step.)

### 1.2 First build [CLI or Xcode]

Either build inside Xcode (target **Edge26Editor**, scheme **My Mac**) or from CLI:

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
  Edge26Editor Mac Development -project="$PWD/Edge26.uproject" -waitmutex
```

Expect 5–15 minutes for a clean build. Subsequent builds are incremental (~10–60s).

### 1.3 Open the editor [Editor]

```bash
open Edge26.uproject
```

If the editor offers to "build missing modules", click **Yes**. The first launch also runs shader compilation (background, watch the toolbar progress).

---

## 2. Create the gameplay assets in the editor [Editor]

The C++ classes are abstract scaffolding. You need editor-created assets that subclass them so meshes, sounds, and animations can be assigned.

### 2.1 Folder layout

In the **Content Browser**, create:

```
Content/
├── Levels/                  L_Pitch (the playable level)
├── Input/                   IMC + IA assets
├── Blueprints/
│   ├── Game/                BP_SoccerGameMode, BP_GoalTrigger
│   ├── Ball/                BP_SoccerBall
│   └── Player/              BP_Footballer, ABP_Footballer (anim BP)
├── Characters/              Imported skeletal meshes / Mixamo anims
└── Audio/                   Crowd, kicks, whistle, footsteps
```

### 2.2 Input Mapping Context + Input Actions

For each action, **right-click → Input → Input Action**. Recommended setup:

| Asset | Value type | Mapped to |
|---|---|---|
| `IA_Move` | Axis2D (Vector2D) | WASD (composite) + Gamepad Left Stick 2D |
| `IA_Look` | Axis2D | Mouse XY + Gamepad Right Stick 2D |
| `IA_Sprint` | Bool | Left Shift + Gamepad Left Shoulder |
| `IA_Pass` | Bool | Space + Gamepad Face Button Bottom (A/Cross) |
| `IA_Shoot` | Bool | F + Gamepad Face Button Right (B/Circle) |
| `IA_Chip` | Bool | Q + Gamepad Face Button Top (Y/Triangle) |

Then create `IMC_Player` (Input Mapping Context) and add each `IA_*` with the appropriate keys + modifiers (`Negate Y` on `IA_Move` for the W key composite, `Swizzle XY` on stick if needed).

### 2.3 BP_Footballer (Player Blueprint)

1. Right-click in `Content/Blueprints/Player` → **Blueprint Class** → search for `FootballerCharacter` → name it `BP_Footballer`.
2. Open it. In **Class Defaults**, set:
   - `Default Mapping Context` → `IMC_Player`.
   - `IA_Move`, `IA_Look`, `IA_Sprint`, `IA_Pass`, `IA_Shoot`, `IA_Chip` → the assets you just created.
3. Select the `Mesh` component → assign a skeletal mesh. **Quick option**: enable `Engine/Plugins/Animation/AnimContent → Manny/Quinn` from the Add Content browser; or use a Mixamo upload (see §4).
4. Anim Class → `ABP_Footballer` (created next).

### 2.4 BP_SoccerBall

1. Blueprint Class → `SoccerBall` → `BP_SoccerBall`.
2. `Mesh` component → assign a sphere static mesh (Engine basic shapes). Scale uniformly so the visual matches the 11cm collision radius (default cube is 50cm so set scale `0.22` for a 1m sphere — adjust to taste).
3. Optional: assign `KickSound` and `BounceSound`.

### 2.5 BP_GoalTrigger

1. Blueprint Class → `GoalTrigger` → `BP_GoalTrigger`.
2. Place two instances in the level — one per goal — and on each set `Defending Team Id` to `0` and `1`.
3. Optionally add nets/posts as static mesh children.

### 2.6 BP_SoccerGameMode

1. Blueprint Class → `SoccerGameMode` → `BP_SoccerGameMode`.
2. **Class Defaults**: `Default Pawn Class` → `BP_Footballer`. `HUD Class` → `ASoccerHUD` (or a subclass).
3. Edit `Edge26.uproject`'s World Settings? — better: in **Project Settings → Maps & Modes → Default GameMode**, point at `BP_SoccerGameMode`.

### 2.7 ABP_Footballer (Animation Blueprint)

1. Right-click → **Animation → Animation Blueprint**.
2. Parent class: `FootballerAnimInstance` (the C++ base — gives you `Speed`, `RelativeDirection`, `LeanAngle`, etc., for free).
3. Target Skeleton: whichever skeleton your mesh uses.
4. In **AnimGraph**, create a state machine: `Idle / Locomotion / In Air / Kick`.
5. The `Locomotion` state pipes into a 1D **Blend Space** keyed on `Speed` (Idle 0 → Walk 220 → Jog 500 → Sprint 820). For directional locomotion, switch to a 2D Blend Space on `(ForwardSpeed, RightSpeed)` or `(Speed, RelativeDirection)`.
6. Add a `Lean` modifier in Anim Graph: `Modify Bone → spine_03`, Roll = `LeanAngle * 0.5`, axis = additive.

---

## 3. Build the level [Editor]

### 3.1 Create `L_Pitch`

1. **File → New Level → Open World** (or Empty if you want to keep memory low).
2. Save as `Content/Levels/L_Pitch`.
3. Add a basic **Plane** static mesh, scale `(60, 90, 1)` for a roughly 60m × 90m miniature pitch (UE units = cm — scale 1.0 = 100cm).
4. Apply a green grass material (Engine starter content has `M_Grass_Tile` if you enabled the starter content; otherwise create a simple `M_Pitch` with a grass texture).
5. Add **Directional Light** (sun), **Sky Light**, **Sky Atmosphere**, **Volumetric Cloud**, **Exponential Height Fog**, **Post Process Volume** (Unbound). Lumen runs by default with the renderer settings already in `DefaultEngine.ini`.
6. Drop two `BP_GoalTrigger` instances at each end (~45m from center along Y), facing inward. Set `DefendingTeamId` 0 and 1.
7. Drop one `BP_SoccerBall` near the center spot.
8. Drop two `BP_Footballer` instances + two `Player Start` actors.
9. **World Settings** → set `GameMode Override` → `BP_SoccerGameMode` (only needed if not set globally).

### 3.2 Verify

Hit **Play in Editor**. You should see:

- HUD scoreline with `HOM 0 - 0 AWY` and a `KICKOFF` banner.
- Camera follows the controlled footballer at a high pitch.
- WASD moves; Shift sprints; Space passes the ball.
- Driving the ball into the goal triggers `GOAL!` and a kickoff reset.

If movement feels slidey: confirm `BP_Footballer`'s `Character Movement` component has `Orient Rotation To Movement = true` and `Use Controller Desired Rotation = false`.

---

## 4. Animations (recommended path)

The C++ already exposes everything an AnimBP needs. The work is editor-only.

### 4.1 Source

- **Mixamo** (free, fastest path): https://www.mixamo.com — upload a base mesh once, then bulk-download FBX with skin. Search: `idle`, `running`, `sprinting`, `walk`, `soccer kick`, `slide tackle`, `running stop`, `quick turn`.
- **Unreal Marketplace**: search `football mocap` or `sports animations`. Quality is higher but mostly paid.
- **Rokoko** free pack: useful for body language.

### 4.2 Retargeting (UE5 IK Rig)

1. Import the Mixamo FBX with **Skeleton: <create new>** the first time. Subsequent imports reuse the same skeleton.
2. Create an **IK Rig** for the Mixamo skeleton (`IKR_Mixamo`).
3. Create an **IK Rig** for your target skeleton (e.g. `IKR_Manny` if using UE5 Mannequin).
4. Create an **IK Retargeter** linking source → target.
5. **Batch Retarget Animations** → output to `Content/Characters/Anims/`.

### 4.3 Wire into ABP_Footballer

- Replace the Idle/Walk/Run nodes inside the Blend Space with the retargeted clips.
- Add a `Kick` montage and call `PlayAnimMontage` from `BP_Footballer`'s `OnKick` event (you can also expose a `BlueprintImplementableEvent` from the C++ if you want it called automatically — currently `ExecuteKick` only fires the ball; a follow-up commit could call a montage by name).

### 4.4 Foot planting / IK (later)

Use **Control Rig** or a **Foot IK** node in the AnimBP that traces down per foot socket and modifies effector locations to match terrain. Set up basic floor IK first, refine later.

---

## 5. What's NOT done — milestones 6 and 7

### 5.1 Milestone 6 — Opponent AI

Create `Source/Edge26/Public/AI/OpponentAIController.h` extending `AAIController`. Skeleton plan:

1. **Blackboard keys**: `BallActor` (Object), `BallLocation` (Vector), `bHasBall` (Bool), `OwnTeamId` (Int), `TargetGoal` (Vector).
2. **Behavior Tree**: top-level Selector
   - **HasBall** → Sequence: `MoveTo TargetGoal` → `Shoot` (call `ApplyKick` directly via a custom BTTask).
   - **BallNearby** (range < 200) → `BTTask_Tackle` (sphere overlap, push toward ball, possibly set bHasBall).
   - **Default** → `MoveTo BallLocation`.
3. **Service**: tick every 0.2s — refresh `BallLocation` from the world's `ASoccerBall`.
4. **Spawn**: `BP_OpponentFootballer` subclass of `BP_Footballer`. Its AI Controller Class = `BP_OpponentAIController`.

### 5.2 Milestone 7 — Animation polish

Once the AnimBP exists, the next quality jumps are:

- **Strafe / directional locomotion** via 2D Blend Space on `(ForwardSpeed, RightSpeed)` instead of 1D.
- **Distance Matching** for stop animations — eliminates foot sliding when decelerating.
- **Turn-in-place** state driven by `YawRatePerSec` when `Speed < 50`.
- **Foot IK** with `Two Bone IK` nodes per foot, traced against the pitch.
- **Additive lean** layer driven by `LeanAngle` modifying `spine_03` roll.
- **Kick montages** with hit windows (`AnimNotify_Kick`) that call `ApplyKick` precisely on foot contact rather than on input press.

### 5.3 Visual polish (parallel track)

- **Lumen GI** is on by default — tune via Post Process Volume (Exposure mode = Manual, Bloom intensity ~0.6, AutoExposure if using Manual disable).
- **Volumetric Fog** at the stadium ground level for atmosphere.
- **Crowd**: low-poly impostor planes around the pitch, animated via Niagara or a static texture array.
- **Grass shader**: tessellated WPO grass blades using `M_Grass_Master` from Lyra/Megascans, or a Niagara grass component.

---

## 6. Useful CLI reference

```bash
# Cook + run packaged build (after Content is set up):
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/RunUAT.sh" \
  BuildCookRun -project="$PWD/Edge26.uproject" \
  -platform=Mac -clientconfig=Development \
  -build -cook -stage -package -archive \
  -archivedirectory="$PWD/Packaged"

# Quick standalone editor with logs:
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" \
  "$PWD/Edge26.uproject" -log

# Run automation tests headless:
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" \
  "$PWD/Edge26.uproject" -ExecCmds="Automation RunTests Edge26" -unattended -nopause
```

---

## 7. Common pitfalls

| Symptom | Likely cause | Fix |
|---|---|---|
| Player won't move | No `IMC_Player` assigned on `BP_Footballer` | Set `Default Mapping Context` in class defaults |
| Camera clips into player | Spring arm too short | Increase `Base Arm Length` on `BP_Footballer`'s SpringArm |
| Ball passes through player | Player has no capsule collision or wrong profile | Verify `CapsuleComponent` profile is `Pawn` |
| Goal triggers immediately on kickoff | Trigger overlapping ball spawn | Move trigger box behind goal line; `BallSpawnLocation` away from box |
| AnimBP shows T-pose | Skeleton mismatch or Anim Class not set | Confirm `BP_Footballer.Mesh.Anim Class = ABP_Footballer` and skeletons match |
| Compile fails on `IWYUSupport` | Older engine version | Confirm engine is 5.5+; remove `IWYUSupport = IWYUSupport.Full;` from `Edge26.Build.cs` if targeting 5.3 |

---

## 8. Where I would start tomorrow

1. **Compile the project** (§1) and confirm it boots.
2. **Create IMC + IA assets** (§2.2) — without these, you can't drive the player.
3. **Drop a Mannequin into BP_Footballer** so you have *some* visual to control.
4. **Place L_Pitch** with one ball and two goals (§3.1).
5. **Hit Play.** Tweak `JogSpeed`, `SprintSpeed`, `BaseArmLength`, `BallLinearDamping` until the prototype "feels right" — that's the single most important thing in football games.
6. Only then move on to AI (§5.1) or animations (§4 / §5.2).

Movement feel beats feature count. Resist the urge to add menus, modes, or career systems until the on-pitch feel is solid.
