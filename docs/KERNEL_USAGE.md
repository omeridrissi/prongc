# Using prongC with the Linux Kernel

## Make sure drivers are enabled

If you're analyzing any drivers (in-tree) that might be disabled by default make sure you enable them:

```bash
make menuconfig
```

Example: To enable a staging drivers, go to "Device Drivers" -> "Staging drivers" -> any staging driver

## Generate compile_commands.json

```bash
make M=path/to/driver compile_commands.json CC=clang -j$(nproc)
```

## Use with prongC

```bash
prongc ... --compdb-dir="[directory with compile_commands.json]"
```

