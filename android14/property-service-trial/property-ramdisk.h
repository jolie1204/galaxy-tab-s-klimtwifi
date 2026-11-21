// SPDX-License-Identifier: Apache-2.0
// Included by the trial 21 mount-trial.h, before its namespace declaration.
#include <sys/mount.h>

namespace klimt_trial {
[[noreturn]] inline void Stop(const std::string& result);

inline void StageTrialFile(const char* source, const char* path) {
    std::string content;
    if (!android::base::ReadFileToString(source, &content) || content.empty() ||
        !android::base::WriteStringToFile(content, path, 0444, 0, 0)) {
        Stop(std::string("FAIL staging ") + source);
    }
}

inline void MountTrialCgroups() {
    const char* target = "/system/etc/cgroups.json";
    std::string installed, baseline;
    if (!android::base::ReadFileToString(target, &installed) ||
        !android::base::ReadFileToString("/dev/klimt-cgroups.original", &baseline) ||
        installed != baseline) {
        Stop("FAIL unexpected installed cgroups.json");
    }
    if (mount("/dev/klimt-cgroups.json", target, nullptr, MS_BIND, nullptr) ||
        mount(nullptr, target, nullptr, MS_BIND | MS_REMOUNT | MS_RDONLY, nullptr)) {
        Stop("FAIL read-only cgroups.json bind mount");
    }
}
}  // namespace klimt_trial
