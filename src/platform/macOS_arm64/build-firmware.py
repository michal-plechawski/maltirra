#!/usr/bin/env python3
"""Build the built-in ROM resources using the production MADS/ATCompiler recipes."""
import argparse
import hashlib
import json
import pathlib
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--mads", default="mads")
parser.add_argument("--compiler", required=True)
parser.add_argument("--cache-root", required=True)
parser.add_argument("--output-root", required=True)
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parents[3]
mads = shutil.which(args.mads)
if not mads:
    parser.error("MADS assembler not found; set MADS to its executable path")
compiler = pathlib.Path(args.compiler).resolve()
cache = pathlib.Path(args.cache_root).resolve()
output = pathlib.Path(args.output_root).resolve()
cache.mkdir(parents=True, exist_ok=True)
output.mkdir(parents=True, exist_ok=True)
kernel = root / "src/Kernel"
basic = root / "src/ATBasic"
names = ["kernel.rom", "kernelxl.rom", "kernel816.rom", "nokernel.rom", "atbasic.bin",
         "superkernel.rom", "nocartridge.rom", "nohdbios.rom", "nogame.rom",
         "ultimate.lzrom", "nomio.lzrom", "noblackbox.lzrom", "850relocator.bin",
         "850handler.bin", "1030firmware.bin", "rapidflash.lzrom", "rapidpbi16.bin"]

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

inputs = [("build-firmware.py", pathlib.Path(__file__).resolve()), ("mads", pathlib.Path(mads)), ("ATCompiler", compiler)]
for directory in [kernel / "source", kernel / "autobuild", kernel / "autobuild_default", basic / "source"]:
    if directory.exists():
        inputs.extend((p.relative_to(root).as_posix(), p) for p in sorted(directory.rglob("*")) if p.is_file())
signature = hashlib.sha256(json.dumps([(label, sha(p)) for label, p in inputs]).encode()).hexdigest()
manifest_path = cache / "manifest.json"
try:
    manifest = json.loads(manifest_path.read_text())
    hit = manifest["signature"] == signature and all(
        (cache / n).exists() and sha(cache / n) == manifest["outputs"][n] for n in names)
except (OSError, ValueError, KeyError):
    hit = False

def assemble(source, destination, *flags, directory=kernel):
    subprocess.run([mads, source, "-s", "-p", f"-i:{kernel / 'source/Shared'}",
                    f"-i:{cache}", "-i:autobuild", "-i:autobuild_default",
                    f"-o:{cache / destination}", f"-l:{cache / (destination + '.lst')}", *flags], cwd=directory, check=True)

def pack(source, destination):
    subprocess.run([str(compiler), "lzpack", str(cache / source), str(cache / destination)], check=True)
    restored = cache / (destination + ".check")
    subprocess.run([str(compiler), "lzunpack", str(cache / destination), str(restored)], check=True)
    if restored.read_bytes() != (cache / source).read_bytes():
        raise RuntimeError(f"Firmware compression round trip failed: {source}")
    restored.unlink()

if not hit:
    assemble("source/main.xasm", "kernel.rom", "-d:_KERNEL_XLXE=0", "-b:$d800")
    assemble("source/main.xasm", "kernelxl.rom", "-d:_KERNEL_XLXE=1", "-b:$c000")
    assemble("source/main.xasm", "kernel816.rom", "-d:_KERNEL_XLXE=1", "-d:_KERNEL_816=1", "-b:$c000")
    assemble("source/nokernel/main.xasm", "nokernel.rom", "-b:$c000")
    assemble("source/atbasic.s", "atbasic.bin", "-c", "-d:CART=1", "-b:$a000", directory=basic)
    assemble("source/superkernel/main.xasm", "superkernel.rom", "-b:$f800")
    assemble("source/nocartridge/main.s", "nocartridge.rom", "-b:$b000")
    assemble("source/NoHDBIOS/main.s", "nohdbios.rom", "-b:$d800")
    assemble("source/NoGame/main.s", "nogame.rom", "-b:$a000")
    assemble("source/Ultimate/main.s", "ultimate.rom")
    pack("ultimate.rom", "ultimate.lzrom")
    for stem, copies in [("nomio", 4), ("noblackbox", 8)]:
        assemble(f"source/Placeholders/{stem}.s", f"{stem}.rom")
        (cache / f"{stem}-repeated.rom").write_bytes((cache / f"{stem}.rom").read_bytes() * copies)
        pack(f"{stem}-repeated.rom", f"{stem}.lzrom")
    for suffix, address, high in [("2800", 10240, 0), ("2801", 10241, 0), ("a800", 43008, 0), ("2800hi", 10240, 1)]:
        assemble("source/850/handler.s", f"850handler-{suffix}.obx", f"-d:BASEADDR={address}", f"-d:HIBUILD={high}")
    subprocess.run([str(compiler), "makereloc", *[str(cache / f"850handler-{s}.obx") for s in ["2800", "2801", "a800", "2800hi"]],
                    str(cache / "850handler.bin"), str(cache / "850handler-relocs.inc")], check=True)
    assemble("source/850/relocator.s", "850relocator.bin")
    assemble("source/1030/handler.s", "1030handler.bin", "-d:BASEADDR=7424")
    assemble("source/1030/firmware.s", "1030firmware.bin")
    for stem in ["rapidpbi8", "rapidfirmware", "rapidos", "rapidpbi16", "rapidflash"]:
        assemble(f"source/Rapidus/{stem}.s", f"{stem}.bin")
    pack("rapidflash.bin", "rapidflash.lzrom")
    for name, size in [("kernel.rom", 10240), ("kernelxl.rom", 16384), ("kernel816.rom", 16384), ("atbasic.bin", 8192)]:
        if (cache / name).stat().st_size != size:
            raise RuntimeError(f"Unexpected ROM size: {name}")
    manifest_path.write_text(json.dumps({"signature": signature, "outputs": {n: sha(cache / n) for n in names}}))

for name in names:
    shutil.copy2(cache / name, output / name)
print(f"{'CACHED' if hit else 'BUILT'}   {len(names)} built-in firmware resources")
