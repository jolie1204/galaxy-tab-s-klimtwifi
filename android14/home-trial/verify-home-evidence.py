from pathlib import Path
import re
import sys

launcher = b'com.android.launcher3/.uioverrides.QuickstepLauncher'
step = re.compile(rb'KLIMT-HOME [0-9a-f]{16} step=\d+,activity=\d,package=\d,window=\d,sf=\d,boot=(1|unset)$')
patterns = {
    b'RESUMED': re.compile(re.escape(launcher) + rb'$'),
    b'FOCUS': re.compile(re.escape(launcher) + rb'$'),
    b'HWC': re.compile(rb'Display [0-9]+ \(HWC display 0\)'),
    b'DISPLAY': re.compile(rb'\s*powerMode=On\s*$'),
    b'CAPTURE': re.compile(rb'capture=(640x400|400x640|2560x1600|1600x2560),format=[12],lit=[1-9][0-9]*$'),
}
observation = re.compile(rb'KLIMT-HOME (RESUMED|FOCUS|HWC|DISPLAY|CAPTURE) (.*)')

for name in sys.argv[1:]:
    boot = None
    cycle = {}
    for line in Path(name).read_bytes().splitlines():
        boot_sample = step.search(line)
        if boot_sample:
            cycle.clear()
            if boot_sample[1] == b'1':
                boot = boot or line
            continue
        sample = observation.search(line)
        if sample is None:
            continue
        kind, value = sample.groups()
        if patterns[kind].match(value):
            cycle[kind] = line
        else:
            cycle.pop(kind, None)
        if kind == b'CAPTURE' and boot and all(item in cycle for item in patterns):
            print(name)
            print(boot.decode())
            for key in patterns:
                print(cycle[key].decode())
            break
    else:
        raise SystemExit(f'{name}: complete same-cycle physical Home evidence has not been observed')
