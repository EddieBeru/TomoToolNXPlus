"""Shared restrictions for the two Linux MTP transports."""
from pathlib import Path

BUILD_NAMES = {"TomoToolNXPlus.nro", "TomoToolNXPlusBorealis.nro"}
DIRECTORIES = {"switch", "switch/TomoToolNXPlus", "switch/TomoToolNXPlusBorealis"}


class DeployError(Exception):
    pass


def validate_source(source):
    source = Path(source)
    if source.is_symlink() or source.name not in BUILD_NAMES:
        raise DeployError("Only this project's two named NRO builds are allowed; symlinks are refused.")
    source = source.resolve(strict=True)
    if not source.is_file():
        raise DeployError("The build must be a regular .nro file.")
    return source


def remote_path(directory, name):
    if directory not in DIRECTORIES or name not in BUILD_NAMES:
        raise DeployError("Only the project NROs in the three permitted switch folders are allowed.")
    return f"/{directory}/{name}"
