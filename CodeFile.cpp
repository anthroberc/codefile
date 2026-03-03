#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <cstring>
#include <iomanip>
#include <cstdint>
#include <stdexcept>
#include <system_error>

namespace fs = std::filesystem;

static constexpr uint32_t SHA256_K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

class SHA256 {
    uint32_t st[8]{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                   0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    uint8_t  blk[64]{};
    uint64_t bits{0};
    uint32_t blen{0};

    static constexpr uint32_t rotr(uint32_t x, uint32_t n) noexcept {
        return (x >> n) | (x << (32u - n));
    }

    void compress(const uint8_t* b) noexcept {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = (uint32_t(b[i*4])<<24)|(uint32_t(b[i*4+1])<<16)
                  |(uint32_t(b[i*4+2])<<8)|uint32_t(b[i*4+3]);
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(w[i-15],7)^rotr(w[i-15],18)^(w[i-15]>>3);
            uint32_t s1 = rotr(w[i-2],17)^rotr(w[i-2],19)^(w[i-2]>>10);
            w[i] = w[i-16]+s0+w[i-7]+s1;
        }
        uint32_t a=st[0],b2=st[1],c=st[2],d=st[3],e=st[4],f=st[5],g=st[6],h=st[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t t1 = h+(rotr(e,6)^rotr(e,11)^rotr(e,25))+((e&f)^(~e&g))+SHA256_K[i]+w[i];
            uint32_t t2 = (rotr(a,2)^rotr(a,13)^rotr(a,22))+((a&b2)^(a&c)^(b2&c));
            h=g; g=f; f=e; e=d+t1; d=c; c=b2; b2=a; a=t1+t2;
        }
        st[0]+=a; st[1]+=b2; st[2]+=c; st[3]+=d;
        st[4]+=e; st[5]+=f;  st[6]+=g; st[7]+=h;
    }

public:
    void update(const uint8_t* data, size_t n) noexcept {
        for (size_t i = 0; i < n; ++i) {
            blk[blen++] = data[i];
            bits += 8;
            if (blen == 64) { compress(blk); blen = 0; }
        }
    }

    std::array<uint8_t,32> finalize() noexcept {
        uint8_t pad = 0x80;
        update(&pad, 1);
        uint8_t z = 0;
        while (blen != 56) update(&z, 1);
        uint8_t lb[8]; uint64_t bc = bits;
        for (int i = 7; i >= 0; --i) { lb[i] = bc & 0xff; bc >>= 8; }
        update(lb, 8);
        std::array<uint8_t,32> d{};
        for (int i = 0; i < 8; ++i) {
            d[i*4+0]=(st[i]>>24)&0xff; d[i*4+1]=(st[i]>>16)&0xff;
            d[i*4+2]=(st[i]>>8)&0xff;  d[i*4+3]=st[i]&0xff;
        }
        return d;
    }
};

struct FileEntry {
    fs::path    abs;
    std::string rel;
    uintmax_t   size{0};
};

static constexpr size_t IO_CHUNK = 65536;
static constexpr const char HEX_CHARS[] = "0123456789abcdef";

static std::string to_fwd(std::string s) {
    for (char& c : s) if (c == '\\') c = '/';
    return s;
}

static std::string trim_ws(const std::string& s) {
    static constexpr const char* WS = " \t\r\n";
    const size_t a = s.find_first_not_of(WS);
    if (a == std::string::npos) return {};
    return s.substr(a, s.find_last_not_of(WS) - a + 1);
}

static std::string utc_now() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm u{};
#ifdef _WIN32
    gmtime_s(&u, &t);
#else
    gmtime_r(&t, &u);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &u);
    return buf;
}

static std::string fmt_mb(uintmax_t bytes) {
    std::ostringstream o;
    o << std::fixed << std::setprecision(2)
      << (static_cast<double>(bytes) / (1024.0 * 1024.0));
    return o.str();
}

