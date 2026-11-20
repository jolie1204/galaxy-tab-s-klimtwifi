// Build-time helper for the matching recovery bionic. Run in a disposable root.
#include <string.h>

extern int __system_property_area_init(void);
extern int __system_property_add(const char *, unsigned int, const char *, unsigned int);

int main(void)
{
    static const struct {
        const char *name;
        const char *value;
    } properties[] = {
        {"ro.adb.secure", "1"},
        {"ro.secure", "1"},
        {"ro.debuggable", "0"},
        {"ro.kernel.qemu", "0"},
        {"ro.product.name", "klimtwifi"},
        {"ro.product.model", "SM-T700"},
        {"ro.product.device", "klimtwifi"},
    };

    if (__system_property_area_init())
        return 1;
    for (unsigned int i = 0; i < sizeof(properties) / sizeof(properties[0]); ++i) {
        const char *name = properties[i].name;
        const char *value = properties[i].value;
        if (__system_property_add(name, strlen(name), value, strlen(value)))
            return 1;
    }
    return 0;
}
