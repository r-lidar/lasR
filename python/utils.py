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


def _fetch_pypi_package_info(package_name="pylasr"):
    """
    Fetches package information from PyPI API.
    Returns the latest version information for the specified package.
    """
    import json
    
    pypi_url = f"https://pypi.org/pypi/{package_name}/json"
    req = urllib.request.Request(
        pypi_url,
        headers={"User-Agent": "pylasr-update-checker/1.0"}
    )
    with urllib.request.urlopen(req, timeout=5) as resp:
        data = json.loads(resp.read().decode('utf-8'))
    
    return {
        'version': data['info']['version'],
        'name': data['info']['name']
    }

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

            package_info = _fetch_pypi_package_info("pylasr")
            if not package_info or "version" not in package_info:
                return

            try:
                latest_v = Version(package_info["version"])
            except InvalidVersion:
                return

            if latest_v > curr_v:
                warnings.warn(
                    f"pylasr {latest_v} is now available; you have {curr_v}.",
                    stacklevel=2
                )
        except Exception:
            pass

    threading.Thread(target=_worker, daemon=True).start()