static std::string compute_id(const fs::path& p, bool& fail) {
    std::ifstream in(p, std::ios::binary);
    if (!in) { fail = true; return {}; }
    SHA256 h;
    uint8_t buf[IO_CHUNK];
    while (in) {
        in.read(reinterpret_cast<char*>(buf), IO_CHUNK);
        const auto n = static_cast<size_t>(in.gcount());
        if (n > 0) h.update(buf, n);
    }
    if (in.bad()) { fail = true; return {}; }
    fail = false;
    const auto d = h.finalize();
    char out[13]{};
    for (int i = 0; i < 6; ++i) {
        out[i*2]   = HEX_CHARS[d[i] >> 4];
        out[i*2+1] = HEX_CHARS[d[i] & 0xf];
    }
    return std::string(out, 12);
}

static bool stream_file(const fs::path& p, std::ostream& out, bool& read_err) {
    read_err = false;
    std::ifstream in(p, std::ios::binary);
    if (!in) { read_err = true; return false; }
    char buf[IO_CHUNK];
    while (in) {
        in.read(buf, IO_CHUNK);
        const auto n = in.gcount();
        if (n > 0) {
            out.write(buf, n);
            if (!out) return false;
        }
    }
    if (in.bad()) { read_err = true; return false; }
    return true;
}

static std::vector<FileEntry> collect_files(const fs::path& root) {
    std::vector<FileEntry> v;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(
                root, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec))
    {
        if (ec) { ec.clear(); continue; }
        const auto& e = *it;
        std::error_code ec2;
        if (e.is_symlink(ec2) || ec2) { ec2.clear(); continue; }
        if (!e.is_regular_file(ec2) || ec2) { ec2.clear(); continue; }
        auto rel = fs::relative(e.path(), root, ec2);
        if (ec2) { ec2.clear(); continue; }
        auto rs = to_fwd(rel.generic_string());
        if (rs == "CodeFile" || rs == "CodeFile.tmp") continue;
        uintmax_t sz = e.file_size(ec2);
        if (ec2) { sz = 0; ec2.clear(); }
        v.push_back({e.path(), std::move(rs), sz});
    }
    std::sort(v.begin(), v.end(),
        [](const FileEntry& a, const FileEntry& b) { return a.rel < b.rel; });
    return v;
}

struct Node {
    std::string       name;
    bool              is_dir{false};
    std::vector<Node> kids;
};

static Node& get_or_add(Node& parent, const std::string& name, bool is_dir) {
    for (auto& c : parent.kids) if (c.name == name) return c;
    parent.kids.push_back({name, is_dir, {}});
    return parent.kids.back();
}

static void tree_insert(Node& root, const std::string& rel) {
    std::vector<std::string> parts;
    std::string tok;
    for (const char c : rel) {
        if (c == '/') {
            if (!tok.empty()) { parts.push_back(std::move(tok)); tok.clear(); }
        } else {
            tok += c;
        }
    }
    if (!tok.empty()) parts.push_back(std::move(tok));
    Node* cur = &root;
    for (size_t i = 0; i < parts.size(); ++i)
        cur = &get_or_add(*cur, parts[i], i + 1 < parts.size());
}

static void tree_sort(Node& n) {
    std::sort(n.kids.begin(), n.kids.end(), [](const Node& a, const Node& b) {
        if (a.is_dir != b.is_dir) return a.is_dir > b.is_dir;
        return a.name < b.name;
    });
    for (auto& c : n.kids) tree_sort(c);
}

static void tree_render(const Node& n, std::ostream& o,
                        const std::string& pfx, bool last, bool is_root)
{
    if (!is_root) {
        o << pfx << (last ? "\\-- " : "|-- ") << n.name << (n.is_dir ? "/" : "") << '\n';
    } else {
        o << n.name << "/\n";
    }
    const std::string next = is_root ? "" : pfx + (last ? "    " : "|   ");
    for (size_t i = 0; i < n.kids.size(); ++i)
        tree_render(n.kids[i], o, next, i + 1 == n.kids.size(), false);
}

