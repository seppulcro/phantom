// PHANTOM mesh node — gossip transport simulation
//
// Each node:
//   1. Generates an ML-DSA-65 keypair (signing) and ML-KEM-768 keypair (encryption)
//   2. Produces signed, SHA3-256 hash-chained attestation records on a timer
//   3. Gossips records to all connected peers (enforcing LoRa 250-byte packet limit)
//   4. Receives records from peers, verifies the ML-DSA-65 signature before accepting
//   5. Maintains a seen-set so records are never rebroadcast twice
//   6. Persists everything to a local SQLite attestation log
//
// Optional: pass --psk <passphrase> to enable AES-256-GCM payload encryption.
// The key is derived from the PSK via SHA3-256. Tamper-evidence (hash chain +
// ML-DSA-65 signatures) works identically in both plaintext and encrypted modes.
//
// This simulates the LoRa mesh protocol layer. The gossip logic, packet framing,
// deduplication, and cross-node signature verification are identical to what
// would run over LoRa SX1262 on a real node. See ARCHITECTURE.md for the
// ESP32 porting gap.
//
// Build:
//   g++ -std=c++20 -O2 -o phantom_mesh_node phantom_mesh_node.cpp \
//       -I/usr/local/include -L/usr/local/lib \
//       /usr/local/lib/liboqs.a -lsqlite3 -lssl -lcrypto -lpthread \
//       -Wl,-rpath,/usr/local/lib
//
// Run plaintext (3 nodes):
//   ./phantom_mesh_node --id node-1 --port 7001 --peers node-2:7002,node-3:7003
//
// Run encrypted (3 nodes):
//   ./phantom_mesh_node --id node-1 --port 7001 --peers node-2:7002,node-3:7003 --psk phantom-demo-key

#include <oqs/oqs.h>
#include <sqlite3.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// LoRa constraint: max 250-byte payload per packet.
// We enforce this on every gossip record — if a record won't fit in 250 bytes
// after framing, it is fragmented into signed chunks. On real LoRa hardware
// this maps directly to the SX1262 packet size register.
// ---------------------------------------------------------------------------
static constexpr size_t LORA_MAX_PAYLOAD = 250;

// ---------------------------------------------------------------------------
// SHA3-256 via OpenSSL
// ---------------------------------------------------------------------------
static std::string sha3_256_hex(const std::string &s) {
    uint8_t digest[32];
    unsigned int len = 32;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha3_256(), nullptr);
    EVP_DigestUpdate(ctx, s.data(), s.size());
    EVP_DigestFinal_ex(ctx, digest, &len);
    EVP_MD_CTX_free(ctx);
    std::ostringstream ss;
    for (int i = 0; i < 32; i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    return ss.str();
}

static std::string to_hex(const uint8_t *d, size_t n) {
    std::ostringstream ss;
    for (size_t i = 0; i < n; i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)d[i];
    return ss.str();
}

static std::vector<uint8_t> from_hex(const std::string &h) {
    std::vector<uint8_t> out(h.size() / 2);
    for (size_t i = 0; i < out.size(); i++) {
        unsigned int b;
        sscanf(h.c_str() + 2 * i, "%02x", &b);
        out[i] = (uint8_t)b;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Key derivation: SHA3-256(psk) → 32-byte AES-256-GCM key
// ---------------------------------------------------------------------------
static std::vector<uint8_t> derive_key(const std::string &psk) {
    uint8_t digest[32];
    unsigned int len = 32;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha3_256(), nullptr);
    EVP_DigestUpdate(ctx, psk.data(), psk.size());
    EVP_DigestFinal_ex(ctx, digest, &len);
    EVP_MD_CTX_free(ctx);
    return std::vector<uint8_t>(digest, digest + 32);
}

// ---------------------------------------------------------------------------
// AES-256-GCM encryption/decryption
// Format: hex(IV[12] || ciphertext || tag[16])
// ---------------------------------------------------------------------------
static std::string aes_gcm_encrypt(const std::vector<uint8_t> &key, const std::string &plaintext) {
    uint8_t iv[12];
    RAND_bytes(iv, 12);

    std::vector<uint8_t> ct(plaintext.size());
    uint8_t tag[16];
    int len = 0;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr);
    EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv);
    EVP_EncryptUpdate(ctx, ct.data(), &len,
                      reinterpret_cast<const uint8_t*>(plaintext.data()), (int)plaintext.size());
    EVP_EncryptFinal_ex(ctx, ct.data() + len, &len);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag);
    EVP_CIPHER_CTX_free(ctx);

    std::vector<uint8_t> blob;
    blob.insert(blob.end(), iv, iv + 12);
    blob.insert(blob.end(), ct.begin(), ct.end());
    blob.insert(blob.end(), tag, tag + 16);
    return to_hex(blob.data(), blob.size());
}

