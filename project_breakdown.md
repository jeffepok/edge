Project Edge26 — Deep-Dive Design Docs (Unreal Engine 5)
Before I drill in, a quick framing on what UE5 changes about the plan. UE5 gives you a shipping renderer, world streaming, the editor toolchain, console certification paths, and a networking framework — but it actively fights you on the four systems that make a football game. So the strategy is: use UE5's shell, replace its core for gameplay. Specifically you will not use UE5's Character Movement Component, you will not use Chaos for ball or player gameplay physics, you will not use the stock Animation Blueprint state-machine approach for locomotion, and you will not trust UE5's default replication for competitive match sync. You'll build a deterministic simulation layer that sits beside UE5 and treats UE5 as a rendering/IO host.
Here are the four deep dives. I'll go deepest on the two that decide whether the game is good — animation and AI.

DEEP DIVE 1: The Motion-Matching Animation System
This is the system that decides if the game ships. Everything else is recoverable.
1.1 Why custom, and how it sits in UE5
UE5 ships Motion Matching (the Pose Search plugin) as of 5.4+. It is genuinely usable as a starting point — but the stock plugin is built for a single character responding to player input, not 22 simulated athletes whose foot must meet a physically-simulated ball at an exact point in spacetime. So:

Keep UE5's Pose Search runtime as the locomotion search core. It gives you the trajectory-matching, the database tooling, the schema/channel system. Don't rebuild that.
Replace the part that decides what trajectory to ask for — that's driven by your deterministic sim, not by UE5 input.
Add on top the procedural ball-contact layer, the physical-balance layer, and the context-posture layer. These do not exist in stock UE5.
Run the pose search inside your sim tick, not on UE5's animation thread, so it's deterministic across machines.

Architecturally: your simulation produces a PlayerAnimationIntent struct every 50 Hz tick (desired trajectory, current momentum/balance state, contextual posture, any ball-contact event). A thin adapter feeds that into Pose Search and the layered rig. UE5's USkeletalMeshComponent is just the thing that draws the result.
1.2 The animation database

Volume: plan 8–15 hours of mocap captured and tagged by ship. This is captured over the whole production, not up front. Phase 1 needs maybe 90 minutes — enough locomotion + basic ball actions — to prove feel.
Capture content, by category:

Locomotion: idle, jog, run, sprint, decel, sharp cuts at every 15° increment, backpedal, shuffle, lateral, plant-and-pivot, stumble/recover. Captured at multiple speeds.
Ball-carrying locomotion: the same movement set but with the ball, because a player's gait changes when dribbling. Touch cadence at jog vs sprint.
Ball actions: every pass type, every shot type, first touches (cushion, push, killed dead, deliberate let-run), traps, chest/thigh control.
Contests: jostles, shoulder barges, pulls, aerial jumps and challenges, tackles (standing/slide/poke), being tackled, trips, falls.
Goalkeeper: a whole separate capture program — dives, parries, catches, tips, distribution, positioning shuffles.
Set pieces & flavor: celebrations, restarts, walk-ups, reactions.


Tagging: every clip is tagged with foot-phase (which foot is planted, when contact frames occur), balance state, speed band, and "cost group." This tagging is what Pose Search consumes. Build a custom tagging tool early (Tools team) — hand-tagging 15 hours of mocap in raw UE5 is a productivity disaster. The tool should auto-detect foot plants from toe/heel marker velocity and let animators correct.

