#include "ReplayIO.h"
#include <cstring>

namespace edge26 {

bool ReadReplay(const char* path, std::vector<FInputFrame>& outFrames) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) { std::fprintf(stderr, "ReplayIO: cannot open %s\n", path); return false; }
    FInputStreamHeader hdr;
    if (std::fread(&hdr, sizeof(hdr), 1, f) != 1) {
        std::fprintf(stderr, "ReplayIO: short header in %s\n", path); std::fclose(f); return false;
    }
    if (hdr.Magic != kInputStreamMagic) {
        std::fprintf(stderr, "ReplayIO: bad magic 0x%llx in %s\n",
                     (unsigned long long)hdr.Magic, path);
        std::fclose(f); return false;
    }
    if (hdr.Version != kInputStreamVersion) {
        std::fprintf(stderr, "ReplayIO: unsupported version %u\n", hdr.Version);
        std::fclose(f); return false;
    }
    outFrames.resize(hdr.TickCount);
    if (hdr.TickCount > 0 &&
        std::fread(outFrames.data(), sizeof(FInputFrame), hdr.TickCount, f) != hdr.TickCount) {
        std::fprintf(stderr, "ReplayIO: short body in %s\n", path);
        std::fclose(f); return false;
    }
    std::fclose(f);
    return true;
}

bool WriteReplay(const char* path, const std::vector<FInputFrame>& frames) {
    std::FILE* f = std::fopen(path, "wb");
    if (!f) { std::fprintf(stderr, "ReplayIO: cannot create %s\n", path); return false; }
    FInputStreamHeader hdr{kInputStreamMagic, kInputStreamVersion, (uint32_t)frames.size()};
    if (std::fwrite(&hdr, sizeof(hdr), 1, f) != 1) { std::fclose(f); return false; }
    if (!frames.empty() &&
        std::fwrite(frames.data(), sizeof(FInputFrame), frames.size(), f) != frames.size()) {
        std::fclose(f); return false;
    }
    std::fclose(f);
    return true;
}

}  // namespace edge26
