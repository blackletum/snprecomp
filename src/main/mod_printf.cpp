// recomp_printf for mods: RECOMP_IMPORT("*", int recomp_printf(const char* fmt, ...)),
// the name other recomps' mods use. Prints to stdout, and on PC also to
// mod_log.txt in the working directory (with crash_log.txt), started fresh each
// run: Windows release builds have no console. On Android stdout already goes
// to log.txt (src/android/android_startup.cpp).
//
// The arguments are read as MIPS o32 passes a variadic call's: 4-byte slots,
// the first four in a0-a3 and the rest on the caller's stack, with doubles
// (floats are promoted) and 64-bit integers in an aligned pair of slots, high
// word first. Takes the C conversions d i u o x X c s p f F e E g G a A % with
// flags, width, precision (also *) and the h, hh, l, ll, z, j, t sizes.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

#include "librecomp/helpers.hpp"
#include "librecomp/overlays.hpp"

#include "snp_mod_api.h"

namespace {
    std::mutex print_mutex;
#if !defined(__ANDROID__)
    FILE* log_file = nullptr;
    bool log_tried = false;
#endif

    struct Args {
        uint8_t* rdram;
        recomp_context* ctx;
        int slot; // the next argument's

        uint32_t word(int i) {
            return i < 4 ? (uint32_t)(&ctx->r4)[i] : (uint32_t)MEM_W(i * 4, ctx->r29);
        }
        uint32_t next32() {
            return word(slot++);
        }
        uint64_t next64() {
            slot = (slot + 1) & ~1;
            uint64_t hi = word(slot), lo = word(slot + 1);
            slot += 2;
            return (hi << 32) | lo;
        }
    };

    std::string read_string(uint8_t* rdram, uint32_t addr, size_t max_len) {
        std::string s;
        if (addr == 0) {
            return "(null)";
        }
        for (size_t i = 0; i < max_len; i++) {
            char c = (char)MEM_B(i, (gpr)(int32_t)addr);
            if (c == '\0') {
                break;
            }
            s += c;
        }
        return s;
    }

    void write_out(const std::string& text) {
        std::lock_guard lock(print_mutex);
        fputs(text.c_str(), stdout);
        fflush(stdout);
#if !defined(__ANDROID__)
        if (!log_tried) {
            log_tried = true;
            log_file = fopen("mod_log.txt", "w");
        }
        if (log_file != nullptr) {
            fputs(text.c_str(), log_file);
            fflush(log_file);
        }
#endif
    }
}

extern "C" void recomp_printf(uint8_t* rdram, recomp_context* ctx) {
    constexpr size_t max_output = 4096;
    Args args{ rdram, ctx, 1 };
    const std::string fmt = read_string(rdram, args.word(0), max_output);
    std::string out;
    char buf[512];

    for (size_t i = 0; i < fmt.size() && out.size() < max_output; i++) {
        if (fmt[i] != '%') {
            out += fmt[i];
            continue;
        }
        // One conversion: %[flags][width][.precision][size]type
        std::string spec = "%";
        size_t j = i + 1;
        while (j < fmt.size() && std::strchr("-+ #0", fmt[j])) {
            spec += fmt[j++];
        }
        auto number = [&]() {
            if (j < fmt.size() && fmt[j] == '*') {
                spec += std::to_string((int32_t)args.next32());
                j++;
                return;
            }
            while (j < fmt.size() && fmt[j] >= '0' && fmt[j] <= '9') {
                spec += fmt[j++];
            }
        };
        number();
        if (j < fmt.size() && fmt[j] == '.') {
            spec += fmt[j++];
            number();
        }
        bool wide = false; // a 64-bit integer
        bool half = false, byte = false;
        if (j < fmt.size() && fmt[j] == 'h') {
            j++;
            if (j < fmt.size() && fmt[j] == 'h') {
                j++;
                byte = true;
            }
            else {
                half = true;
            }
        }
        else if (j < fmt.size() && fmt[j] == 'l') {
            j++;
            if (j < fmt.size() && fmt[j] == 'l') {
                j++;
                wide = true;
            }
        }
        else if (j < fmt.size() && (fmt[j] == 'z' || fmt[j] == 'j' || fmt[j] == 't' || fmt[j] == 'L')) {
            wide = fmt[j] == 'j';
            j++;
        }
        if (j >= fmt.size()) {
            out += spec;
            break;
        }
        const char type = fmt[j];
        i = j;
        buf[0] = '\0';
        switch (type) {
            case '%':
                out += '%';
                continue;
            case 'd': case 'i': {
                int64_t v = wide ? (int64_t)args.next64() : (int64_t)(int32_t)args.next32();
                if (half) v = (int16_t)v;
                if (byte) v = (int8_t)v;
                snprintf(buf, sizeof(buf), (spec + "lld").c_str(), (long long)v);
                break;
            }
            case 'u': case 'o': case 'x': case 'X': {
                uint64_t v = wide ? args.next64() : (uint64_t)args.next32();
                if (half) v = (uint16_t)v;
                if (byte) v = (uint8_t)v;
                snprintf(buf, sizeof(buf), (spec + "ll" + type).c_str(), (unsigned long long)v);
                break;
            }
            case 'c':
                snprintf(buf, sizeof(buf), (spec + "c").c_str(), (int)(char)args.next32());
                break;
            case 's':
                snprintf(buf, sizeof(buf), (spec + "s").c_str(), read_string(rdram, args.next32(), max_output).c_str());
                break;
            case 'p':
                snprintf(buf, sizeof(buf), "0x%08X", args.next32());
                break;
            case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A': {
                uint64_t bits = args.next64();
                double v;
                std::memcpy(&v, &bits, sizeof(v));
                snprintf(buf, sizeof(buf), (spec + type).c_str(), v);
                break;
            }
            default: // unknown: printed as written
                out += spec;
                out += type;
                continue;
        }
        out += buf;
    }
    if (out.size() > max_output) {
        out.resize(max_output);
    }
    write_out(out);
    _return(ctx, (int32_t)out.size());
}

void snp::register_printf_export() {
    recomp::overlays::register_base_export("recomp_printf", recomp_printf);
}
