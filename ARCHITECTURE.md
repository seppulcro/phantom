# PHANTOM Architecture

## What is built and working

The PHANTOM cryptographic core is implemented, tested, and reproducible via `docker compose up --build`.

### `phantom_node` — standalone attestation

Generates an ML-DSA-65 keypair, signs a SHA3-256 hash-chained sequence of events,
and writes a tamper-evident SQLite attestation log. Any holder of the public key can
verify every entry independently, offline, without contacting any server.

### `phantom_mesh_node` — gossip mesh simulation

Extends the standalone node with a TCP gossip transport that simulates the LoRa mesh
protocol layer:

- Each node listens for incoming records from peers
- Signed records are gossiped to all connected peers on a 1-second cycle
- Every incoming record's ML-DSA-65 signature is verified before it is accepted
- A seen-set prevents any record from being forwarded twice
- All writes are flushed to the local SQLite attestation log, tagged `local` or `gossip`
- **The LoRa 250-byte packet constraint is enforced at the transport layer** —
  payloads are split into ≤250-byte length-prefixed chunks, exactly as the SX1262
  radio would transmit them

After running `docker compose up --build` with three nodes, every node's log contains
records from all three nodes — all ML-DSA-65 verified.

---

## Mapping to real hardware

### What maps 1:1 to an ESP32 + SX1262 LoRa node

| Simulation layer | Real hardware equivalent | Notes |
|---|---|---|
| TCP gossip transport | LoRa SX1262 radio | Same framing: length-prefixed packets, ≤250 bytes |
| Docker bridge network | LoRa RF mesh | Same gossip protocol, same dedup logic |
| 250-byte MTU enforcement | SX1262 packet size register | Already enforced in simulation |
| ML-DSA-65 signature verification | Same | Protocol is hardware-agnostic |
| SHA3-256 hash chain | Same | Algorithm is hardware-agnostic |
| Seen-set deduplication | Same | Data structure is hardware-agnostic |
| SQLite attestation log | LittleFS or SPIFFS on flash | Schema is identical |

### What needs porting for ESP32

| Component | Simulation | ESP32 port required |
|---|---|---|
| **ML-DSA-65 signing** | liboqs (~10 MB RAM) | [pqm4](https://github.com/mupq/pqm4) — optimised ML-DSA for Cortex-M; or sign on the cyberdeck and relay via ESP32 |
| **SHA3-256** | OpenSSL EVP | [tiny-sha3](https://github.com/mjosaarinen/tiny-sha3) — 200-line C, runs on any MCU |
| **SQLite** | libsqlite3 | LittleFS + a minimal write-ahead log, or ship logs to the cyberdeck for storage |
| **Key storage** | Filesystem | ESP32 NVS (non-volatile storage) or encrypted flash partition |
| **Entropy** | `/dev/urandom` (CSPRNG) | `esp_fill_random()` (hardware RNG in ESP32-S3) or `/dev/qrng0` on the cyberdeck |

### Recommended deployment split

```
┌─────────────────────────────────┐      LoRa SX1262
│  Cyberdeck (RPi 5 / NUC)        │ ◄──────────────► ESP32 relay nodes
│  - liboqs (full ML-DSA-65)      │                  - Transport only
│  - phantom_mesh_node            │                  - Forward packets
│  - SQLite attestation log       │                  - No signing needed
│  - llama.cpp (AI inference)     │                  - Cheap, many nodes
└─────────────────────────────────┘                  ~$15 each
```

The cyberdeck holds the signing keys and attestation log. ESP32 nodes act as
LoRa relay points — they forward packets verbatim without needing to verify
signatures themselves (though they could, if pqm4 is ported).

---

## What PHANTOM does NOT claim

- **It is not a quantum computer.** It uses post-quantum *resistant* cryptography —
  classical algorithms designed to withstand attacks from future quantum computers.
- **It does not replace the internet.** It operates without internet infrastructure,
  but it is a mesh transport for attestation records, not a general-purpose network.
- **The DRAM/Tailslayer integration is speculative.** The tailslayer-phantom fork
  randomises channel offsets from a set of hardware-valid XOR bit positions. The
  latency impact of this is unbenchmarked. See the tailslayer-phantom README.
- **QRNG hardware is optional.** The system uses `getrandom()` (Linux kernel CSPRNG)
  by default. If a hardware QRNG is present at `/dev/qrng0`, it is used automatically.
  The security of the attestation chain does not depend on QRNG hardware.

---

## Airsoft field deployment

The intended proof-of-concept field test: a cyberdeck + 2–3 ESP32+SX1262 nodes,
carried in a pack, operating on an airsoft field with zero cell coverage.

Every AI inference result, position broadcast, and operator order would be signed
and hash-chained in real time. After the session, the attestation log can be
verified by anyone with the public key — proving what was said, when, and that
nobody altered the record.

The cryptographic guarantees are identical whether you're running `docker compose up`
on a laptop or carrying hardware through woodland.
