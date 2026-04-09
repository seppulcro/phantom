// PHANTOM node — proof-of-concept
// Demonstrates: ML-DSA-65 keygen + sign, SHA3-256 hash chain, SQLite attestation log
//
// Build:
//   g++ -std=c++20 -O2 -o phantom_node phantom_node.cpp \
//       -I/usr/local/include -L/usr/local/lib -loqs -lsqlite3 -Wl,-rpath,/usr/local/lib
//
// Run:
//   ./phantom_node [message ...]     (default: runs 3 built-in demo events)

#include <oqs/oqs.h>
#include <sqlite3.h>
#include <openssl/evp.h>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// SHA3-256 via OpenSSL EVP
// ---------------------------------------------------------------------------

static std::string sha3_256_hex(const std::string &s) {
    uint8_t digest[32];
    unsigned int digest_len = 32;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha3_256(), nullptr);
    EVP_DigestUpdate(ctx, s.data(), s.size());
    EVP_DigestFinal_ex(ctx, digest, &digest_len);
    EVP_MD_CTX_free(ctx);
    std::ostringstream ss;
    for (int i = 0; i < 32; i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    return ss.str();
}

// ---------------------------------------------------------------------------
// Hex helper
// ---------------------------------------------------------------------------

static std::string to_hex(const uint8_t *data, size_t len) {
    std::ostringstream ss;
    for (size_t i = 0; i < len; i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
    return ss.str();
}

// ---------------------------------------------------------------------------
// SQLite attestation log
// ---------------------------------------------------------------------------

struct DB {
    sqlite3 *db = nullptr;

    explicit DB(const std::string &path) {
        if (sqlite3_open(path.c_str(), &db) != SQLITE_OK)
            throw std::runtime_error(std::string("sqlite3_open: ") + sqlite3_errmsg(db));
        exec(R"(CREATE TABLE IF NOT EXISTS attestation_log (
            seq         INTEGER PRIMARY KEY AUTOINCREMENT,
            ts          INTEGER NOT NULL,
            message     TEXT    NOT NULL,
            prev_hash   TEXT    NOT NULL,
            entry_hash  TEXT    NOT NULL,
            signature   TEXT    NOT NULL
        );)");
    }

    ~DB() { if (db) sqlite3_close(db); }

    void exec(const std::string &sql) {
        char *err = nullptr;
        if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
            std::string msg(err); sqlite3_free(err);
            throw std::runtime_error("sqlite3_exec: " + msg);
        }
    }

    void insert(int64_t ts, const std::string &message,
                const std::string &prev_hash, const std::string &entry_hash,
                const std::string &sig_hex) {
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db,
            "INSERT INTO attestation_log (ts, message, prev_hash, entry_hash, signature) "
            "VALUES (?, ?, ?, ?, ?);", -1, &stmt, nullptr);
        sqlite3_bind_int64(stmt, 1, ts);
        sqlite3_bind_text(stmt, 2, message.c_str(),   -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, prev_hash.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, entry_hash.c_str(),-1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, sig_hex.c_str(),   -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    std::string last_hash() {
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db,
            "SELECT entry_hash FROM attestation_log ORDER BY seq DESC LIMIT 1;",
            -1, &stmt, nullptr);
        std::string h(64, '0');
        if (sqlite3_step(stmt) == SQLITE_ROW)
            h = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        sqlite3_finalize(stmt);
        return h;
    }

    void dump() {
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db,
            "SELECT seq, ts, message, prev_hash, entry_hash, substr(signature,1,16) "
            "FROM attestation_log;", -1, &stmt, nullptr);
        std::cout << "\n--- Attestation Log ---\n";
        std::cout << std::left
                  << std::setw(4)  << "seq"
                  << std::setw(14) << "timestamp"
                  << std::setw(40) << "message"
                  << std::setw(20) << "prev_hash[:8]"
                  << std::setw(20) << "entry_hash[:8]"
                  << "sig[:16]\n"
                  << std::string(110, '-') << "\n";
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            auto col = [&](int i) {
                return std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, i)));
            };
            std::cout << std::left
                      << std::setw(4)  << sqlite3_column_int(stmt, 0)
                      << std::setw(14) << sqlite3_column_int64(stmt, 1)
                      << std::setw(40) << col(2).substr(0, 38)
                      << std::setw(20) << col(3).substr(0, 8) + "..."
                      << std::setw(20) << col(4).substr(0, 8) + "..."
                      << col(5) << "...\n";
        }
        sqlite3_finalize(stmt);
    }
};

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char **argv) {
    std::cout << "=== PHANTOM Node — Post-Quantum Attestation PoC ===\n\n";

    // Key generation (ML-DSA-65 / NIST FIPS 204)
    OQS_SIG *alg = OQS_SIG_new(OQS_SIG_alg_ml_dsa_65);
    if (!alg) throw std::runtime_error("ML-DSA-65 unavailable in this liboqs build");

    std::vector<uint8_t> pub(alg->length_public_key);
    std::vector<uint8_t> sec(alg->length_secret_key);

    std::cout << "[keygen] Algorithm : ML-DSA-65 (NIST FIPS 204)\n"
              << "[keygen] Pub key   : " << alg->length_public_key << " bytes\n"
              << "[keygen] Sec key   : " << alg->length_secret_key << " bytes\n";

    if (OQS_SIG_keypair(alg, pub.data(), sec.data()) != OQS_SUCCESS)
        throw std::runtime_error("keypair generation failed");

    std::cout << "[keygen] Public key (first 16 bytes): "
              << to_hex(pub.data(), 16) << "...\n\n";

    // Open attestation DB
    DB db("phantom_attestation.db");

    // Events to attest
    std::vector<std::string> events;
    if (argc > 1) {
        for (int i = 1; i < argc; i++) events.emplace_back(argv[i]);
    } else {
        events = {
            "PHANTOM node boot — post-quantum stack initialized",
            "AI inference result: threat assessment NEGATIVE",
            "Operator order: RTB acknowledged and signed",
        };
    }

    // Hash-chain + sign each event
    for (const auto &message : events) {
        std::string prev_hash  = db.last_hash();
        int64_t     ts         = static_cast<int64_t>(std::time(nullptr));
        std::string entry_hash = sha3_256_hex(prev_hash + std::to_string(ts) + message);

        std::vector<uint8_t> sig(alg->length_signature);
        size_t sig_len = alg->length_signature;
        const auto *hash_bytes = reinterpret_cast<const uint8_t *>(entry_hash.data());

        if (OQS_SIG_sign(alg, sig.data(), &sig_len,
                         hash_bytes, entry_hash.size(), sec.data()) != OQS_SUCCESS)
            throw std::runtime_error("signing failed");

        OQS_STATUS ok = OQS_SIG_verify(alg, hash_bytes, entry_hash.size(),
                                        sig.data(), sig_len, pub.data());

        db.insert(ts, message, prev_hash, entry_hash, to_hex(sig.data(), sig_len));

        std::cout << "[attest] \"" << message.substr(0, 55) << "\"\n"
                  << "         entry_hash : " << entry_hash.substr(0, 16) << "...\n"
                  << "         signature  : " << to_hex(sig.data(), 16)   << "...\n"
                  << "         verified   : " << (ok == OQS_SUCCESS ? "OK" : "FAIL") << "\n\n";
    }

    db.dump();
    std::cout << "\n=== Chain intact. Anyone with the public key can verify offline. ===\n";

    OQS_SIG_free(alg);
    return 0;
}
