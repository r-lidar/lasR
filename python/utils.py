import sys
import json
import urllib.request
import warnings

try:
    from importlib.metadata import version as get_version
except ImportError:
    from pkg_resources import get_distribution as get_version

def check_update():
    try:
        # Get current version
        try:
            curr_version = get_version("pylasr")
        except Exception:
            # If the package is not installed via pip, try to get the version from the binary module
            import pylasr
            curr_version = getattr(pylasr, "__version__", None)
            if curr_version is None:
                return

        # Get the latest version from PyPI
        url = "https://pypi.org/pypi/pylasr/json"
        with urllib.request.urlopen(url, timeout=2) as resp:
            data = json.load(resp)
            last_version = data["info"]["version"]

        if last_version > curr_version:
            msg = f"New version of pylasr {last_version} is available. You have {curr_version}.\nUpdate the package with the command: pip install -U pylasr"
            warnings.warn(msg)
    except Exception:
        pass