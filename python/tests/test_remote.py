import pytest
import subprocess
import os
import sys
import time
import socket

# Add parent directory to path for pylasr import
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))


def get_test_data_dir():
    """Find the inst/extdata directory with test COPC files."""
    base = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    return os.path.join(base, "inst", "extdata")


def find_free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


@pytest.fixture(scope="module")
def http_server():
    """Start a local HTTP server in a subprocess (avoids GIL blocking with GDAL)."""
    data_dir = get_test_data_dir()
    port = find_free_port()
    proc = subprocess.Popen(
        [sys.executable, "-m", "http.server", str(port), "--directory", data_dir],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    # Wait for server to be ready
    for _ in range(30):
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.5):
                break
        except OSError:
            time.sleep(0.1)
    else:
        proc.terminate()
        proc.wait(timeout=5)
        pytest.fail(f"HTTP server subprocess never became reachable on 127.0.0.1:{port}")
    yield f"http://127.0.0.1:{port}"
    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=5)


def _get_npoints(result):
    """Extract npoints from a pipeline execute() result dict."""
    assert result["success"], f"Pipeline failed: {result.get('message', '')}"
    all_stages = {}
    for item in result["data"]:
        all_stages.update(item)
    return all_stages["summary"]["npoints"]


def test_remote_copc_read(http_server):
    """Test that remote COPC reading matches local reading."""
    import pylasr

    local_file = os.path.join(get_test_data_dir(), "example.copc.laz")
    remote_url = f"{http_server}/example.copc.laz"

    # Read local
    pipeline_local = pylasr.reader_coverage() + pylasr.summarise()
    ans_local = pipeline_local.execute([local_file])

    # Read remote
    pipeline_remote = pylasr.reader_coverage() + pylasr.summarise()
    ans_remote = pipeline_remote.execute([remote_url])

    assert _get_npoints(ans_remote) == _get_npoints(ans_local)


def test_remote_copc_depth(http_server):
    """Test that depth works with remote files."""
    import pylasr

    local_file = os.path.join(get_test_data_dir(), "example.copc.laz")
    remote_url = f"{http_server}/example.copc.laz"

    pipeline_local = pylasr.reader_coverage(depth=0) + pylasr.summarise()
    ans_local = pipeline_local.execute([local_file])

    pipeline_remote = pylasr.reader_coverage(depth=0) + pylasr.summarise()
    ans_remote = pipeline_remote.execute([remote_url])

    assert _get_npoints(ans_remote) == _get_npoints(ans_local)


@pytest.mark.skipif(
    os.environ.get("CI") == "true" or os.environ.get("CRAN") == "true",
    reason="Skip public endpoint tests in CI/CRAN",
)
def test_public_copc_endpoint():
    """Test against a real public COPC endpoint."""
    import pylasr

    url = "https://s3.amazonaws.com/hobu-lidar/autzen-classified.copc.laz"
    pipeline = pylasr.reader_coverage(depth=0) + pylasr.summarise()
    ans = pipeline.execute([url])
    assert _get_npoints(ans) > 0