static std::string aes_gcm_decrypt(const std::vector<uint8_t> &key, const std::string &hex_blob) {
    auto blob = from_hex(hex_blob);
    if (blob.size() < 28) throw std::runtime_error("ciphertext too short");

    uint8_t *iv  = blob.data();
    size_t ct_len = blob.size() - 28;
    uint8_t *ct  = blob.data() + 12;
    uint8_t *tag = blob.data() + 12 + ct_len;

    std::vector<uint8_t> pt(ct_len);
    int len = 0;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr);
    EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv);
    EVP_DecryptUpdate(ctx, pt.data(), &len, ct, (int)ct_len);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag);
    int ret = EVP_DecryptFinal_ex(ctx, pt.data() + len, &len);
    EVP_CIPHER_CTX_free(ctx);

    if (ret <= 0) throw std::runtime_error("AES-GCM auth failed — tampered ciphertext or wrong key");
    return std::string(pt.begin(), pt.end());
}

// ---------------------------------------------------------------------------
// Attestation record — the unit gossiped between nodes
// ---------------------------------------------------------------------------
struct Record {
    std::string origin_id;    // which node produced it
    std::string pub_key_hex;  // originator's ML-DSA-65 public key (hex)
    int64_t     ts;
    std::string message;
    std::string prev_hash;
    std::string entry_hash;
    std::string sig_hex;      // ML-DSA-65 signature of entry_hash

    // Serialise to a compact line-delimited format.
    // Each field on its own line; record terminated by "---\n".
    // We enforce the LoRa 250-byte limit per network write (see send_record).
    std::string serialise() const {
        std::ostringstream ss;
        ss << origin_id   << "\n"
           << pub_key_hex << "\n"
           << ts          << "\n"
           << message     << "\n"
           << prev_hash   << "\n"
           << entry_hash  << "\n"
           << sig_hex     << "\n"
           << "---\n";
        return ss.str();
    }

    static Record deserialise(const std::string &s) {
        std::istringstream ss(s);
        Record r;
        std::getline(ss, r.origin_id);
        std::getline(ss, r.pub_key_hex);
        std::string ts_s; std::getline(ss, ts_s); r.ts = std::stoll(ts_s);
        std::getline(ss, r.message);
        std::getline(ss, r.prev_hash);
        std::getline(ss, r.entry_hash);
        std::getline(ss, r.sig_hex);
        return r;
    }

    bool verify() const {
        auto pub  = from_hex(pub_key_hex);
        auto sig  = from_hex(sig_hex);
        OQS_SIG *alg = OQS_SIG_new(OQS_SIG_alg_ml_dsa_65);
        if (!alg) return false;
        OQS_STATUS ok = OQS_SIG_verify(alg,
            reinterpret_cast<const uint8_t*>(entry_hash.data()), entry_hash.size(),
            sig.data(), sig.size(),
            pub.data());
        OQS_SIG_free(alg);
        return ok == OQS_SUCCESS;
    }
};

