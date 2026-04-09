
 # PHANTOM — Post-quantum Hash-chained Attestation Network for Tactical Offline Mesh

 ![Palantir at home](https://files.catbox.moe/crtpxu.jpg)

 ---

 > — Can we get Palantir?
 >
 > — We have Palantir at home.
 >
 > **Palantir at home:** a cyberdeck and some ESP32s from AliExpress.

 ---

 **A working, post-quantum, tamper-evident mesh network for environments where the internet isn't available, isn't trusted, or doesn't exist.**

 PHANTOM is what you'd get if a **flight data recorder**, a **notary public**, and a **LoRa mesh** had a baby — and the notary used physics instead of a stamp.
 Every AI output, every position broadcast, every operator order is cryptographically signed with a post-quantum algorithm and hash-chained — so anyone with your public key can verify what was said, when, and that nobody altered the record. Offline. Forever.

 It isn't a replacement for the internet. It's what secure communication looks like **when the internet isn't there** — and what the internet's foundation should have looked like from the start: tamper-evident by design, no central authority, post-quantum from day one. In normal operations you use the internet. When towers go down, infrastructure fails, or you need provable tamper-evidence the internet can't provide — that's PHANTOM's domain.

 No cloud. No cell tower. No internet required. Fits in a backpack. ~$650 of commodity hardware.

 **The cryptographic core is implemented and validated.** The attestation layer (ML-DSA-65 signatures + SHA3-256 hash chain + SQLite) builds from source and runs. The mesh transport (LoRa/ESP-NOW) and QRNG hardware integration are next.

 | Property | Status | Standard / Mechanism |
 |---|---|---|
 | **Post-quantum signatures** | ✅ Working | NIST FIPS 204 (ML-DSA-65) — lattice-based, Shor-resistant |
 | **Post-quantum key encapsulation** | ✅ Working | NIST FIPS 203 (ML-KEM-768) — keypair generated per node at startup |
 | **Payload encryption** | ✅ Working | AES-256-GCM, key derived from PSK via SHA3-256 (`--psk` flag) |
 | **Tamper-evident audit chain** | ✅ Working | SHA3-256 hash chain + ML-DSA-65 signatures + SQLite |
 | **Reproducible build** | ✅ Working | `docker compose up --build` — 3 isolated nodes, all chains verified |
 | **DRAM layout unpredictability** | 🔬 Research | Tailslayer fork: entropy-seeded channel offsets; latency impact unbenchmarked |
 | **Quantum-seeded entropy** | 🔌 Ready | Code path exists; requires IDQ/Quside hardware at `/dev/qrng0` |
 | **Offline mesh transport** | 🔧 Planned | LoRa SX1262 + ESP-NOW — no cell towers, no cloud, no DNS |

 > *The goal: prove what your AI said, when it said it, and that nobody picked the answer.*
 > *Prove what your hardware is doing, which memory it's touching, and that nobody predicted the layout.*
 > *Prove what your operator said, when they said it, and that nobody faked the order.*
 >
 > If built as specified: anyone with your public key could verify all of it — independently, offline, forever.

  ---

 ## Quickstart

 ```bash
 git clone https://github.com/seppulcro/phantom
 cd phantom

 # Plaintext mode — tamper-evident, readable messages in SQLite
 docker compose up --build

 # Encrypted mode — AES-256-GCM payloads, tamper-evidence still works
 docker compose -f docker-compose.encrypted.yml up --build
 ```

 That's it. Docker builds liboqs (ML-DSA-65/ML-KEM-768) from source inside the container, compiles `phantom_mesh_node`, and runs three independent nodes — each producing a verified post-quantum attestation chain written to `./output/<node>/phantom_attestation.db`.

 In encrypted mode, the SQLite log stores ciphertext blobs. The hash chain and ML-DSA-65 signatures are over the ciphertext — tamper-evidence holds whether or not you have the key.

 No hardware required. No cloud. If you have a hardware QRNG at `/dev/qrng0`, the entropy source upgrades automatically.


 ---

 ## Further reading

 - [ARCHITECTURE.md](ARCHITECTURE.md) — hardware mapping, ESP32 porting guide, what PHANTOM does not claim
 - [docs/DESIGN.md](docs/DESIGN.md) — full design specification, threat model, security properties, references
