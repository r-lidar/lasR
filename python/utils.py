import sys
import urllib.request
import warnings
from warnings import warn
import threading
from packaging.version import Version, InvalidVersion

try:
    from importlib.metadata import version as get_version
except ImportError:
    from pkg_resources import get_distribution as get_version


def _format_warning(message, category, filename, lineno, line=None):
    return f"{category.__name__}: {message}\n"
warnings.formatwarning = _format_warning


# TODO: Change for pypi when available
def _fetch_r_universe_packages(repo_url="https://r-lidar.r-universe.dev/src/contrib/PACKAGES"):
    """
    Downloads and parses the PACKAGES file from R-Universe into a list of dictionaries,
    where each block represents a single package.
    """
    req = urllib.request.Request(
        repo_url,
        headers={"User-Agent": "pylasr-update-checker/1.0"}
    )
    with urllib.request.urlopen(req, timeout=5) as resp:
        text = resp.read().decode('utf-8', errors='ignore')
    pkgs = []
    entry = {}
    for line in text.splitlines():
        if not line.strip():
            if entry:
                pkgs.append(entry)
                entry = {}
        else:
            if ":" in line:
                key, val = line.split(":", 1)
                entry[key.strip()] = val.strip()
    if entry:
        pkgs.append(entry)
    return pkgs

def check_update():
    """Check for updates asynchronously in a background thread."""
    def _worker():
        try:
            try:
                curr = get_version("pylasr")
                curr_str = curr.version if hasattr(curr, "version") else curr
            except Exception:
                import pylasr
                curr_str = getattr(pylasr, "__version__", None)

            if not curr_str:
                return

            try:
                curr_v = Version(curr_str)
            except InvalidVersion:
                return

            pkgs = _fetch_r_universe_packages()
            # TODO: Change for pypi when available
            rec = next((p for p in pkgs if p.get("Package") == "lasR"), None)
            if not rec or "Version" not in rec:
                return

            try:
                latest_v = Version(rec["Version"])
            except InvalidVersion:
                return

            if latest_v > curr_v:
                warnings.warn(
                    f"lasR {latest_v} is now available; you have {curr_v}.",
                    stacklevel=2
                )
        except Exception:
            pass

    threading.Thread(target=_worker, daemon=True).start()