// Interpolation IDs for character limbs (Refresh Rate above the game's 30
// fps). Characters are drawn limb by limb, each limb a matrix load, and the
// limbs are drawn from depth-sorted buckets, so their order changes from
// frame to frame. RT64 pairs matrices between frames by guessing, and with
// several soldiers sharing one model it paired a limb with another soldier's:
// pieces of models jumped about on the in-between frames. Each limb's matrix
// is recorded here as it's built, with the character's pose entry for the
// joint (unique per character and joint, the same every frame) and the mesh,
// and the display list pass (src/game/display_list.cpp) gives its load that
// ID. Hooks: snp.jp.toml.
#include <cstdint>
#include <unordered_map>

namespace {
    uint32_t current_joint = 0;
    uint32_t generation = 0;

    struct Entry {
        uint32_t id;
        uint32_t generation;
    };
    // Matrix address (from the frame's matrix pool) -> ID.
    std::unordered_map<uint32_t, Entry> matrices;
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

// func_80032AA0, as it builds a mesh's matrix: the matrix and the mesh.
extern "C" void snp_interp_matrix(uint32_t mtx, uint32_t mesh) {
    if (current_joint == 0) {
        return;
    }
    uint64_t h = ((uint64_t)current_joint << 32 | mesh) * 0x9E3779B97F4A7C15ull;
    uint32_t id = (uint32_t)(h >> 33); // never G_EX_ID_AUTO (0xFFFFFFFF)
    matrices[mtx & 0x1FFFFFFF] = { id, generation };
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
    }

    // The ID recorded for the matrix at addr, or 0xFFFFFFFF (automatic).
    uint32_t interp_matrix_id(uint32_t addr) {
        auto it = matrices.find(addr & 0x1FFFFFFF);
        if (it == matrices.end() || generation - it->second.generation > 2) {
            return 0xFFFFFFFF;
        }
        return it->second.id;
    }
}