static std::string build_tree(const fs::path& root, const std::vector<FileEntry>& files) {
    std::string rname = root.filename().string();
    if (rname.empty()) rname = root.string();
    Node root_node{std::move(rname), true, {}};
    for (const auto& f : files) tree_insert(root_node, f.rel);
    tree_sort(root_node);
    std::ostringstream o;
    tree_render(root_node, o, "", true, true);
    return o.str();
}

static void safe_remove(const fs::path& p) {
    std::error_code ec;
    fs::remove(p, ec);
}

[[noreturn]] static void fatal(const std::string& msg, const fs::path& tmp = {}) {
    std::cerr << "Error: " << msg << '\n';
    if (!tmp.empty()) safe_remove(tmp);
    std::exit(1);
}

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::cout << "Enter absolute path to project folder: " << std::flush;
    std::string raw;
    if (!std::getline(std::cin, raw))
        fatal("failed to read input from stdin");

    const std::string input = trim_ws(raw);
    if (input.empty()) fatal("empty path provided");

    fs::path root;
    try { root = fs::path(input); }
    catch (const std::exception& ex) {
        fatal(std::string("invalid path: ") + ex.what());
    }

    std::error_code ec;
    root = fs::canonical(root, ec);
    if (ec) fatal("cannot resolve path '" + input + "': " + ec.message());

    if (!fs::exists(root, ec) || ec)
        fatal("path does not exist: " + root.string());
    if (!fs::is_directory(root, ec) || ec)
        fatal("path is not a directory: " + root.string());
    {
        [[maybe_unused]] fs::directory_iterator probe(root, ec);
        if (ec) fatal("directory not accessible: " + ec.message());
    }

    const auto   files       = collect_files(root);
    uintmax_t    total_bytes = 0;
    for (const auto& f : files) total_bytes += f.size;

    std::string root_name = root.filename().string();
    if (root_name.empty()) root_name = root.string();

    const fs::path out_path = root / "CodeFile";
    const fs::path tmp_path = root / "CodeFile.tmp";

    safe_remove(tmp_path);

    {
        std::ofstream out(tmp_path, std::ios::binary | std::ios::trunc);
        if (!out)
            fatal("cannot create temporary file in: " + root.string());

        const std::string tree  = build_tree(root, files);
        const std::string stamp = utc_now();

        out << "ROOT_NAME: "     << root_name          << '\n'
            << "CREATED_UTC: "   << stamp              << '\n'
            << "TOTAL_SIZE_MB: " << fmt_mb(total_bytes) << '\n'
            << '\n'
            << "START_STRUCTURE\n"
            << tree
            << "END_STRUCTURE\n"
            << '\n';

        if (!out) fatal("write failure in header section", tmp_path);

        for (const auto& f : files) {
            bool id_fail = false;
            const std::string id = compute_id(f.abs, id_fail);
            if (id_fail)
                fatal("cannot read file for hashing: " + f.abs.string(), tmp_path);

            out << "FILE_START " << f.rel << ' ' << id << '\n';
            if (!out) fatal("write failure before: " + f.rel, tmp_path);

            bool read_err = false;
            if (!stream_file(f.abs, out, read_err)) {
                if (read_err)
                    fatal("cannot read file content: " + f.abs.string(), tmp_path);
                else
                    fatal("write failure while streaming: " + f.rel, tmp_path);
            }

            out << '\n' << "FILE_END " << f.rel << ' ' << id << '\n' << '\n';
            if (!out) fatal("write failure after: " + f.rel, tmp_path);
        }

        out.flush();
        if (!out) fatal("final flush failed", tmp_path);
    }

    fs::rename(tmp_path, out_path, ec);
    if (ec) fatal("cannot finalize CodeFile: " + ec.message(), tmp_path);

    return 0;
}
