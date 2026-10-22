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

    // A skeleton as drawn (or another object's matrix): what kind it is and
    // where its first matrix puts it.
    struct Instance {
        uint32_t key = 0;
        float pos[3] = {};
        float axes[3][3] = {}; // an object's normalized axes (zero for a skeleton)
        uint32_t number = 0;
        uint32_t built = 0; // generation
    };
    std::unordered_map<uint32_t, Instance> instances; // serial -> instance
    uint32_t current_instance = 0;
    uint32_t current_kind = 0;
    // Serials of other objects' matrices (snp_interp_object_matrix), apart
    // from skeletons'.
    constexpr uint32_t object_serials = 0x80000000;
    uint32_t object_serial = 0;
    // The matrix snp_interp_matrix just recorded (guMtxF2L builds it next).
    uint32_t recorded_matrix = 0;

    void set_pos(Instance& inst, uint32_t x, uint32_t y, uint32_t z) {
        memcpy(&inst.pos[0], &x, 4);
        memcpy(&inst.pos[1], &y, 4);
        memcpy(&inst.pos[2], &z, 4);
        for (float& f : inst.pos) {
            if (!std::isfinite(f)) f = 0.0f;
        }
    }

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
        float axes[3][3];
        float vel[3]; // since the time before, if it was numbered then too
        uint32_t number;
    };
    std::unordered_map<uint32_t, std::vector<Placed>> kinds;

    float distance2(const float* a, const float* b) {
        float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
        return dx * dx + dy * dy + dz * dz;
    }

    // How differently two objects are turned, 0 (alike) to 6, as RT64 measures
    // it; 0 if either has no axes (a skeleton).
    float orientation_difference(const float (&a)[3][3], const float (&b)[3][3]) {
        float d = 0.0f;
        for (int i = 0; i < 3; i++) {
            float dot = a[i][0] * b[i][0] + a[i][1] * b[i][1] + a[i][2] * b[i][2];
            float la = a[i][0] * a[i][0] + a[i][1] * a[i][1] + a[i][2] * a[i][2];
            float lb = b[i][0] * b[i][0] + b[i][1] * b[i][1] + b[i][2] * b[i][2];
            if (la == 0.0f || lb == 0.0f) {
                return 0.0f;
            }
            d += 1.0f - dot;
        }
        return d;
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
        // Each one last time is expected where its speed then takes it: the
        // boss's fire wave is a row of alike panels rushing at the camera,
        // each moving further in a frame than half the gap to the next, and by
        // where they were, a panel was paired with the one behind it and
        // flickered back on the in-between frames.
        std::vector<Pair> pairs;
        for (uint32_t i = 0; i < now.size(); i++) {
            for (uint32_t j = 0; j < placed.size(); j++) {
                if (distance2(now[i]->pos, placed[j].pos) > max_distance * max_distance) {
                    continue;
                }
                // And by how it's turned: a boss's energy fountains are alike
                // spray layers in one place, turned differently, and by place
                // alone a layer could be paired with another and swirl.
                const Placed& p = placed[j];
                const float expected[3] = { p.pos[0] + p.vel[0], p.pos[1] + p.vel[1], p.pos[2] + p.vel[2] };
                const float turn = 64.0f * orientation_difference(now[i]->axes, p.axes);
                pairs.push_back({ distance2(now[i]->pos, expected) + turn * turn, i, j });
            }
        }
        std::sort(pairs.begin(), pairs.end(), [](const Pair& x, const Pair& y) { return x.d < y.d; });
        std::vector<bool> numbered(now.size(), false);
        std::vector<bool> before_used(placed.size(), false);
        std::vector<Placed> next(now.size());
        for (uint32_t i = 0; i < now.size(); i++) {
            memcpy(next[i].pos, now[i]->pos, sizeof(next[i].pos));
            memcpy(next[i].axes, now[i]->axes, sizeof(next[i].axes));
            memset(next[i].vel, 0, sizeof(next[i].vel));
        }
        for (const Pair& pair : pairs) {
            if (numbered[pair.now] || before_used[pair.before]) {
                continue;
            }
            numbered[pair.now] = true;
            before_used[pair.before] = true;
            now[pair.now]->number = placed[pair.before].number;
            for (int k = 0; k < 3; k++) {
                next[pair.now].vel[k] = now[pair.now]->pos[k] - placed[pair.before].pos[k];
            }
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
            next[i].number = now[i]->number;
        }
        placed.swap(next);
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
        inst.built = generation;
        set_pos(inst, x, y, z);
    }
    matrices[mtx & 0x1FFFFFFF] = { key, generation, current_instance };
    recorded_matrix = mtx & 0x1FFFFFFF;
}

