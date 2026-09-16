"""Shared checks for Docker kernel configuration and installed module resolution."""
import hashlib
from pathlib import Path
import shlex
import subprocess

PROJECT = Path(__file__).resolve().parents[1]
BASE_OPTIONS = """
MODULES NAMESPACES UTS_NS IPC_NS PID_NS NET_NS USER_NS CGROUPS CGROUP_PIDS
MEMCG CPUSETS CGROUP_SCHED CGROUP_BPF BPF_SYSCALL SECCOMP SECCOMP_FILTER
POSIX_MQUEUE VETH BRIDGE BRIDGE_NETFILTER OVERLAY_FS NF_TABLES NF_CONNTRACK
NF_NAT NFT_CT NFT_MASQ NFT_NAT NETFILTER_XTABLES NETFILTER_XT_MATCH_ADDRTYPE
NETFILTER_XT_MATCH_CONNTRACK
""".split()
MODULES = {
    "NFT_COMPAT": ("net/netfilter/nft_compat.ko",),
    "NFT_FIB": ("net/netfilter/nft_fib.ko",),
    "NFT_FIB_IPV4": ("net/ipv4/netfilter/nft_fib_ipv4.ko",),
    "NFT_FIB_IPV6": ("net/ipv6/netfilter/nft_fib_ipv6.ko",),
    "NFT_FIB_INET": ("net/netfilter/nft_fib_inet.ko",),
    "NETFILTER_XT_TARGET_MASQUERADE": ("net/netfilter/xt_MASQUERADE.ko",),
    "NETFILTER_XT_NAT": ("net/netfilter/xt_nat.ko",),
    "VETH": ("drivers/net/veth.ko",),
    "BRIDGE": ("net/bridge/bridge.ko",),
    "BRIDGE_NETFILTER": ("net/bridge/br_netfilter.ko",),
    "OVERLAY_FS": ("fs/overlayfs/overlay.ko",),
    "NF_TABLES": ("net/netfilter/nf_tables.ko",),
    "NF_CONNTRACK": ("net/netfilter/nf_conntrack.ko",),
    "NF_NAT": ("net/netfilter/nf_nat.ko",),
    "NFT_CT": ("net/netfilter/nft_ct.ko",),
    "NFT_MASQ": ("net/netfilter/nft_masq.ko",),
    "NFT_NAT": ("net/netfilter/nft_nat.ko", "net/netfilter/nft_chain_nat.ko"),
    "NETFILTER_XTABLES": ("net/netfilter/x_tables.ko", "net/netfilter/xt_tcpudp.ko"),
    "NETFILTER_XT_MATCH_ADDRTYPE": ("net/netfilter/xt_addrtype.ko",),
    "NETFILTER_XT_MATCH_CONNTRACK": ("net/netfilter/xt_conntrack.ko",),
}
ALIASES = {
    "nfnetlink-subsys-11": "nft_compat",
    "nft-expr-2-fib": "nft_fib_ipv4",
    "nft-expr-10-fib": "nft_fib_ipv6",
    "nft-expr-1-fib": "nft_fib_inet",
    "ipt_MASQUERADE": "xt_MASQUERADE",
    "ip6t_MASQUERADE": "xt_MASQUERADE",
    "ipt_DNAT": "xt_nat",
    "ip6t_DNAT": "xt_nat",
}


def parse_config(text):
    return dict(line.split("=", 1) for line in text.splitlines()
                if line.startswith("CONFIG_") and "=" in line)


def verify_config(text):
    config = parse_config(text)
    required = parse_config((PROJECT / "config/kernel/docker.config").read_text())
    errors = [f"{key} must be {value}, got {config.get(key, 'n')}"
              for key, value in required.items() if config.get(key) != value]
    for name in BASE_OPTIONS + ["NFT_FIB"]:
        if config.get(f"CONFIG_{name}") not in ("m", "y"):
            errors.append(f"CONFIG_{name} is required")
    if errors:
        raise RuntimeError("Docker kernel requirements not met:\n" + "\n".join(errors))
    return config


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def required_modules(config):
    return {Path(path).stem: Path("kernel") / path
            for option, paths in MODULES.items()
            if config.get(f"CONFIG_{option}") == "m" for path in paths}


def verify_modules(root, release, config, reference=None):
    """Use modprobe's offline lookup to verify real indexes, aliases and dependencies.

    root contains lib/modules/RELEASE. No module is loaded into the host kernel.
    reference, when given, is the matching build's lib/modules/RELEASE directory.
    """
    module_root = root.resolve() / "lib/modules" / release
    required = required_modules(config)
    checked = {}

    def check_file(path):
        relative = path.relative_to(module_root)
        if str(relative) in checked:
            return
        if not path.is_file():
            raise RuntimeError(f"missing Docker module/dependency: {relative}")
        vermagic = subprocess.check_output(["modinfo", "-F", "vermagic", str(path)], text=True).strip()
        if not vermagic or vermagic.split()[0] != release:
            raise RuntimeError(f"wrong module vermagic for {relative}: {vermagic}")
        checksum = digest(path)
        if reference is not None and checksum != digest(reference / relative):
            raise RuntimeError(f"image/build module mismatch: {relative}")
        checked[str(relative)] = checksum

    for relative in required.values():
        check_file(module_root / relative)

    for query, name in [(name, name) for name in required] + list(ALIASES.items()):
        result = subprocess.run([
            "modprobe", "--show-depends", "--ignore-install", "--config", "/dev/null",
            "--dirname", str(root.resolve()), "--set-version", release, query,
        ], capture_output=True, text=True)
        if result.returncode or result.stderr:
            raise RuntimeError(f"module resolution failed for {query}: {result.stderr.strip()}")
        resolved = []
        for line in result.stdout.splitlines():
            fields = shlex.split(line)
            if fields and fields[0] == "insmod":
                path = Path(fields[1]).resolve()
                check_file(path)
                resolved.append(path)
        expected = module_root / required[name]
        if expected not in resolved:
            raise RuntimeError(f"module index/alias {query} does not resolve to {expected}")
    return checked
