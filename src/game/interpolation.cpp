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
// drawn skeleton is also an instance, numbered by matching it to the nearest
// one of its kind in the previous frame, and the number goes into its IDs.
// Hooks: snp.jp.toml.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace {
    uint32_t current_joint = 0;
    uint32_t generation = 0;

    // A skeleton as drawn: the key of its first matrix (what kind it is) and
    // where that matrix puts it.
    struct Instance {
        uint32_t key = 0;
        float pos[3] = {};
        uint32_t built = 0; // generation
        uint32_t number_generation = 0xFFFFFFFF;
        uint32_t number = 0;
    };
    std::unordered_map<uint32_t, Instance> instances; // serial -> instance
    uint32_t current_instance = 0;

    struct Entry {
        uint32_t id;
        uint32_t generation;
        uint32_t instance;
    };
    // Matrix address (from the frame's matrix pool) -> ID.
    std::unordered_map<uint32_t, Entry> matrices;

    // Per kind, the numbered instances drawn in the latest frame it was in and
    // the frame before.
    struct Placed {
        float pos[3];
        uint32_t number;
    };
    struct Kind {
        uint32_t generation = 0xFFFFFFFF;
        std::vector<Placed> cur;
        std::vector<Placed> prev;
    };
    std::unordered_map<uint32_t, Kind> kinds;

    float distance2(const float* a, const float* b) {
        float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
        return dx * dx + dy * dy + dz * dz;
    }

    // Number this frame's instances of inst's kind (those built with it):
    // pairs of one now and one last frame are taken closest first, each
    // taking that one's number, and the rest get new numbers. Taking them one
    // at a time in draw order let an enemy that just appeared take the number
    // of the one beside it, which then paired with it on the in-between frames.
    void number_instances(Instance& inst) {
        Kind& kind = kinds[inst.key];
        if (kind.generation != generation) {
            kind.prev.swap(kind.cur);
            kind.cur.clear();
            kind.generation = generation;
        }
        std::vector<Instance*> batch;
        for (auto& it : instances) {
            Instance& other = it.second;
            if (other.key == inst.key && other.built == inst.built && other.number_generation != generation) {
                batch.push_back(&other);
            }
        }
        struct Pair {
            float d;
            uint32_t now;
            uint32_t before;
        };
        // Nothing moves this far in a frame (RT64 doesn't pair matrices this far apart either).
        const float max_distance = 256.0f;
        std::vector<Pair> pairs;
        for (uint32_t i = 0; i < batch.size(); i++) {
            for (uint32_t j = 0; j < kind.prev.size(); j++) {
                float d = distance2(batch[i]->pos, kind.prev[j].pos);
                if (d <= max_distance * max_distance) {
                    pairs.push_back({ d, i, j });
                }
            }
        }
        std::sort(pairs.begin(), pairs.end(), [](const Pair& x, const Pair& y) { return x.d < y.d; });
        std::vector<bool> before_used(kind.prev.size(), false);
        for (const Pair& pair : pairs) {
            Instance* now = batch[pair.now];
            if (now->number_generation == generation || before_used[pair.before]) {
                continue;
            }
            before_used[pair.before] = true;
            now->number = kind.prev[pair.before].number;
            now->number_generation = generation;
        }
        auto used = [&](uint32_t n) {
            for (const Placed& p : kind.prev) {
                if (p.number == n) return true;
            }
            for (const Placed& p : kind.cur) {
                if (p.number == n) return true;
            }
            for (const Instance* other : batch) {
                if (other->number_generation == generation && other->number == n) return true;
            }
            return false;
        };
        for (Instance* now : batch) {
            if (now->number_generation != generation) {
                uint32_t n = 0;
                while (used(n)) n++;
                now->number = n;
                now->number_generation = generation;
            }
        }
        for (const Instance* now : batch) {
            Placed placed;
            memcpy(placed.pos, now->pos, sizeof(placed.pos));
            placed.number = now->number;
            kind.cur.push_back(placed);
        }
    }

    uint32_t instance_number(Instance& inst) {
        if (inst.number_generation != generation) {
            number_instances(inst);
        }
        return inst.number;
    }
}

// The skeleton walkers' entry points, before a skeleton is drawn.
extern "C" void snp_interp_instance() {
    current_instance++;
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
        inst.key = key | 1;
        inst.built = generation;
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

    // The ID recorded for the matrix at addr, or 0xFFFFFFFF (automatic).
    uint32_t interp_matrix_id(uint32_t addr) {
        auto it = matrices.find(addr & 0x1FFFFFFF);
        if (it == matrices.end() || generation - it->second.generation > 2) {
            return 0xFFFFFFFF;
        }
        uint32_t id = it->second.id;
        auto inst = instances.find(it->second.instance);
        if (inst != instances.end() && inst->second.key != 0) {
            uint32_t n = instance_number(inst->second);
            id = (uint32_t)(((uint64_t)id << 32 | n) * 0x9E3779B97F4A7C15ull >> 33);
        }
        return id;
    }
}
