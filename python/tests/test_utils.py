#!/usr/bin/env python3
"""
Shared utility functions for tests
"""

import os
import time


# Windows file lock retry delay in seconds
FILE_LOCK_RETRY_DELAY = 0.1


def safe_unlink(filepath, warn=False):
    """
    Safely delete a file with Windows compatibility.
    
    On Windows, files can be temporarily locked after operations. This function
    retries once after a brief delay (FILE_LOCK_RETRY_DELAY seconds) if the 
    initial deletion fails.
    
    Parameters:
    -----------
    filepath : str
        Path to the file to delete
    warn : bool
        If True, print a warning when file cannot be deleted.
        If False, fail silently (default).
    """
    if not os.path.exists(filepath):
        return
    
    try:
        os.unlink(filepath)
    except (PermissionError, OSError):
        # On Windows, sometimes files are locked briefly
        time.sleep(FILE_LOCK_RETRY_DELAY)
        try:
            os.unlink(filepath)
        except (PermissionError, OSError) as e:
            if warn:
                # If still can't delete, warn but don't fail
                print(f"Warning: Could not delete file {filepath}: {e}")
            # Otherwise, continue silently