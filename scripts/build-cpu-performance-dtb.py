#!/usr/bin/env python3
"""Build and verify the tested CPU capacity DTB without changing linux/.

Requires git, tar, patch, gcc, dtc, fdtget and mkbootimg on the build host.
Outputs are local artifacts only; this command never connects to a device.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

PROJECT = Path(__file__).resolve().parents[1]
KERNEL_COMMIT = "2c092365fd5c72efcedc29a578601b0a41ce45c4"
PATCH = PROJECT / "patches/linux/sm8450-cpu-capacity.patch"
DT_SOURCE = "arch/arm64/boot/dts/qcom/sm8450-oneplus-negroni.dts"
EXPECTED = [(1024, 100)] * 4 + [(2253, 251)] * 3 + [(2386, 396)]


def run(*args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)


def output(*args):
    return subprocess.check_output(args, text=True).strip()


def properties(dtb, node="/"):
    """Compare raw property bytes, including phandles and empty properties."""
    values = {}
    for prop in output("fdtget", "-p", str(dtb), node).splitlines():
        key = node.rstrip("/") + "/" + prop
        values[key] = output("fdtget", "-t", "bx", str(dtb), node, prop)
    for child in output("fdtget", "-l", str(dtb), node).splitlines():
        values.update(properties(dtb, node.rstrip("/") + "/" + child))
    return values


def verify_dtb(original, updated):
    before, after = properties(original), properties(updated)
    if any(after.get(k) != v for k, v in before.items()):
        raise ValueError("An original device-tree property changed or disappeared")
    expected = {}
    for cpu, values in enumerate(EXPECTED):
        node = "/cpus/cpu@" + ("0" if cpu == 0 else f"{cpu}00")
        for name, value in zip(
            ("capacity-dmips-mhz", "dynamic-power-coefficient"), values
        ):
            expected[node + "/" + name] = " ".join(
                f"{byte:x}" for byte in struct.pack(">I", value)
            )
    added = {k: v for k, v in after.items() if k not in before}
    if added != expected:
        raise ValueError("Expected exactly the 16 tested CPU capacity/power properties")


def verify_image(image, dtb):
    data = image.read_bytes()
    if data[:8] != b"VNDRBOOT" or struct.unpack_from("<II", data, 8) != (4, 4096):
        raise ValueError("Unexpected vendor_boot format")
    if struct.unpack_from("<I", data, 24)[0] != 0:
        raise ValueError("Expected an empty vendor ramdisk")
    length = struct.unpack_from("<I", data, 2100)[0]
    if length != dtb.stat().st_size or data[4096:4096 + length] != dtb.read_bytes():
        raise ValueError("Embedded DTB does not match the compiled DTB")
    if any(data[4096 + length:]):
        raise ValueError("Unexpected nonzero data after the DTB")
    header = bytearray(data[:4096])
    header[2100:2104] = b"\0" * 4
    return header


def build_tree(work, destination, log):
    preprocessed = work / "device.pp.dts"
    run("gcc", "-E", "-nostdinc", "-I", str(work / "include"), "-undef",
        "-D__DTS__", "-x", "assembler-with-cpp", "-o", str(preprocessed),
        str(work / DT_SOURCE))
    run("dtc", "-@", "-b", "0", "-I", "dts", "-O", "dtb", "-o",
        str(destination), str(preprocessed), stderr=log)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=PROJECT / "logs/cpu-performance-build",
                        help="empty output directory (default: logs/cpu-performance-build)")
    args = parser.parse_args()
    for tool in ("git", "tar", "patch", "gcc", "dtc", "fdtget", "mkbootimg"):
        if not shutil.which(tool):
            parser.error(f"Missing host command: {tool}")
    kernel = PROJECT / "linux"
    run("git", "-C", str(kernel), "cat-file", "-e", KERNEL_COMMIT + "^{commit}")
    destination = args.output.resolve()
    if destination.exists() and any(destination.iterdir()):
        parser.error("Output directory must be empty; choose a new directory")
    destination.mkdir(parents=True, exist_ok=True)
    original = destination / "baseline.dtb"
    updated = destination / "sm8450-oneplus-negroni.dtb"
    with tempfile.TemporaryDirectory(prefix="source-", dir=destination) as temporary:
        work = Path(temporary)
        archive = work / "source.tar"
        with archive.open("wb") as stream:
            run("git", "-C", str(kernel), "archive", KERNEL_COMMIT,
                "arch/arm64/boot/dts/qcom", "include/dt-bindings",
                "include/uapi/linux/input-event-codes.h", stdout=stream)
        run("tar", "-xf", str(archive), "-C", str(work))
        with (destination / "dtc.log").open("w") as log:
            build_tree(work, original, log)
            run("patch", "--batch", "--forward", "-p1", "-d", str(work), "-i", str(PATCH))
            build_tree(work, updated, log)
    verify_dtb(original, updated)
    images = [destination / "vendor_boot-original.img",
              destination / "vendor_boot-cpu-capacity.img"]
    headers = []
    for image, dtb in zip(images, (original, updated)):
        run("mkbootimg", "--header_version", "4", "--vendor_boot", str(image),
            "--dtb", str(dtb), "--pagesize", "4096")
        headers.append(verify_image(image, dtb))
    if headers[0] != headers[1]:
        raise ValueError("vendor_boot headers differ beyond the DTB length")
    hashes = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
              for p in (original, updated, *images)}
    report = {
        "kernel_commit": KERNEL_COMMIT,
        "patch_sha256": hashlib.sha256(PATCH.read_bytes()).hexdigest(),
        "dtc": output("dtc", "--version"),
        "gcc": output("gcc", "-dumpfullversion"),
        "mkbootimg_sha256": hashlib.sha256(Path(shutil.which("mkbootimg")).read_bytes()).hexdigest(),
        "verification": "Original DT properties preserved; exactly 16 CPU properties added; vendor_boot verified",
        "sha256": hashes,
    }
    (destination / "build.json").write_text(json.dumps(report, indent=2) + "\n")
    (destination / "SHA256SUMS").write_text("".join(f"{value}  {key}\n" for key, value in hashes.items()))
    print(json.dumps(report, indent=2))
    print(f"Verified artifacts: {destination}")


if __name__ == "__main__":
    main()
