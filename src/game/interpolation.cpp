// Interpolation IDs for character limbs (Refresh Rate above the game's 30
// fps). Characters are drawn limb by limb, each limb a matrix load, and the
// limbs are drawn from depth-sorted buckets, so their order changes from
// frame to frame. RT64 pairs matrices between frames by guessing, and with
// several soldiers sharing one model it paired a limb with another soldier's:
// pieces of models jumped about on the in-between frames. Each limb's matrix
// is recorded here as it's built, with the character's pose entry for the
// joint (unique per character and joint, the same every frame) and the mesh,
// and the display list pass (src/game/display_list.cpp) gives its load that
// ID. Enemies of a kind in a group (a swarm) can share one pose, so each
// drawn skeleton is also an instance: once a display list has been walked,
// the instances it draws are numbered by matching them to the ones of their
// kind drawn last time, and the numbers go into their IDs. Hooks: snp.jp.toml.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace {
    uint32_t current_joint = 0;
    uint32_t generation = 0;

    // A skeleton as drawn: what kind it is and where its first matrix puts it.
    struct Instance {
        uint32_t key = 0;
        float pos[3] = {};
        uint32_t number = 0;
    };
    std::unordered_map<uint32_t, Instance> instances; // serial -> instance
    uint32_t current_instance = 0;
    uint32_t current_kind = 0;

    struct Entry {
        uint32_t id;
        uint32_t generation;
        uint32_t instance;
    };
    // Matrix address (from the frame's matrix pool) -> ID.
    std::unordered_map<uint32_t, Entry> matrices;

    // Per kind, the numbered instances drawn in the latest display list that
    // had any.
    struct Placed {
        float pos[3];
        uint32_t number;
    };
    std::unordered_map<uint32_t, std::vector<Placed>> kinds;

    float distance2(const float* a, const float* b) {
        float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
        return dx * dx + dy * dy + dz * dz;
    }

    // Number one kind's instances in a display list: pairs of one now and one
    // last time are taken closest first, each taking that one's number, and
    // the rest get new numbers. (Taking them one at a time in draw order let
    // an enemy that just appeared take the number of the one beside it, which
    // then paired with it on the in-between frames.)
    void number_kind(std::vector<Placed>& placed, const std::vector<Instance*>& now) {
        struct Pair {
            float d;
            uint32_t now;
            uint32_t before;
        };
        // Nothing moves this far in a frame (RT64 doesn't pair matrices this far apart either).
        const float max_distance = 256.0f;
        std::vector<Pair> pairs;
        for (uint32_t i = 0; i < now.size(); i++) {
            for (uint32_t j = 0; j < placed.size(); j++) {
                float d = distance2(now[i]->pos, placed[j].pos);
                if (d <= max_distance * max_distance) {
                    pairs.push_back({ d, i, j });
                }
            }
        }
        std::sort(pairs.begin(), pairs.end(), [](const Pair& x, const Pair& y) { return x.d < y.d; });
        std::vector<bool> numbered(now.size(), false);
        std::vector<bool> before_used(placed.size(), false);
        for (const Pair& pair : pairs) {
            if (numbered[pair.now] || before_used[pair.before]) {
                continue;
            }
            numbered[pair.now] = true;
            before_used[pair.before] = true;
            now[pair.now]->number = placed[pair.before].number;
        }
        auto used = [&](uint32_t n) {
            for (const Placed& p : placed) {
                if (p.number == n) return true;
            }
            for (uint32_t i = 0; i < now.size(); i++) {
                if (numbered[i] && now[i]->number == n) return true;
            }
            return false;
        };
        for (uint32_t i = 0; i < now.size(); i++) {
            if (!numbered[i]) {
                uint32_t n = 0;
                while (used(n)) n++;
                now[i]->number = n;
                numbered[i] = true;
            }
        }
        placed.clear();
        for (const Instance* inst : now) {
            Placed p;
            memcpy(p.pos, inst->pos, sizeof(p.pos));
            p.number = inst->number;
            placed.push_back(p);
        }
    }
}