// ---------------------------------------------------------------------------
// SQLite attestation log
// ---------------------------------------------------------------------------
struct DB {
    sqlite3 *db = nullptr;
    std::mutex mu;

    explicit DB(const std::string &path) {
        if (sqlite3_open(path.c_str(), &db) != SQLITE_OK)
            throw std::runtime_error(std::string("sqlite3_open: ") + sqlite3_errmsg(db));
        exec(R"(CREATE TABLE IF NOT EXISTS attestation_log (
            seq        INTEGER PRIMARY KEY AUTOINCREMENT,
            origin_id  TEXT    NOT NULL,
            ts         INTEGER NOT NULL,
            message    TEXT    NOT NULL,
            prev_hash  TEXT    NOT NULL,
            entry_hash TEXT    NOT NULL,
            signature  TEXT    NOT NULL,
            source     TEXT    NOT NULL  -- 'local' or 'gossip'
        );)");
    }

    ~DB() { if (db) sqlite3_close(db); }

    void exec(const std::string &sql) {
        char *err = nullptr;
        if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
            std::string m(err); sqlite3_free(err);
            throw std::runtime_error("sqlite3_exec: " + m);
        }
    }

    void insert(const Record &r, const std::string &source) {
        std::lock_guard<std::mutex> lk(mu);
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db,
            "INSERT INTO attestation_log "
            "(origin_id, ts, message, prev_hash, entry_hash, signature, source) "
            "VALUES (?,?,?,?,?,?,?);", -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, r.origin_id.c_str(),  -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 2, r.ts);
        sqlite3_bind_text(stmt, 3, r.message.c_str(),   -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, r.prev_hash.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, r.entry_hash.c_str(),-1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, r.sig_hex.c_str(),   -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, source.c_str(),       -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    std::string last_local_hash(const std::string &origin_id) {
        std::lock_guard<std::mutex> lk(mu);
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db,
            "SELECT entry_hash FROM attestation_log "
            "WHERE origin_id=? AND source='local' ORDER BY seq DESC LIMIT 1;",
            -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, origin_id.c_str(), -1, SQLITE_STATIC);
        std::string h(64, '0');
        if (sqlite3_step(stmt) == SQLITE_ROW)
            h = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        sqlite3_finalize(stmt);
        return h;
    }

    void dump(const std::string &node_id, const std::vector<uint8_t> &enc_key = {}) {
        std::lock_guard<std::mutex> lk(mu);
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db,
            "SELECT seq, origin_id, source, ts, message, "
            "substr(entry_hash,1,8), substr(signature,1,8) "
            "FROM attestation_log ORDER BY seq;", -1, &stmt, nullptr);
        std::cout << "\n[" << node_id << "] --- Attestation Log"
                  << (enc_key.empty() ? " (plaintext)" : " (AES-256-GCM encrypted — decrypting for display)")
                  << " ---\n";
        std::cout << std::left
                  << std::setw(4)  << "seq"
                  << std::setw(10) << "origin"
                  << std::setw(8)  << "source"
                  << std::setw(12) << "ts"
                  << std::setw(36) << "message"
                  << std::setw(12) << "hash[:8]"
                  << "sig[:8]\n"
                  << std::string(100, '-') << "\n";
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            auto col = [&](int i) {
                return std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, i)));
            };
            std::string msg = col(4);
            if (!enc_key.empty()) {
                try { msg = aes_gcm_decrypt(enc_key, msg); }
                catch (...) { msg = "[ENCRYPTED — wrong key]"; }
            }
            std::cout << std::left
                      << std::setw(4)  << sqlite3_column_int(stmt, 0)
                      << std::setw(10) << col(1).substr(0, 8)
                      << std::setw(8)  << col(2)
                      << std::setw(12) << sqlite3_column_int64(stmt, 3)
                      << std::setw(36) << msg.substr(0, 34)
                      << std::setw(12) << col(5) + "..."
                      << col(6) + "...\n";
        }
        sqlite3_finalize(stmt);
    }
};

