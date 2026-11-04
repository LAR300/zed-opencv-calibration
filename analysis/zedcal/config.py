"""Analysis configuration files (spec nucleo-offline R12). Every run copies its configuration
into its output folder, and its report records the commit, the library versions and the seed.
"""
import os
import shutil
from typing import Any, Dict

import yaml


class ConfigError(ValueError):
    pass


def load(path: str) -> Dict[str, Any]:
    with open(path) as f:
        data = yaml.safe_load(f) or {}
    if not isinstance(data, dict):
        raise ConfigError("%s must be a mapping" % path)
    return data


def require(cfg: Dict[str, Any], key: str, path: str = "config"):
    if key not in cfg:
        raise ConfigError("%s: missing '%s'" % (path, key))
    return cfg[key]


def prepare_output(output_dir: str, config_path: str) -> str:
    """Create the output folder (never over existing content) and copy the config into it."""
    if os.path.exists(output_dir) and os.listdir(output_dir):
        raise FileExistsError("%s is not empty and is never overwritten" % output_dir)
    os.makedirs(output_dir, exist_ok=True)
    copy = os.path.join(output_dir, "config.yaml")
    shutil.copyfile(config_path, copy)
    return copy