// The skeleton walkers' entry points, before a skeleton is drawn: its
// skeleton and animation arguments, what kind of skeleton it is. (Not the key
// of its first matrix: which mesh comes first can change from frame to frame,
// and an insect that changed kind got a new number and jumped.)
extern "C" void snp_interp_instance(uint32_t skeleton, uint32_t animation) {
    current_instance++;
    uint64_t h = ((uint64_t)skeleton << 32 | animation) * 0x9E3779B97F4A7C15ull;
    current_kind = (uint32_t)(h >> 32) | 1;
}

// func_80032F2C, around drawing a joint's meshes: its pose entry, or 0 after.
extern "C" void snp_interp_joint(uint32_t joint) {
    current_joint = joint;
}

// func_80033758 (the other skeleton walker), the same with its two per-joint
// addresses folded into one key.
extern "C" void snp_interp_joint2(uint32_t pose, uint32_t channels) {
    current_joint = pose ^ (channels * 0x9E3779B1u) ^ 0x80000000u;
}

// func_80032AA0, as it builds a mesh's matrix: the matrix, the mesh and the
// float matrix's translation.
extern "C" void snp_interp_matrix(uint32_t mtx, uint32_t mesh, uint32_t x, uint32_t y, uint32_t z) {
    if (current_joint == 0) {
        return;
    }
    uint64_t h = ((uint64_t)current_joint << 32 | mesh) * 0x9E3779B97F4A7C15ull;
    uint32_t key = (uint32_t)(h >> 33);
    Instance& inst = instances[current_instance];
    if (inst.key == 0) {
        inst.key = current_kind != 0 ? current_kind : (key | 1);
        memcpy(&inst.pos[0], &x, 4);
        memcpy(&inst.pos[1], &y, 4);
        memcpy(&inst.pos[2], &z, 4);
        for (float& f : inst.pos) {
            if (!std::isfinite(f)) f = 0.0f;
        }
    }
    matrices[mtx & 0x1FFFFFFF] = { key, generation, current_instance };
}

namespace snp {
    // Once per graphics task, before its display list is walked. A frame's
    // matrices are built two tasks before its display list goes out (measured),
    // and the matrix pools are reused, so an entry counts only if it's at most
    // two tasks old; older ones are some other matrix's now.
    void interp_new_frame() {
        generation++;
        if (matrices.size() > 4096) {
            for (auto it = matrices.begin(); it != matrices.end();) {
                it = (generation - it->second.generation > 3) ? matrices.erase(it) : std::next(it);
            }
        }
        if (instances.size() > 1024) {
            for (auto it = instances.begin(); it != instances.end();) {
                it = (current_instance - it->first > 512) ? instances.erase(it) : std::next(it);
            }
        }
    }

    // The ID recorded for the matrix at addr, or 0xFFFFFFFF (automatic), and
    // its instance (0 if none).
    uint32_t interp_matrix_id(uint32_t addr, uint32_t& instance) {
        instance = 0;
        auto it = matrices.find(addr & 0x1FFFFFFF);
        if (it == matrices.end() || generation - it->second.generation > 2) {
            return 0xFFFFFFFF;
        }
        // SNP_INTERP_INSTANCES=0 leaves instances out (for comparing).
        static const bool instances_off = [] { const char* v = std::getenv("SNP_INTERP_INSTANCES"); return v && v[0] == '0'; }();
        auto inst = instances.find(it->second.instance);
        if (!instances_off && inst != instances.end() && inst->second.key != 0) {
            instance = it->second.instance;
        }
        return it->second.id;
    }

    // Once a display list has been walked: number the instances it draws.
    void interp_number_instances(const std::vector<uint32_t>& drawn) {
        std::unordered_map<uint32_t, std::vector<Instance*>> by_kind;
        for (uint32_t serial : drawn) {
            auto it = instances.find(serial);
            if (it != instances.end()) {
                by_kind[it->second.key].push_back(&it->second);
            }
        }
        for (auto& kind : by_kind) {
            number_kind(kinds[kind.first], kind.second);
        }
    }

    // The ID with its instance's number in it.
    uint32_t interp_numbered_id(uint32_t id, uint32_t instance) {
        auto it = instances.find(instance);
        uint32_t n = it != instances.end() ? it->second.number : 0;
        return (uint32_t)(((uint64_t)id << 32 | n) * 0x9E3779B97F4A7C15ull >> 33);
    }
}
