# Android ashmem on ARMv7 Linux 6.12

The SM-T700 Android 14 system reports `ro.treble.enabled=false`. Its libcutils chooses the legacy `/dev/ashmem` device even when Linux provides memfd. A bring-up run without the device logged repeated Settings shared-memory allocation failures. The ARM32 diagnostic uses the C ashmem implementation from Android Common Kernel 6.12.69. [Android Common Kernel source](https://android.googlesource.com/kernel/common/+/b1ac27dd8a57803034f8361d820e3f2580714031/drivers/staging/android/)

From the root of the Linux 6.12.118 source tree, stage the three pinned files and apply [the ARM32 adaptation](ashmem-arm32.patch). The patch adds the built-in driver under `drivers/android` and adapts the Android-specific 16 KiB page helper to native `PAGE_ALIGN()` on this 4 KiB ARMv7 build.

```sh
set -e
rev=b1ac27dd8a57803034f8361d820e3f2580714031
base="https://android.googlesource.com/kernel/common/+/$rev/drivers/staging/android"
mkdir -p drivers/android/uapi
for file in ashmem.c ashmem.h uapi/ashmem.h; do
    curl -fsSL "$base/$file?format=TEXT" | base64 -d > "drivers/android/$file"
done
sha256sum -c <<'HASHES'
63bd518d3e739ae6a190b4fc8ebf9359c8745d5e579c6cdf562a8c8776f56089  drivers/android/ashmem.c
404a8a987ec795fa8667b8a9018d38abb24b445d9fdf1d702aaee31e4ff54b93  drivers/android/ashmem.h
32e4a2b6f0430e6607e36a1331c1d73661a1cd5e74262928bcbb4005ba571516  drivers/android/uapi/ashmem.h
HASHES
patch --fuzz=0 -p1 < /path/to/galaxy-tab-s-klimtwifi/kernel/klimt-ashmem/ashmem-arm32.patch
```

Enable `CONFIG_ASHMEM=y` and check that it survives `olddefconfig`. A built-in driver creates the character device before Android userspace starts.

The [ABI probe](ashmem-probe.c) checks the character device, region name and size ioctls, and data visibility across two shared mappings. Compile it with `armv7a-linux-androideabi34-clang -O2 -Wall -Wextra -Werror -I drivers/android /path/to/ashmem-probe.c -o klimt-ashmem-probe`. The isolated ARM init probe exited with status 0 using the existing diagnostic `su` domain. Android stayed SELinux-enforcing and the ordinary shell received its policy denial for opening the device. The comparison log with the driver contained none of the previous ashmem allocation errors. Permission-controller startup still exceeded the default time limit in that ARM emulation and kept Home unverified.