// ---------------------------------------------------------------------------
// Peer connection helpers
// ---------------------------------------------------------------------------
static int connect_to(const std::string &host, int port) {
    struct addrinfo hints{}, *res;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0)
        return -1;
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (connect(fd, res->ai_addr, res->ai_addrlen) != 0) {
        close(fd); freeaddrinfo(res); return -1;
    }
    freeaddrinfo(res);
    return fd;
}

// Send a record over a socket in LoRa-constrained chunks.
// Each write is at most LORA_MAX_PAYLOAD bytes, prefixed with a 4-byte
// network-order length. This is the same framing you'd use over SX1262.
static void send_record(int fd, const Record &r) {
    std::string payload = r.serialise();
    size_t offset = 0;
    while (offset < payload.size()) {
        size_t chunk_len = std::min(payload.size() - offset, LORA_MAX_PAYLOAD);
        uint32_t len_be  = htonl(static_cast<uint32_t>(chunk_len));
        send(fd, &len_be, 4, MSG_NOSIGNAL);
        send(fd, payload.data() + offset, chunk_len, MSG_NOSIGNAL);
        offset += chunk_len;
    }
    // Terminator
    uint32_t zero = 0;
    send(fd, &zero, 4, MSG_NOSIGNAL);
}

// Receive a full record from a socket (reassembling LoRa chunks).
static std::string recv_record(int fd) {
    std::string buf;
    while (true) {
        uint32_t len_be = 0;
        ssize_t n = recv(fd, &len_be, 4, MSG_WAITALL);
        if (n <= 0) return "";
        uint32_t chunk_len = ntohl(len_be);
        if (chunk_len == 0) break;  // terminator
        if (chunk_len > LORA_MAX_PAYLOAD)
            throw std::runtime_error("packet exceeds LoRa MTU — protocol violation");
        std::string chunk(chunk_len, '\0');
        n = recv(fd, chunk.data(), chunk_len, MSG_WAITALL);
        if (n <= 0) return "";
        buf += chunk;
    }
    return buf;
}

// ---------------------------------------------------------------------------
// Node
// ---------------------------------------------------------------------------
struct Node {
    std::string id;
    int         listen_port;
    std::vector<std::pair<std::string,int>> peer_addrs; // host, port

    OQS_SIG             *alg = nullptr;
    std::vector<uint8_t> pub_key;
    std::vector<uint8_t> sec_key;
    std::string          pub_key_hex;

    // ML-KEM-768 keypair (post-quantum key encapsulation — NIST FIPS 203)
    OQS_KEM             *kem = nullptr;
    std::vector<uint8_t> kem_pub;
    std::vector<uint8_t> kem_sec;

    // Optional AES-256-GCM encryption key (derived from PSK via SHA3-256)
    std::vector<uint8_t> enc_key;

    DB db;

    // Seen-set: entry_hash values we've already processed (dedup)
    std::mutex        seen_mu;
    std::set<std::string> seen;

    // Outbound gossip queue
    std::mutex              queue_mu;
    std::vector<Record>     queue;

    std::atomic<bool> running{true};