// guMtxF2L, as it builds any matrix: the float matrix, the matrix and its
// caller (where it was called from: the kind of object). One not recorded as
// a skeleton's above is an object of its own (the cones in the city stage,
// all alike, were paired with one another by RT64's guesses and flickered),
// numbered like skeletons with the others built at the same place, placed by
// the float matrix's translation and turned by its axes.
extern "C" void snp_interp_object_matrix(uint8_t* rdram, uint32_t fmtx, uint32_t mtx, uint32_t ra) {
    mtx &= 0x1FFFFFFF;
    if (mtx == recorded_matrix) {
        recorded_matrix = 0;
        return;
    }
    auto word = [&](uint32_t offset) { return *(uint32_t*)(rdram + ((fmtx + offset) & 0x1FFFFFFF)); };
    uint32_t serial = object_serials | (++object_serial & 0x7FFFFFFF);
    uint32_t kind = (uint32_t)((ra * 0x9E3779B97F4A7C15ull) >> 32) | 0x80000001u;
    Instance& inst = instances[serial];
    inst = Instance();
    inst.key = kind;
    inst.built = generation;
    set_pos(inst, word(0x30), word(0x34), word(0x38));
    for (int i = 0; i < 3; i++) {
        float len2 = 0.0f;
        for (int j = 0; j < 3; j++) {
            uint32_t w = word(i * 0x10 + j * 4);
            memcpy(&inst.axes[i][j], &w, 4);
            if (!std::isfinite(inst.axes[i][j])) inst.axes[i][j] = 0.0f;
            len2 += inst.axes[i][j] * inst.axes[i][j];
        }
        float scale = len2 > 0.0f ? 1.0f / std::sqrt(len2) : 0.0f;
        for (int j = 0; j < 3; j++) {
            inst.axes[i][j] *= scale;
        }
    }
    matrices[mtx] = { kind >> 1, generation, serial };
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
        if (instances.size() > 4096) {
            for (auto it = instances.begin(); it != instances.end();) {
                it = (generation - it->second.built > 4) ? instances.erase(it) : std::next(it);
            }
        }
    }

    // The ID recorded for the matrix at addr, or 0xFFFFFFFF (automatic), its
    // instance (0 if none) and whether that's an object's rather than a
    // skeleton's.
    uint32_t interp_matrix_id(uint32_t addr, uint32_t& instance, bool& object) {
        instance = 0;
        object = false;
        auto it = matrices.find(addr & 0x1FFFFFFF);
        if (it == matrices.end() || generation - it->second.generation > 2) {
            return 0xFFFFFFFF;
        }
        // SNP_INTERP_INSTANCES=0 leaves instances out (for comparing).
        static const bool instances_off = [] { const char* v = std::getenv("SNP_INTERP_INSTANCES"); return v && v[0] == '0'; }();
        auto inst = instances.find(it->second.instance);
        object = (it->second.instance & object_serials) != 0;
        if (!instances_off && inst != instances.end() && inst->second.key != 0) {
            instance = it->second.instance;
        }
        else if (object) {
            return 0xFFFFFFFF; // an object's ID is only of use with its number
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