1.3 The layered rig — composition order
Each player evaluates these layers every sim tick, composited in this order:
Layer 0 — Locomotion (Pose Search).
Input: desired trajectory + current pose + speed band + ball-carry flag. Output: a full-body pose from the database. This handles the "where are the legs, how is the body moving" base.
Layer 1 — Upper-body action overlay.
Passing/shooting wind-up and follow-through, arms-for-balance, throw-ins. Blended onto the upper body with a spine-down weight mask so the legs keep doing locomotion until the action demands otherwise (e.g., a shot eventually overrides the plant leg too).
Layer 2 — Procedural ball-contact (the crown jewel — §1.4).
Two-bone IK + foot-roll that warps the kicking/touching limb so the foot meets the ball exactly. This overrides whatever Layers 0–1 produced for that limb, for the duration of the contact window.
Layer 3 — Look-at, head tracking, facial.
Head turns toward ball/threat/target. Eyes. Facial state (effort, focus, reaction). Cosmetic — runs on the render thread, not the sim, since it can't affect outcomes.
A custom UAnimInstance subclass orchestrates this, but the decisions about what each layer does come from the sim, not from an Animation Blueprint graph. Treat the AnimBP as a dumb executor.
1.4 Procedural ball-contact IK — the single most important feature
This is what kills "ice skating to line up a shot" and the "magnetic ball." The principle: the simulation decides the contact first; the animation is warped to honor it. Sequence per kick/touch:

Sim computes the contact event. Given the ball's predicted trajectory and the player's intent (pass here, shoot there), the sim solves for: contact point in world space, contact time, which foot, foot velocity vector at contact, and contact point on the ball (the ball-relative offset that produces the needed spin — feeds Ball Physics, Deep Dive elsewhere).
Sim picks a base animation from Layer 1 that's the right kind of action (instep drive, finesse wrap, toe poke, volley...) and the right approximate body shape.
Approach warping. Over the ~3–6 frames before contact, the player's locomotion (Layer 0) is gently warped — small stride-length and direction adjustments — so the plant foot lands in a plausible spot. This is subtle re-targeting, not teleporting. If the contact is geometrically impossible for this player's reach/balance/speed, the sim must have already chosen a different (worse) action or a stretch/lunge variant. The sim never asks for a contact the body can't honor — that constraint check happens at decision time.
Contact-window IK. During the contact frames, two-bone IK on the kicking leg drives the foot to the exact contact point, with foot-roll IK orienting the foot so the correct surface (instep/laces/outside/toe) faces the ball. The hips and support leg get a counter-adjustment so the player doesn't look dislocated.
Follow-through blends back from the IK'd pose into the base animation's follow-through, then back to Layer 0 locomotion.

Key rules for engineers:

IK correction is budgeted. Each player has a per-action "warp budget" (max cm of foot displacement, max degrees of approach re-aim) scaled by attributes — agility, balance, composure. A world-class player has a bigger budget (makes the difficult contact look clean); a clumsy player has a small budget (the warp can't save them, the touch looks awkward, the contact quality drops). The warp budget being attribute-scaled is what makes skill visible.
If the required warp exceeds budget → the sim is forced to a degraded outcome (stretch, miscontrol, foul-the-ball-away) and Layer 2 plays a "reach/stretch" variant. This is good — it's where bad touches come from.
Determinism: the IK solver runs in the sim tick with deterministic math (see §1.7). It is not UE5's stock IK rig running on the anim thread.

1.5 Physical balance & momentum layer
Sits between Layer 0 and the sim's movement model. Tracks a lightweight per-player balance state: center-of-mass offset, planted-foot support polygon, momentum vector.

A 180° turn at sprint isn't a clip swap — the sim checks the player's agility/balance vs the angular momentum change, and either: (a) allows a tight plant-pivot from the database, (b) forces a wider arc, or (c) triggers a stumble/over-run. Pose Search then finds the matching animation; it doesn't fake it.
Contact from another player perturbs the balance state → if it exceeds the support polygon, you get a stagger or a fall, ragdoll-blended (§1.6).
This layer is also what makes off-balance shots/passes degrade — Layer 1 actions query the balance state and a poor balance state shrinks the warp budget and raises error.

1.6 Ragdoll & physics blending
UE5's Chaos is fine here — for falls and collisions only, never for gameplay-relevant ball or locomotion physics.

Players are animation-driven ~95% of the time. On a trip/heavy collision/violent contact, the affected body (or just lower body) blends to Physical Animation (UE5's PhysicalAnimationComponent) — partial ragdoll with motors still trying to hold a pose, so it looks like a real falling athlete, not a dropped marionette.
Get-up blends back from the ragdoll pose into a context-appropriate recovery animation via Pose Search (find the database pose closest to the current ragdoll pose, blend to it, continue).
Cosmetic-only physics (a player clipped while not involved in the play) never affects the sim — it's render-thread only.

1.7 Determinism — the non-negotiable constraint
Because the pose search and ball-contact IK run inside the 50 Hz sim and affect outcomes, they must be bit-identical across PS5/Xbox/PC. Practical rules:

The Pose Search query, cost evaluation, and selection run in your sim module with a fixed-point or strictly-controlled deterministic float path — not UE5's default math, which is not guaranteed identical across platform compilers/SIMD.
The IK solver is your own deterministic two-bone solver, not UE5's AnimNode IK.
Animation playback (the actual skinning, the cosmetic Layer 3) runs on UE5's normal anim thread and does not need to be deterministic — only the pose selection and contact solve do, because only those feed back into the sim.
CI runs a determinism test every build: same input stream, every platform, hash the post-tick player+ball state, diff. Animation selection divergence is the most common determinism bug — catch it the day it's introduced.

1.8 Performance budget (UE5 specifics)

22 players × (Pose Search query + layered eval + IK) every 50 Hz tick. Pose Search is the cost. Mitigations: LOD the search — players far from the ball and off-camera search a smaller database subset at a lower rate; the ball-carrier and nearby players get full rate/full database. UE5's Pose Search supports database partitioning — use it.
Run pose search across worker threads (it's parallelizable per-player), but gather results deterministically (fixed order) before they touch the sim.
Skinning, cloth (UE5 Chaos Cloth for kits), and hair (UE5 Groom on hero players only) are render-thread and LOD hard.


DEEP DIVE 2: Match AI — The Spatial Value Model & Layered Intelligence
After feel, this is what players and reviewers notice. "The game feels dead" is always an AI failure.
2.1 The three-layer stack, mapped to UE5
LayerTick rateScopeUE5 implementationA — Team Strategy2–5 HzOne per teamPlain C++ sim object. Not an AIController.B — Unit Coordination~10 HzPer unit (def/mid/att)Plain C++ sim object.C — Individual Player AI50 HzPer playerPlain C++ in the sim tick. Not UE5 Behavior Trees for the core loop.
Why not UE5's AI framework (Behavior Trees, EQS, NavMesh, AIController)? Because it's built for non-deterministic, individually-reasoning NPCs on a navmesh. Football AI is coordinated, deterministic, and spatial-field-based, not navmesh-based. You may use EQS-style spatial querying as inspiration, and you may use NavMesh for nothing on the pitch (the pitch is open space — you don't need pathfinding, you need spatial evaluation). Build the AI as plain deterministic C++ inside the sim module.
2.2 The Spatial Value Model — the heart of off-ball intelligence
This is the core data structure that makes 21 off-ball players look intelligent. Every sim tick (or every few ticks, LOD'd), the pitch is evaluated as a set of value fields — think of them as heatmaps overlaid on the pitch.
The grid: the pitch is a coarse grid (e.g., ~2m cells, ~52×34 ≈ 1,800 cells — tune for perf). Each cell holds several scalar values, recomputed continuously:

Space value — how open is this cell? (distance to nearest opponent, weighted). High = open.
Pass-reception value — if a teammate were here, how good a passing option would they be? Combines: is there a clear passing lane from the ball carrier, is the cell ahead of play, is it in a dangerous area.
Threat value (attacking) — how dangerous is it to have the ball in this cell? An xG-like surface — high in the box, in the half-spaces, at the top of the D.
Defensive-coverage value — how well is this cell currently covered by our defense? Used to find gaps to plug.
Passing-lane occupancy — which cells are "blocked" because an opponent is screening them.

These fields are cheap to compute (it's mostly distance calculations and lane raycasts on a coarse grid) and they are shared — every player on a team reads the same fields. That shared read is what produces coordination without explicit messaging: if one striker takes the high-value cell, the field updates, and the second striker now sees a different cell as best. Emergent spacing, for free.
This is conceptually similar to UE5's EQS (scoring points in space) — but it's a persistent shared field, deterministic, and team-coordinated, so you build it custom.
2.3 Layer C — Individual Player AI
Every player, every tick, runs a compact decision loop. Two modes:
Off-ball (the make-or-break behavior)
The player evaluates a small set of candidate intents against the value fields and their role:

Make a run (into a high pass-reception cell that's also high threat) — with run-type (in-behind, checking, overlap, underlap, third-man, decoy).
Hold position / hold the line (defensive shape discipline — see Layer B).
Drop to receive (offer a safe passing angle by moving into a high pass-reception cell behind the ball).
Provide width / provide depth.
Press / cover / track a runner (defensive).

Each candidate gets a score = f(value field at target cell, role weighting, player attributes/traits, current team mentality from Layer A, stamina). Highest score wins; the player moves toward that cell. Crucially, the same situation yields different choices for different players because the role weights and trait weights differ — a poacher heavily weights "run in behind," a deep playmaker heavily weights "drop to receive."
On-ball
The ball carrier evaluates: pass / dribble / shoot / hold / clear / cross. Each option is scored:

Pass: for each viable receiver, score = pass-reception value at their location × pass success probability (from passer attributes, distance, lane occupancy, pressure) × resulting threat gain.
Shoot: threat value of current cell × finishing probability vs pressure/angle/keeper position.
Dribble: is there an adjacent high-space cell I can carry into, vs the dribbling/agility attributes of me vs the nearest defender.
Hold/shield: if all options are bad and I have the strength for it.
Clear: defensive panic option, weighted up by low team mentality + high threat against.

Player traits bias the weights — a "flair" player's dribble score is inflated, a "safe" player's backward-pass score is inflated, a "long-shot specialist" shoots from range. This is where PlayStyle-type traits live.
Defensive individual AI
Marking assignment (man/zone/hybrid — assigned by Layer B), jockeying (don't dive in — hold the channel), the decision to commit to a tackle (a scored gamble: tackle-success probability vs the cost of being beaten, weighted by whether there's cover behind — read from the defensive-coverage field), tracking runners, stepping up for the offside line.
2.4 Layer B — Unit Coordination
Runs at ~10 Hz, three instances per team (defense, midfield, attack). Its job is the stuff individuals can't decide alone:

Defensive line: holds a coordinated line height (set by Layer A), shifts laterally as a unit toward the ball, steps up to compress space or to spring an offside trap, drops on a ball over the top. The line is a single coordinated entity that assigns each defender a slot; the defenders' Layer C then occupies the slot while still reacting locally.
Pressing coordination: Layer B owns the press trigger — it watches for trigger conditions (opponent's bad touch, a back-pass, a pass into a sideline trap, a slow square ball) and, when fired, assigns press roles: who jumps the ball, who cuts the backward option, who shifts to cover. Without this central coordination you get either no press or all-11-chase-the-ball chaos.
Attacking patterns: owns multi-player patterns — the overlap (fullback + winger handshake), the switch of play, the third-man run, creating-and-exploiting space. It nudges the Layer C weights of the involved players so the pattern emerges.
Compactness: keeps the unit's vertical and horizontal spacing within tactical bounds.

2.5 Layer A — Team Strategy
Runs at 2–5 Hz, one per team. The "manager brain."

Holds the tactical plan: formation, mentality (defensive↔ultra-attacking), pressing intensity, defensive line height, width, tempo, build-up style (play out from back / direct / mixed), and counter-attack vs possession bias.
Reads the game state — scoreline, time remaining, man advantage/disadvantage, momentum (a tracked value), fatigue across the squad — and shifts the plan: "1–0 up, 80th minute → mentality down, line drops, tempo slows, time-wasting behaviors unlock." "0–1 down, 75th minute → mentality up, line steps up, more bodies forward, accept more risk."
Manager personality presets so AI teams play to their real-world identity (a possession side vs a transition side vs a low-block side). These are data-defined config (see §config-driven, original doc).
Owns substitution and in-game tactical-change AI for career/offline.
Layer A's outputs are just inputs (weights, line heights, trigger sensitivities) to Layers B and C. It never controls a player directly.

2.6 Where ML earns its place (and where it doesn't)

Use ML offline to tune the value fields and run-timing, trained on real match tracking data plus your own playtest telemetry. The output is a set of learned weights / small lightweight evaluators that plug into the deterministic field computation. They are evaluated at runtime as fixed, deterministic functions.
Use ML for difficulty scaling — a model that subtly adjusts AI weight parameters to keep matches competitive for a given player. Tunable, transparent, and hard-off in competitive/ranked modes.
Do NOT put a black-box neural net in the per-tick decision loop. It breaks determinism, it can't be debugged, it can't be balanced by designers, and it will do something insane on launch day in front of a million people. The AI's decisions stay as inspectable scored systems; ML only tunes the scoring.

2.7 Difficulty levels
Difficulty is not "AI cheats / gets attribute boosts" (players hate that and it's detectable). Difficulty scales:

Decision quality (how often the AI picks the genuinely optimal option vs a suboptimal one).
Reaction latency (how many ticks before the AI responds to a state change).
Press aggression and line coordination tightness.
The warp-budget / error tolerance on AI ball actions.
Higher difficulty = the AI plays better football, not cheating football.

2.8 Debuggability
Build an AI debug overlay from week one (Tools team): visualize the value fields as colored heatmaps on the pitch, draw each player's candidate intents and chosen score, show the press-trigger state, show Layer A's current plan. You cannot tune this system blind. This overlay is also how designers balance it.

DEEP DIVE 3: Netcode — Deterministic Rollback on UE5
UE5's stock replication (the Actor replication / CharacterMovementComponent prediction model) is built for shooters with server-authoritative actors and is not suitable for a 22-body deterministic physics sim. You replace the gameplay-sync layer entirely and use UE5 only for transport (its socket layer, NAT traversal, the connection handshake) and for non-gameplay replication (lobby, chat, presence).
3.1 The model: deterministic lockstep with rollback
Because the entire match sim is already deterministic (50 Hz, controlled math — the same property the animation and AI dives depend on), the netcode is conceptually simple:

Both clients run the identical deterministic simulation.
The only thing exchanged over the wire is player input (a tiny packet: stick vectors + button bitfield + tick number).
Each client predicts forward using its own input + the predicted remote input.
When the real remote input arrives and differs from the prediction → rollback: restore sim state to the last agreed tick, re-apply the corrected inputs, re-simulate forward to "now." Because the sim is deterministic and fast, this is invisible.

This is the fighting-game netcode model (GGPO-style) applied to football — it's the right fit precisely because you already paid the determinism tax for animation and AI.
3.2 What this requires of the sim

Full sim state must be snapshottable and restorable cheaply. The entire match state (22 players + ball + officials + match clock + RNG state + AI layer state) must serialize to a compact buffer and restore from it. Budget this as a hard constraint on every sim system: if a system holds state, that state must be in the snapshottable block. No hidden statics, no UE5 actor state that isn't mirrored.
Ring buffer of snapshots — keep the last N ticks (N = max rollback window, e.g. ~8–12 ticks = ~160–240ms) so you can roll back to any of them.
The sim must be re-runnable headless and fast — re-simulating 10 ticks on rollback must fit comfortably inside one frame's spare budget. (This is the same headless-runnable property you need for CI and for career-mode simulation — one architectural decision pays off three times.)
RNG is part of the state. Any randomness (contact micro-variation, etc.) comes from a seeded PRNG whose state is in the snapshot, so rollback re-rolls identically.

3.3 Two deployment modes
Casual / friendly: peer-to-peer rollback. Two clients, direct connection, both authoritative-ish, rollback reconciles. Cheap (no server cost), great feel on good connections. Cheating risk is tolerable for unranked.
Ranked / competitive: dedicated-server-authoritative. The dedicated server runs the authoritative deterministic sim. Clients still predict locally (so it feels responsive) and reconcile against the server's authoritative state. The server is the single source of truth for the result, the economy, and anti-cheat. Fleet-managed (Agones on Kubernetes), region-routed by the matchmaker. This costs money per match — it's why ranked is gated and casual isn't.
3.4 Lag mitigation

Input delay buffer: a small fixed input delay (a couple of ticks) reduces how often rollback is needed — tunable, and the matchmaker pairs similar-latency players so the delay can stay low.
Rollback cap: beyond the max window, you don't roll back further — you accept the remote state and show a brief "connection unstable" indicator.
Adaptive: on a degrading connection, lean more on input delay, less on rollback (rollback past a point becomes visible as rubber-banding).
Graceful disconnect: if a peer drops, the dedicated-server mode continues with the server; p2p mode awards/voids per the competitive ruleset.

3.5 Determinism enforcement (the CI gate)
This is the same gate referenced in the animation and AI dives — it's worth stating as its own pipeline:

A CI determinism test runs every build: a recorded input stream is fed through the headless sim on PS5, Xbox, and PC build targets. After every tick, the full sim state is hashed. The hashes must be identical across all three platforms for the entire run. Any divergence fails the build and bisects to the offending commit.
Common divergence sources to police: uncontrolled float math (compiler/SIMD differences across platforms), iteration order over hash maps/sets, uninitialized memory, any use of wall-clock time or UE5 frame delta inside the sim, any UE5 engine call inside the sim that isn't itself deterministic.
The rule for engineers: the sim module links against a restricted surface of UE5 — math, containers (with deterministic iteration), and not much else. Rendering, audio, input polling, and networking transport are outside the sim and feed it via clean interfaces.

3.6 Anti-cheat integration

Server-authoritative ranked means the server validates every input is physically plausible and every resulting state is reachable — impossible inputs/states flag.
The input stream is the replay (see Replay system in the original doc) — a flagged match ships its input stream for exact deterministic re-review.
Kernel-level client component on PC + telemetry anomaly detection feed the trust/penalty system.


DEEP DIVE 4: The Economy & Compliance Service
The revenue engine of the Ultimate-Team-style mode — and the system most likely to get you fined or pulled from a storefront if architected naively. It is a backend service; the client is a dumb terminal that displays what the server says it owns.
4.1 Core principle: the client owns nothing
The client never decides what items or currency a player has. It requests and displays. Every item, every coin, every pack outcome is authoritative on the server. This kills an entire class of duplication and injection exploits and is the foundation of both anti-fraud and regulatory compliance.
4.2 The authoritative ledger

Double-entry, append-only transaction ledger. Every movement of an item or currency is a transaction with a source and a destination. Balances are derived from the ledger, never edited in place. This makes everything auditable and reversible.
Item instances are unique and tracked — every player-card item has an instance ID and a full provenance chain (pulled from pack X at time T → listed → sold to user Y → used in squad Z). Provenance is what lets you detect RMT and unwind fraud.
Idempotency everywhere — every economy mutation carries an idempotency key so a retried request (flaky mobile connection) can't double-spend or double-grant.
Built on a strongly-consistent transactional datastore for the ledger; caches/read-replicas for the high-volume read paths (browsing the marketplace).

4.3 The pack/reward service — architected for regulation from day one
This is the part that must be region-configurable as a first-class design property, not a patch:

Probabilities are server-defined, server-stored, and disclosed. The drop rates for every pack live in server config. The client fetches and displays them. They are never in the client binary.
The roll happens server-side. Client requests "open pack," server rolls against the authoritative probability table, writes the resulting item instances into the ledger via transactions, returns the result. The client animation is pure theater played after the outcome is decided.
Region configuration layer. Every market has a config profile that can independently set: whether random-purchase packs are available at all, whether disclosure UI is mandatory and in what form, age-gating requirements, spend limits/throttles, and whether a non-randomized direct-purchase path must be offered (some jurisdictions require you can buy the specific thing, not gamble for it). A regulatory change in a market becomes a config change, not a re-architecture or a client patch.
Spend tracking & limits — per-account spend is tracked server-side; configurable cooling-off / spend-cap / session-spend-warning behaviors per region; parental controls integration on console.
Audit trail — every pack definition, every probability table, every change to them is versioned and logged. When a regulator asks "what were the odds on this date," you can answer exactly.

4.4 The marketplace / auction engine

Bid + buy-now auction house. Bid resolution, anti-sniping (small auto-extensions on last-second bids), listing limits.
Price ranges (floor/ceiling) per item — server-enforced min/max prices. This is the single most effective anti-RMT and anti-fraud tool: it prevents the "sell a common card for 10 million coins" money-laundering pattern that real-money traders use to move illicit currency to buyers.
Transaction tax / sink — a configurable cut on sales removes currency from the economy to fight inflation. Economy health is monitored via telemetry; sinks and faucets are tunable server-side.
All marketplace mutations go through the same idempotent, double-entry ledger.

4.5 Fraud & RMT detection

Integrated with the anti-cheat telemetry pipeline. Signals: provenance anomalies (an account that only ever receives high-value items and sends coins), implausible win/pack patterns, price-range-skirting trade loops, account-takeover patterns (sudden behavior change + new device + asset liquidation).
Anomaly models flag; a trust system gates; an appeals path exists. Confirmed fraud is unwound through the ledger — because every transaction is recorded and reversible, you can claw back a fraud chain without nuking innocent downstream holders unfairly (you can see exactly who knowingly participated).

4.6 Service architecture (UE5 client side is thin)

Microservices on Kubernetes, multi-region, behind the same API gateway as the rest of the backend (original doc §6). The economy services: Ledger, Pack/Reward, Marketplace, Region-Config, Fraud, Spend-Limits.
The UE5 client talks to them over the gateway with authenticated, idempotent requests. The client has a local cache of "what I own" for fast UI, but treats it as untrusted display state — the server reconciles it and the server always wins.
Event bus (Kafka/Pulsar) streams every economy event to the data lake for economy-health analytics, balancing, and fraud detection.

4.7 The compliance posture, stated plainly
Architect as if every market will eventually require: full odds disclosure, a non-gambling purchase path, spend limits, age verification, and the right to be audited. Building those in as config from day one costs you maybe 15% extra on the economy service. Retrofitting them under regulatory pressure after launch costs you a re-architecture, emergency patches, and possibly a storefront removal in the meantime. This is the cheapest insurance in the project — buy it.

How the four systems lock together
The thread running through all four: one determinism decision pays off everywhere. The 50 Hz deterministic sim makes the animation pose-selection reproducible, makes the AI inspectable and reproducible, makes rollback netcode possible at all, and gives you a headless-runnable sim that serves CI testing, career-mode simulation, and exact replay/anti-cheat review. If any team breaks determinism, all four systems degrade at once — which is exactly why the CI determinism gate is the most important piece of infrastructure in the project and should exist before Phase 1 gameplay work begins.
The UE5 thread: you're using UE5 as a rendering and platform host, and building a deterministic simulation module that lives beside it. UE5 draws, streams, handles IO, ships to console. Your sim module decides everything that matters. Keep that boundary clean and ruthlessly enforced — every time something leaks across it (a UE5 actor holding gameplay state, an engine call inside the sim tick), you've introduced a determinism bug, a netcode bug, and a testability gap simultaneously.