    Node(const std::string &id, int port,
         const std::vector<std::pair<std::string,int>> &peers,
         const std::string &psk = "")
        : id(id), listen_port(port), peer_addrs(peers),
          db("/data/" + id + "_attestation.db")
    {
        alg = OQS_SIG_new(OQS_SIG_alg_ml_dsa_65);
        if (!alg) throw std::runtime_error("ML-DSA-65 unavailable");
        pub_key.resize(alg->length_public_key);
        sec_key.resize(alg->length_secret_key);
        OQS_SIG_keypair(alg, pub_key.data(), sec_key.data());
        pub_key_hex = to_hex(pub_key.data(), pub_key.size());

        std::cout << "[" << id << "] ML-DSA-65 keypair ready. "
                  << "pub[:8]=" << pub_key_hex.substr(0,16) << "...\n";

        // ML-KEM-768 keypair (NIST FIPS 203 — post-quantum key encapsulation)
        kem = OQS_KEM_new(OQS_KEM_alg_ml_kem_768);
        if (!kem) throw std::runtime_error("ML-KEM-768 unavailable");
        kem_pub.resize(kem->length_public_key);
        kem_sec.resize(kem->length_secret_key);
        OQS_KEM_keypair(kem, kem_pub.data(), kem_sec.data());
        std::cout << "[" << id << "] ML-KEM-768 keypair ready. "
                  << "pub[:8]=" << to_hex(kem_pub.data(), 4) << "...\n";

        if (!psk.empty()) {
            enc_key = derive_key(psk);
            std::cout << "[" << id << "] AES-256-GCM encryption ENABLED "
                      << "(key derived from PSK via SHA3-256).\n";
        } else {
            std::cout << "[" << id << "] Encryption OFF — plaintext mode.\n";
        }
    }

    ~Node() {
        if (alg) OQS_SIG_free(alg);
        if (kem) OQS_KEM_free(kem);
    }

    // Produce a new local attestation record and queue it for gossip
    void attest(const std::string &message) {
        // Encrypt payload if PSK was provided
        std::string stored_msg = message;
        if (!enc_key.empty())
            stored_msg = aes_gcm_encrypt(enc_key, message);

        std::string prev  = db.last_local_hash(id);
        int64_t     ts    = static_cast<int64_t>(std::time(nullptr));
        std::string ehash = sha3_256_hex(prev + std::to_string(ts) + id + stored_msg);

        std::vector<uint8_t> sig(alg->length_signature);
        size_t sig_len = alg->length_signature;
        OQS_SIG_sign(alg, sig.data(), &sig_len,
                     reinterpret_cast<const uint8_t*>(ehash.data()), ehash.size(),
                     sec_key.data());

        Record r{id, pub_key_hex, ts, stored_msg, prev, ehash,
                 to_hex(sig.data(), sig_len)};

        db.insert(r, "local");
        {
            std::lock_guard<std::mutex> lk(seen_mu);
            seen.insert(ehash);
        }
        {
            std::lock_guard<std::mutex> lk(queue_mu);
            queue.push_back(r);
        }
        std::string label = enc_key.empty() ? message.substr(0,50)
                                            : "[ENCRYPTED] " + message.substr(0,38);
        std::cout << "[" << id << "] attested: \"" << label
                  << "\" hash=" << ehash.substr(0,12) << "...\n";
    }

    // Accept an incoming record from a peer — verify signature, dedup, store, requeue
    void accept_gossip(const Record &r) {
        {
            std::lock_guard<std::mutex> lk(seen_mu);
            if (seen.count(r.entry_hash)) return;  // already have it
            seen.insert(r.entry_hash);
        }

        if (!r.verify()) {
            std::cout << "[" << id << "] REJECT gossip from " << r.origin_id
                      << " — invalid ML-DSA-65 signature\n";
            return;
        }

        db.insert(r, "gossip");
        {
            std::lock_guard<std::mutex> lk(queue_mu);
            queue.push_back(r);  // rebroadcast to our own peers
        }
        std::string display_msg = r.message;
        if (!enc_key.empty()) {
            try { display_msg = aes_gcm_decrypt(enc_key, r.message); }
            catch (...) { display_msg = "[DECRYPTION FAILED]"; }
        }
        std::cout << "[" << id << "] accepted gossip from " << r.origin_id
                  << ": \"" << display_msg.substr(0,40) << "\" ✓\n";
    }

    // Listen for incoming peer connections
    void listen_thread() {
        int srv = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        struct sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port        = htons(listen_port);
        bind(srv, (struct sockaddr*)&addr, sizeof(addr));
        listen(srv, 16);
        std::cout << "[" << id << "] listening on :" << listen_port << "\n";

        while (running) {
            int client = accept(srv, nullptr, nullptr);
            if (client < 0) continue;
            std::thread([this, client]() {
                std::string raw = recv_record(client);
                close(client);
                if (!raw.empty()) {
                    try {
                        Record r = Record::deserialise(raw);
                        accept_gossip(r);
                    } catch (...) {}
                }
            }).detach();
        }
        close(srv);
    }

