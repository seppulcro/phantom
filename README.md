
 # PHANTOM — Post-quantum Hash-chained Attestation Network for Tactical Offline Mesh

 ![Palantir at home](https://files.catbox.moe/crtpxu.jpg)

 ---

 > — Can we get Palantir?
 >
 > — We have Palantir at home.
 >
 > **Palantir at home:** a cyberdeck and some ESP32s from AliExpress.

 ---

 **A proposed design for a quantum-anchored flight recorder for AI and field ops — intended to run over LoRa without internet infrastructure, on ~$650 of commodity hardware.**

 PHANTOM is what you'd get if a **flight data recorder**, a **notary public**, and a **LoRa mesh** had a baby — and the notary used physics instead of a stamp.
 The design calls for every AI output, every position broadcast, and every tactical order to be cryptographically signed and hash-chained to a quantum event that existed before the system booted.
 No cloud. No cell tower. No internet required. Fits in a backpack. Designed to survive quantum-era cryptographic attacks.

 **This is a design specification. None of the phases below are implemented yet.**

 | Property | Target Phase | Standard / Mechanism |
 |---|---|---|
 | **Offline mesh transport** | Phase 1+ | LoRa SX1262 + ESP-NOW — no cell towers, no cloud, no DNS |
 | **Post-quantum signatures** | Phase 1+ | NIST FIPS 203 (ML-KEM-768) + FIPS 204 (ML-DSA-65) — lattice-based, Shor-resistant |
 | **Quantum-seeded entropy** | Phase 1 (beacon) / Phase 5 (on-chip) | NIST/CURBy/ANU beacons → IDQ/Quside hardware QRNG |
 | **Tamper-evident audit** | Phase 1+ | SHA3-256 hash chain + ML-DSA-65 signatures + SQLite |
 | **DRAM layout unpredictability** | Phase 5 (hypothesis) | Tailslayer fork: QRNG-seeded channel offsets; whether this prevents targeting or preserves p99 latency benefit is untested |

 > *The goal: prove what your AI said, when it said it, and that nobody picked the answer.*
 > *Prove what your hardware is doing, which memory it's touching, and that nobody predicted the layout.*
 > *Prove what your operator said, when they said it, and that nobody faked the order.*
 >
 > If built as specified: anyone with your public key could verify all of it — independently, offline, forever.

 ---

 **Why now?** Three things converged in 2024–2026:

 1. **AI output is increasingly unverifiable.** C2PA [9] exists for images but is cloud-anchored and PRNG-seeded — provenance breaks the moment the internet does. There is no tamper-evident, offline, quantum-anchored AI attestation system for field use. PHANTOM is a proposed first attempt.
 2. **Post-quantum cryptography just standardized.** NIST finalized ML-DSA-65 and ML-KEM-768 (FIPS 203/204, August 2024). Production C implementations exist in liboqs today. Almost nobody has deployed them on ESP32-class embedded hardware yet. PHANTOM proposes to.
 3. **The Tailslayer extension is a new research question.** Tailslayer demonstrates hedged DRAM reads reduce p99 tail latency. PHANTOM proposes feeding the channel offset from a hardware QRNG to make the layout physically unpredictable — whether that preserves or improves the latency benefit is an open, untested hypothesis.

 The composition is novel. The parts are not. That's a strong place to be.

 **Inspired by** Laurie Kirk's ([@LaurieWired](https://github.com/LaurieWired)) video *"Your RAM Has a 60 Year Old Design Flaw. I Bypassed It."* [34] and her Tailslayer project — which demonstrated that DRAM channel placement is predictable and exploitable, and reduces tail latency by issuing hedged reads across multiple channels with uncorrelated refresh schedules. The core insight is that hardware assumptions we treat as fixed are often reverse-engineerable. That's a useful lens for thinking about trust in general: if you can't inspect or verify a layer, you're implicitly trusting it. PHANTOM's design response is to make trust explicit and verifiable at every layer — signed, hash-chained, and anchored to entropy that no party controlled. The offline mesh design is driven by operational resilience: it should work when cell towers are down, in remote terrain, or in environments where internet routing can't be trusted. In Phase 5, PHANTOM proposes forking Tailslayer to feed its channel offset selection from a hardware quantum chip rather than a fixed reverse-engineered value — making the memory layout physically unpredictable. Unlike CSPRNG seeds, which are deterministic given sufficient compute, quantum measurement outcomes are not determined until the moment of measurement [10]. No adversary, regardless of computational resources, can predict which channel offset a QRNG-seeded session will select. Whether this approach preserves Tailslayer's p99 latency benefit is an open research question.

 --- Every component exists and ships today. NIST/CURBy quantum beacons are live production APIs.
 ML-DSA-65 and ML-KEM-768 are NIST FIPS 203/204 standards (finalized August 2024) with production C implementations in liboqs.
 The ESP32 + LoRa SX1262 combo powers tens of thousands of Meshtastic nodes globally right now.
 llama.cpp runs quantized 7B models on a Raspberry Pi 5 at 1–3 tokens/sec — slow by cloud standards, fast enough for tactical queries offline.
 Tailslayer [34] demonstrates that hedged DRAM reads across channels with uncorrelated refresh schedules measurably reduce p99 tail latency — benchmarked on AMD, Intel, and Graviton. PHANTOM Phase 5 proposes feeding the channel offset from a hardware QRNG (IDQ/Quside) to make the layout physically unpredictable. Whether QRNG-selected offsets preserve that latency benefit is an open, unbenchmarked hypothesis.
 Certificate Transparency already runs hash-chained append-only logs at internet scale.
 The integration is novel. The parts are not.

 ---
 
 ## Abstract
 
 Modern computing stacks have a systemic trust problem: every layer that depends on
 a pseudo-random number generator (PRNG) — memory layout randomization (ASLR),
 DRAM channel assignment, LLM token sampling, session key generation — is
 theoretically predictable by a sufficiently motivated adversary. PHANTOM proposes
 a unified architectural response: replace PRNG seeds with **quantum random number
 generator (QRNG)** entropy at every security-critical layer, and anchor every
 seeding event in a **post-quantum-signed, hash-chained attestation record** that
 can be independently verified by anyone with the public key.
 
 PHANTOM proposes extending **QAIA** (Quantum-Attested Inference Architecture) — a pipeline
 for constructing tamper-evident attestation chains anchored to quantum randomness
 [1, 2, 3] — into a decentralized, offline tactical mesh. The proposed system would compose
 quantum random number generation [1, 2, 3], post-quantum digital signatures [4, 5],
 hash-chained audit logs [13], DRAM channel randomization via Tailslayer [34], and
 a tiered ESP32 WiFi/LoRa mesh to provide non-repudiation, temporal binding,
 sampling integrity, and hardware-level unpredictability in infrastructure-denied
 environments. Whether QRNG-seeded Tailslayer channel offsets preserve the hedged-read
 latency benefit while making DRAM layout physically unpredictable is an open,
 unbenchmarked research question that Phase 5 is designed to answer.

 The intended proof-of-concept is deliberately minimal: a ~$15–25 ESP32 and a cyberdeck built
 from commodity parts, to be validated on an airsoft field with zero cell coverage.
 **The mesh transport is designed to operate without internet infrastructure** — every
 critical operation runs over LoRa and ESP-Mesh with no cell towers or cloud required —
 **and the cryptographic layer is specified to be post-quantum secure** — ML-DSA-65 and ML-KEM-768 are NIST FIPS 203/204
 lattice-based algorithms designed to withstand quantum computer attacks. No
 quantum chip required to start: the architecture uses public quantum randomness beacons
 [1, 2] over the internet today, and is designed so that when quantum chips become
 commodity (embedded in NUCs, MacBooks, or microcontrollers), every layer upgrades
 in place without protocol changes.
 
 ---
 
 ## 1. The Vision
 
 Every PRNG seed in a modern system is a potential attack surface. ASLR is only as
 strong as its entropy source. DRAM channel placement — as demonstrated by
 Tailslayer [34] — follows deterministic XOR offsets that can be reverse-engineered,
 making Rowhammer targeting [31] tractable. LLM inference samples tokens from a
 seeded distribution that can be replayed. Session keys are generated from
 algorithmically predictable state.
 
 PHANTOM's thesis is that **quantum entropy closes all of these gaps simultaneously**,
 and that **attestation makes the closure verifiable**:
 
 | Layer | Current entropy source | PHANTOM upgrade | Attestation |
 |---|---|---|---|
 | ASLR / pointer auth | `getrandom()` CSPRNG | On-chip QRNG seed per boot | Signed boot event in chain |
 | DRAM channel XOR (Tailslayer) [34] | Fixed deterministic offset | QRNG-seeded per session | Channel seed pulse ID in 
chain |
 | LLM token sampling (QAIA) | PRNG seed | NIST beacon / on-chip QRNG | `quantum_entropy` field per block |
 | Mesh node session keys | Static keypair | QRNG-seeded ephemeral keys | ML-KEM-768 wrapped, logged |
 | TLS / comms | CSPRNG | QRNG-seeded, ML-DSA-65 signed | Every handshake attested |
 
 The chain is the key insight: not just "we used quantum entropy" as a marketing
 claim, but a signed, hash-chained, publicly verifiable proof that every seeding
 event was anchored to a specific quantum pulse — one that existed before the
 system booted, that no attacker could have predicted, and that anyone can
 independently verify against the public beacon record.
 
 This works today using public beacon APIs. The `qaia.entropy` module abstracts
 the entropy source behind a hardware driver interface: swap the HTTP beacon client
 for a local QRNG IC driver and every other layer is unchanged. The attestation
 schema already includes an `entropy_source` field: `beacon | hardware | cached`.
 
 ---
 
 ## 2. The Problem
 
 ### 2.1 The PRNG Attack Surface
 
 Pseudo-random number generators are deterministic given their seed. This is not
 a theoretical concern:
 
 - **Rowhammer on inference**: DeepHammer [31] demonstrated targeted DRAM bit
   flips that degrade DNN classification accuracy to near-zero. No kernel exploit,
   no quantum computer. The attack works because DRAM channel placement follows
   predictable deterministic offsets — reverse-engineered by Laurie Kirk (LaurieWired) for AMD,
   Intel, and Graviton in Tailslayer [34]. QRNG-seeded channel offsets remove the
   predictability that makes targeting possible.
 - **Selective re-sampling**: An operator running LLM inference can replay the same
   prompt until a favorable output is produced and selectively attest only that
   result. There is no mechanism to detect this without a sequential, externally
   anchored attestation chain with gap-detectable block IDs.
 - **Harvest-now-decrypt-later**: Adversaries collect signed traffic today to
   decrypt retroactively when quantum hardware matures [8]. Classic ECDSA and RSA
   are broken by Shor's algorithm [32]; the timeline is uncertain but contracting
   [33].
 
 ### 2.2 The Provenance Gap
 
 Tactical operators and first responders face a structural auditability failure.
 Current tools (Discord, ATAK, Zello) provide no cryptographic non-repudiation
 and are vulnerable to future quantum adversaries [8].
 
 - **Infrastructure Dependency**: Communication fails when cell towers or cloud
   servers are unavailable. There is no fallback with integrity guarantees.
 - **Auditability Gap**: No immutable record of "who said what and when" exists in
   offline environments. After-action reports rely on human memory and screenshots.
 - **Inference Integrity**: AI outputs with life-safety consequences (medical
   triage, tactical advisories) are unsigned plaintext with no proof of provenance.
    Content provenance standards such as C2PA [9] address this in media pipelines,
    but provide no offline, radio-transportable equivalent for tactical operations.
 
 ### 2.3 The Threat Stack
 
 | Threat | Vector | Timeline |
 |---|---|---|
 | **Rowhammer on inference** | Predictable DRAM channel placement enables targeted bit flips [31, 34] | **Now** |
 | **Unsigned API responses** | MITM or selective disclosure of plaintext LLM outputs | **Now** |
 | **Harvest-now-decrypt-later** | Collect signed traffic today, decrypt when quantum hardware matures [8] | 
**Near-future** |
 | **Shor's algorithm breaks PKI** | Quantum computer attacks RSA/ECDSA signing infrastructure [32] | **Uncertain — but 
contracting [33]** |
 
 ---
 
 ## 3. Background
 
 ### 3.1 Quantum Random Number Generation
 
 Quantum random number generators exploit fundamental quantum mechanical processes
 — typically vacuum fluctuations or photon detection — to produce bits whose
 randomness is guaranteed by physical law rather than computational assumptions
 [10]. Public randomness beacons such as the NIST Randomness Beacon [1] and the
 CURBy quantum randomness beacon at the University of Colorado [2] broadcast
 timestamped, signed random pulses at regular intervals, creating a publicly
 auditable source of unpredictable entropy available today over HTTP.
 
 ### 3.2 Post-Quantum Cryptography (PQC)
 
 NIST standardized three post-quantum cryptographic algorithms in 2024. PHANTOM
 utilizes two:
 
 - **ML-DSA-65 (Dilithium)** [4] — Module-Lattice-Based Digital Signature Standard
   (FIPS 204). Signs every mesh message and every attestation block. Security
   grounded in the hardness of Module Learning With Errors (Module-LWE) [12].
 - **ML-KEM-768 (Kyber)** [5] — Module-Lattice-Based Key-Encapsulation Mechanism
   Standard (FIPS 203). Quantum-resistant key exchange for encrypted mesh sessions.
 
 Classic ECDSA and RSA are broken by Shor's algorithm on a sufficiently powerful
 quantum computer [8]. ML-DSA and ML-KEM provide durability against this threat.
 
 ### 3.3 Hash-Chained Audit Logs
 
 Tamper-evident logging via hash chains was introduced by Haber and Stornetta [13]
 and formalized in the Merkle tree structure [6]. Certificate Transparency [14]
 demonstrated append-only hash-chained logs at internet scale. PHANTOM applies the
 same pattern to every security-critical seeding event: each block embeds
 `SHA3-256` [15] of the previous block's canonical serialization, making any
 retroactive modification detectable by any node holding the chain.
 
 ### 3.4 DRAM Channel Randomization (Tailslayer)
 
 Tailslayer [34] is a C++ library by Laurie Kirk (LaurieWired) that exploits undocumented CPU
 address XOR/scrambling bits to force data onto specific DRAM channels, then
 hedges reads across replicas with uncorrelated refresh schedules to reduce tail
 latency. The same mechanism that enables this — deterministic, reverse-engineerable
 channel offsets on AMD, Intel, and Graviton — is what makes Rowhammer targeting
 tractable. PHANTOM proposes feeding QRNG entropy into these offsets: the channel
 layout would change every session, seeded from a quantum event no attacker could have
 predicted, with the seed pulse ID attested in the chain. Whether QRNG-selected offsets
 preserve the hedged-read latency benefit, and whether they meaningfully reduce Rowhammer
 targeting tractability, are open research questions that Phase 5 is designed to test.

 This dual-use property is the hypothesis driving Phase 5. LLM inference with llama.cpp on a
 Raspberry Pi 5 is DRAM-bandwidth bound during autoregressive decoding (~1 FLOP/byte
 arithmetic intensity), making it directly sensitive to DRAM refresh stalls. Those stalls
 are invisible in average latency but clearly visible as p99 spikes in time-to-first-token
 and inter-token intervals. Tailslayer's hedged reads across channels with uncorrelated
 refresh schedules suppress those stalls on standard DRAM. Whether QRNG-seeded channel
 offsets preserve that benefit — or whether the randomization disrupts the latency
 advantage — is an unbenchmarked question. If it holds, the same mechanism delivers both
 physically unpredictable memory layout (security) and lower token generation tail latency
 (performance). That's worth testing.
 
 ### 3.5 Mesh Transport
 
 PHANTOM uses a tiered mesh strategy, drawing on the LoRa mesh ecosystem [28]
 as prior art. The primary transport is a **WiFi mesh**
 formed by ESP32 nodes using **ESP-NOW** and **ESP-Mesh** (Espressif proprietary mesh protocol over 802.11 b/g/n). ESP-NOW
 provides ultra-low-latency peer-to-peer communication (~200m range) without a
 central access point. ESP-Mesh extends this into a self-healing routed network
 where each node is a relay — more players means better coverage, not worse.
 
 **LoRa (SX1262)** is retained as a fallback transport for nodes outside WiFi mesh
 range, providing multi-kilometer range at sub-watt power levels. LoRa activates
 automatically on mesh loss. The attestation chain is transport-agnostic: blocks
 delivered over LoRa and WiFi are verified identically.
 
 ---
 
 ## 4. Architecture
 
```
Quantum Entropy Sources           Inference Engine (Cyberdeck)
(NIST Beacon / CURBy / ANU)      (llama.cpp [7] + QRNG sampling)
        │                                   │
        ▼                                   ▼
┌───────────────────────────────────────────────────────┐
│              QAIA Block Builder                        │
│                                                       │
│  block_id          — sequential index                 │
│  timestamp         — ISO-8601                         │
│  quantum_entropy   — beacon pulse hash + pulse ID     │
│  entropy_source    — beacon | hardware | cached       │
│  input_hash        — SHA3-256(prompt)                 │
│  output_hash       — SHA3-256(completion)             │
│  model_id          — SHA3-256(model file)             │
│  prev_block_hash   — SHA3-256(prev canonical block)   │
│  signature         — ML-DSA-65 over all fields        │
└───────────────────────────┬───────────────────────────┘
                            │
              ┌─────────────▼──────────────┐
              │    ESP-Mesh (WiFi primary)  │
              │    LoRa (automatic fallback)│
              │    ML-DSA-65 signed msgs    │
              └──────┬──────────┬──────────┘
                     │          │
          ┌──────────▼──┐  ┌────▼────────────┐
          │  Cyberdeck   │  │  ESP32 CYD ×N   │
          │  (Tier 1)    │  │  (Tier 2)       │
          │              │  │                 │
          │  Full chain  │  │  Verify sigs    │
          │  SQLite      │  │  GPS + map UI   │
          │  NOMAD KB    │  │  WiFi + LoRa    │
          └─────────────┘  └─────────────────┘
```

 
 ### Tier 1: Hub — The Cyberdeck
 
 A portable, hand-built SBC enclosure (Raspberry Pi 5 or equivalent) running
 local LLMs and the **NOMAD** offline knowledge stack [29]. Battery-powered.
 No cloud. No cell signal. Fits in a backpack. Participates in the ESP-Mesh WiFi
 network directly as a peer node.
 
 - **QAIA Block Builder**: Generates signed attestation blocks for every inference
   event, containing `input_hash`, `output_hash`, `quantum_entropy` pulse IDs,
   `entropy_source`, and `model_id`.
 - **Mesh Peer**: Joins ESP-Mesh directly; serves the QAIA HTTP API to all nodes.
 - **Offline Knowledge**: Local mirrors of Wikipedia, medical databases, and
   OpenStreetMaps via [NOMAD](https://github.com/Crosstalk-Solutions/project-nomad).
 - **Chain Authority**: Maintains the canonical SQLite attestation chain; exports
   JSON-LD blocks for independent verification.
 
 ### Tier 2: Edge Nodes — Open AirTags
 
 **~$15–25 ESP32 CYD units** [30] with GPS (BN-220) and LoRa (SX1262). Each one is
 functionally an open, hackable AirTag — except every transmission it receives is
 verified against ML-DSA-65 signatures from the Hub, works fully offline, and doesn't
 phone home to anyone. Private keys never leave the Hub; edge nodes hold only public keys.
 
 - **Primary transport**: ESP-NOW / ESP-Mesh WiFi — nodes relay for each other.
 - **Fallback transport**: LoRa (SX1262) activates automatically on mesh loss.
 - **Signature Verification**: Verifies ML-DSA-65 blocks from the cyberdeck.
 - **Offline-First**: No internet required at any point in the verification path.
 
 ---
 
 ## 5. Security Properties
 
 | Property | Mechanism | Security Assumption |
 |---|---|---|
 | **Tamper-evidence** | SHA3-256 hash chain [15] | Collision resistance of Keccak sponge [16] |
 | **Non-repudiation** | ML-DSA-65 signature per block [4] | Signer maintains exclusive control of private key |
 | **Temporal binding** | NIST beacon pulse ID embedded in block [1] | Beacon operates honestly (publicly auditable, 
signed pulses) |
 | **Sampling integrity** | QRNG seeds token selection [10] | Entropy source randomness guaranteed by quantum mechanics |
 | **DRAM unpredictability** | QRNG seeds Tailslayer channel offset [34] | Same quantum entropy guarantee |
 | **Post-quantum durability** | ML-DSA + SHA3 [4, 15] | Hardness of Module-LWE [12] |
 | **Message attribution** | ML-DSA-65 per mesh transmission | Key custody maintained per node identity |
 | **Offline verifiability** | Cached beacon pulses + embedded signatures | Local copy of public key and pulse cache 
sufficient |
 
 ---
 
 ## 6. Threat Model
 
 | Threat | Impact | Mitigation |
 |---|---|---|
 | Signing key compromised | Attacker forges valid attestations or mesh messages | Key rotation policy; revocation list 
propagated over mesh; HSM storage [25] |
 | NIST beacon downtime | Temporal binding weakened | Multi-source entropy (CURBy [2], ANU [3]); cached pulses; 
`entropy_source` flagged in block |
 | Model weights poisoned | QAIA faithfully attests poisoned output | Out of scope — QAIA is an integrity layer, not a 
safety layer. See [26]. |
 | Rowhammer / bit-flip attack | DRAM bit flips corrupt model weights or output buffer [31] | `output_hash` captured 
immediately post-inference; QRNG-seeded channel offsets [34] remove targeting predictability |
 | Host machine compromised | Attacker intercepts data before attestation | Trusted Execution Environment (Intel SGX/TDX 
[27]) — future work |
 | Selective attestation | Operator attests only favorable inference outputs | Mandatory sequential block numbering; gaps 
in `block_id` are detectable |
 | LoRa replay attack | Stale signed message rebroadcast as current | Block timestamp + `prev_block_hash` binding; 
duplicate `block_id` detection |
 | Node impersonation | Attacker injects unsigned messages | All mesh messages require valid ML-DSA-65 signature |
 
 ---
 
 ## 7. Limitations
 
 - **No computational speedup.** Quantum entropy adds latency, not performance.
   No quantum advantage is claimed.
 - **No model safety guarantees.** A poisoned model produces attested-but-incorrect
   output. PHANTOM is an integrity layer, not a safety layer.
 - **No anonymity.** PHANTOM attributes messages to signing identities.
 - **No distributed consensus.** The hash chain is a tamper-evident append-only log,
   not a blockchain. Chain authority lies with the Hub node.
 - **LoRa bandwidth constraints.** ML-DSA-65 signatures are ~3.3 KB. LoRa's low
   bandwidth requires fragmentation; lightweight (hash + sig) messages are the
   default edge-node protocol.
 - **On-chip QRNG not yet commodity.** Current implementation uses public beacon
   HTTP APIs. The architecture is designed for hardware swap-in when chips arrive.
   The `entropy_source` field in every block makes the transition transparent and
   auditable.
 
 ---
 
 ## 8. Implementation Plan
 
 ### Phase 1: Core Backbone
 
 **Goal**: Establish the QAIA chain and PQC signing primitives.
 
 - Integrate `qaia.entropy` module (NIST Beacon [1] + CURBy [2] + ANU [3] fallback).
 - Implement **SHA3-256** [15] hash-chaining for the transport layer.
 - Port **ML-DSA-65** verification to ESP32 via liboqs [20]. Signing runs on the Hub only;
  edge nodes hold public keys and verify received blocks. ML-DSA-65 secret key (4,032 bytes)
  and signing stack pressure make signing impractical on constrained ESP32 SRAM alongside
  WiFi, LoRa, GPS, and display drivers.
 - **Deliverable**: `qaia.py` core module and `phantom-mesh` ESP32 firmware.
 
 ### Phase 2: Tactical Visualization
 
 **Goal**: Field-usable edge node with map rendering and peer verification.
 
 - Integrate **GPS (BN-220)** and **LoRa (SX1262)** with the CYD hardware.
 - Render offline map tiles from SD card with PQC-verified teammate icons.
 - Display signature validity indicator per received message.
 - **Deliverable**: CYD-Tactical-Interface firmware (~$15–25 hardware target).
 
 ### Phase 3: Hub & AI Integration (The NOMAD Layer)
 
 **Goal**: End-to-end attested inference accessible from edge nodes over the mesh.
 
 - Integrate **Ollama** with the QAIA block builder wrapper.
 - ESP32 nodes submit inference queries via the WiFi mesh; Hub responds with
   QAIA-signed blocks.
 - Cache NIST Beacon pulses locally for offline temporal anchoring.
 - **Deliverable**: Hub inference service with full QAIA chain output.
 
 ### Phase 4: Proving Ground — The Airsoft Test
 
 **Goal**: Stress-test the architecture on the cheapest possible hardware. If it
 holds here, it holds everywhere.
 
 #### Reference Implementation
 
 - **Hub**: One **cyberdeck** (Raspberry Pi 5, custom enclosure, battery-powered)
   running QAIA, Ollama, and NOMAD. Joins ESP-Mesh directly.
 - **Edge nodes**: 20× **ESP32 CYD units** — open AirTag clones with GPS, LoRa,
   WiFi mesh, and ML-DSA-65 signing. ~$15–25 each.
 
 #### Transport Hierarchy (automatic, no configuration)
 
 1. **ESP-Mesh (WiFi)** — primary: sub-second updates, full HTTP to cyberdeck
 2. **LoRa fallback** — activates automatically when a node leaves mesh range
 
 #### Scenario
 
 - 20+ players, 2 km terrain, 0% cell coverage
 - Each node broadcasts ML-DSA-65-signed GPS position over the mesh
 - Cyberdeck renders live tactical map; fields AI queries via HTTP
 - Full attestation chain accumulates in SQLite
 
 #### Success Metrics
 
 - Real-time map sync while in WiFi mesh range
 - Graceful LoRa degradation at mesh boundary — no data loss
 - AAR reconstructable from the chain alone — no human testimony required
 - **Total cost: ~$450–650** *(cyberdeck ~$150 + 20× CYD @ ~$15–25)*
 
 ### Phase 5: On-chip QRNG Integration
 
 **Goal**: Prove the architecture on dedicated quantum hardware as it becomes
 commodity — no protocol changes required, only entropy source swap.
 
 - Abstract `qaia.entropy` behind a hardware driver interface.
 - Integrate with available QRNG ICs (IDQ, Quside) as drop-in beacon replacement.
 - Implement QRNG-seeded Tailslayer [34] channel offsets on Hub hardware: DRAM
   channel placement becomes physically unpredictable per session, with the seed
   pulse ID attested in the chain. Rowhammer targeting requires predicting a
   quantum event.
 - Extend attestation schema to include `entropy_source: hardware|beacon|cached`.
 - **Target platforms**: NUC with QRNG PCIe card, future ESP32 variants with
   on-chip quantum noise source.
 
 ### Phase 6: Crisis Deployment (Scaling)
 
 **Goal**: Transition the stack to humanitarian and SAR operational contexts.
 
 - Repackage as a **Rapid Response Case**: cyberdeck + 6 CYD nodes, deployable
   in under 10 minutes.
 - Adapt use-case from "objective capture" to "shelter/triage management."
 - Document chain export and AAR generation for incident command handoff.
 
 ---
 
 ## 9. Dependencies
 
 ### Hub (Cyberdeck)
 

```
oqs-python          # ML-DSA-65 signatures via liboqs [20]
fips203             # ML-KEM-768 key exchange [5]
llama-cpp-python    # Local LLM inference [21]
httpx               # Async HTTP client for beacon APIs
rich                # Terminal dashboard [24]
sqlite3             # Attestation chain storage (Python stdlib)
```

 
 ### Edge Node (ESP32 Firmware)
 

```
liboqs (C)          # ML-DSA-65 verify [20]
TinyGPS++           # GPS NMEA parsing
RadioLib            # SX1262 LoRa driver
LovyanGFX           # CYD display rendering
```

 
 ---
 
 ## 10. References
 
 [1] NIST. "NIST Randomness Beacon." https://beacon.nist.gov/home
 
 [2] Leibrandt, D. et al. "CURBy — Colorado Ultrastable Random Beacon."
     JILA, University of Colorado Boulder. https://random.colorado.edu/
 
 [3] Symul, T., Assad, S.M., and Lam, P.K. "Real time demonstration of high
     bitrate quantum random number generation with coherent laser light."
     Applied Physics Letters, 98(23), 231103 (2011).
     https://doi.org/10.1063/1.3597793
 
 [4] NIST. "FIPS 204: Module-Lattice-Based Digital Signature Standard (ML-DSA)."
     Federal Information Processing Standards Publication 204 (2024).
     https://doi.org/10.6028/NIST.FIPS.204
 
 [5] NIST. "FIPS 203: Module-Lattice-Based Key-Encapsulation Mechanism Standard
     (ML-KEM)." Federal Information Processing Standards Publication 203 (2024).
     https://doi.org/10.6028/NIST.FIPS.203
 
 [6] Merkle, R.C. "A Certified Digital Signature." CRYPTO '89.
     https://doi.org/10.1007/0-387-34805-0_21
 
 [7] Gerganov, G. "llama.cpp." https://github.com/ggerganov/llama.cpp
 
 [8] Mosca, M. "Cybersecurity in an Era with Quantum Computers."
     IEEE Security & Privacy, 16(5), pp. 38–41 (2018).
     https://doi.org/10.1109/MSP.2018.3761723
 
 [9] C2PA. "C2PA Technical Specification." https://c2pa.org/specifications/
 
 [10] Herrero-Collantes, M. and Garcia-Escartin, J.C. "Quantum random number
      generators." Reviews of Modern Physics, 89(1), 015004 (2017).
      https://doi.org/10.1103/RevModPhys.89.015004
 
 [12] Langlois, A. and Stehlé, D. "Worst-Case to Average-Case Reductions for
      Module Lattices." Designs, Codes and Cryptography, 75(3) (2015).
      https://doi.org/10.1007/s10623-014-9938-4
 
 [13] Haber, S. and Stornetta, W.S. "How to Time-Stamp a Digital Document."
      Journal of Cryptology, 3(2), pp. 99–111 (1991).
      https://doi.org/10.1007/BF00196791
 
 [14] Laurie, B., Langley, A., and Kasper, E. "Certificate Transparency."
      RFC 6962, IETF (2013). https://doi.org/10.17487/RFC6962
 
 [15] NIST. "FIPS 202: SHA-3 Standard." (2015).
      https://doi.org/10.6028/NIST.FIPS.202
 
 [16] Bertoni, G. et al. "The Keccak Reference." Version 3.0 (2011).
      https://keccak.team/files/Keccak-reference-3.0.pdf
 
 [20] Open Quantum Safe Project. "liboqs." https://openquantumsafe.org/
 
 [21] Jllllll. "llama-cpp-python." https://github.com/abetlen/llama-cpp-python
 
 [24] McGugan, W. "Rich." https://github.com/Textualize/rich
 
 [25] NIST. "FIPS 140-3: Security Requirements for Cryptographic Modules." (2019).
      https://doi.org/10.6028/NIST.FIPS.140-3
 
 [26] Goldblum, M. et al. "Dataset Security for Machine Learning."
      IEEE TPAMI, 45(2), pp. 1563–1580 (2023).
      https://doi.org/10.1109/TPAMI.2022.3162397
 
 [27] Costan, V. and Devadas, S. "Intel SGX Explained." IACR ePrint 2016/086.
      https://eprint.iacr.org/2016/086
 
 [28] Meshtastic Project. https://meshtastic.org
 
 [29] Crosstalk Solutions. "Project NOMAD."
      https://github.com/Crosstalk-Solutions/project-nomad
 
 [30] Leitch, B. "ESP32-Cheap-Yellow-Display."
      https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display
 
 [31] Yao, F., Rakin, A.S., and Fan, D. "DeepHammer: Depleting the Intelligence
      of Deep Neural Networks through Targeted Chain of Bit Flips."
      USENIX Security 20, pp. 1463–1480 (2020).
      https://arxiv.org/abs/2003.13746
 
 [32] Cain, M. et al. "Shor's algorithm is possible with as few as 10,000
      reconfigurable atomic qubits." arXiv:2603.28627 (2026).
      https://arxiv.org/abs/2603.28627
 
 [33] Alvarez Morales, L. "Ten Thousand Qubits and a Prayer." Singular Grit (2026).
      https://singulargrit.substack.com/p/ten-thousand-qubits-and-a-prayer
 
 [34] Kirk, L. (LaurieWired). "Tailslayer: Reducing tail latency in RAM reads via DRAM channel
      hedging." GitHub repository (2025).
      https://github.com/LaurieWired/tailslayer
 
 ---
 
 ## License
 
 MIT