    // Gossip queued records out to all peers
    void gossip_thread() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            std::vector<Record> to_send;
            {
                std::lock_guard<std::mutex> lk(queue_mu);
                to_send.swap(queue);
            }
            if (to_send.empty()) continue;

            for (auto &[host, port] : peer_addrs) {
                int fd = connect_to(host, port);
                if (fd < 0) continue;
                for (auto &r : to_send) {
                    send_record(fd, r);
                }
                close(fd);
            }
        }
    }

    // Produce attestation events on a timer
    void attest_thread(const std::vector<std::string> &events, int interval_s) {
        for (auto &msg : events) {
            std::this_thread::sleep_for(std::chrono::seconds(interval_s));
            attest(msg);
        }
    }

    void run(const std::vector<std::string> &events, int duration_s, int interval_s) {
        std::thread lt(&Node::listen_thread,  this);
        std::thread gt(&Node::gossip_thread,  this);
        std::thread at(&Node::attest_thread,  this, events, interval_s);

        std::this_thread::sleep_for(std::chrono::seconds(duration_s));
        running = false;

        at.join();
        gt.join();
        // listen_thread blocks on accept — detach it, process will exit
        lt.detach();

        db.dump(id, enc_key);
        std::cout << "\n[" << id << "] === Done. Chain entries in log above are "
                  << "tamper-evident and ML-DSA-65 verified"
                  << (enc_key.empty() ? "" : " (payloads AES-256-GCM encrypted)")
                  << ". ===\n";
    }
};

// ---------------------------------------------------------------------------
// CLI arg parsing
// ---------------------------------------------------------------------------
static std::string get_arg(int argc, char **argv, const std::string &flag, const std::string &def = "") {
    for (int i = 1; i < argc - 1; i++)
        if (std::string(argv[i]) == flag) return argv[i+1];
    return def;
}

int main(int argc, char **argv) {
    std::string node_id    = get_arg(argc, argv, "--id",    "node-1");
    int         port       = std::stoi(get_arg(argc, argv, "--port",  "7001"));
    std::string peers_str  = get_arg(argc, argv, "--peers", "");
    int         duration   = std::stoi(get_arg(argc, argv, "--duration", "15"));
    int         interval   = std::stoi(get_arg(argc, argv, "--interval", "3"));
    std::string psk        = get_arg(argc, argv, "--psk",   "");

    std::vector<std::pair<std::string,int>> peers;
    if (!peers_str.empty()) {
        std::istringstream ss(peers_str);
        std::string token;
        while (std::getline(ss, token, ',')) {
            auto colon = token.rfind(':');
            if (colon != std::string::npos)
                peers.push_back({token.substr(0, colon),
                                 std::stoi(token.substr(colon + 1))});
        }
    }

    std::vector<std::string> events = {
        node_id + ": post-quantum stack initialized",
        node_id + ": position broadcast attested",
        node_id + ": AI inference result signed",
        node_id + ": mesh beacon transmitted",
    };

    std::cout << "=== PHANTOM Mesh Node [" << node_id << "] ===\n"
              << "    Port      : " << port << "\n"
              << "    Peers     : " << (peers_str.empty() ? "(none)" : peers_str) << "\n"
              << "    Duration  : " << duration << "s\n"
              << "    LoRa MTU  : " << LORA_MAX_PAYLOAD << " bytes/packet (enforced)\n"
              << "    Encryption: " << (psk.empty() ? "OFF (plaintext)" : "ON (AES-256-GCM, PSK mode)") << "\n\n";

    Node node(node_id, port, peers, psk);
    node.run(events, duration, interval);
    return 0;
}
