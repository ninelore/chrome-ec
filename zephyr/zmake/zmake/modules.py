# Copyright 2020 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Registry of known Zephyr modules."""

from zmake import build_config
from zmake import util


known_modules = {
    # TODO(b/384581513): boringssl is not officially recognized by Zephyr,
    # since it doesn't have a zephyr/module.yaml. That doesn't prevent us from
    # using it with zmake.
    "boringssl": {"path": "src/third_party/borringssl", "type": "public"},
    "hal_stm32": {"path": "src/third_party/zephyr/hal_stm32", "type": "public"},
    "cmsis": {"path": "src/third_party/zephyr/cmsis", "type": "public"},
    "ec": {"path": "src/platform/ec", "type": "public"},
    "fpc": {"path": "src/platform/fingerprint/fpc", "type": "public"},
    "nanopb": {"path": "src/third_party/zephyr/nanopb", "type": "public"},
    "pigweed": {"path": "src/third_party/pigweed", "type": "public"},
    "hal_intel_public": {
        "path": "src/third_party/zephyr/hal_intel_public",
        "type": "public",
    },
    "picolibc": {"path": "src/third_party/zephyr/picolibc", "type": "public"},
    "intel_module_private": {
        "path": "src/third_party/zephyr/intel_module_private",
        "type": "private",
    },
}


def is_private(module_name):
    """Indicate if module is private.

    Public modules are required and zmake will raise an exception if a public
    module cannot be found.  Private modules are not available in the public
    manifest, so projects that require a private module are skipped instead
    of throwing an error.

    Args:
        module_name: Name of the module.

    Returns:
        True: the specified module_name is private.
        False: the specified module_name is public.


    Raises:
        A KeyError, if the module_name is not known.
    """
    try:
        module_info = known_modules[module_name]
    except KeyError as e:
        raise KeyError(f"The {module_name} module is not a known module") from e

    if module_info["type"] == "private":
        return True

    return False


def locate_from_checkout(checkout_dir):
    """Find modules from a Chrome OS checkout.

    Important: this function should only conditionally be called if a
    checkout exists.  Zmake *can* be used without a Chrome OS source
    tree.  You should call locate_from_directory if outside of a
    Chrome OS source tree.

    Args:
        checkout_dir: The path to the chromiumos source.

    Returns:
        A dictionary mapping module names to paths.
    """
    result = {}
    for name, module_info in known_modules.items():
        path = checkout_dir / module_info["path"]
        if path.exists():
            result[name] = path
    return result


def locate_from_directory(directory):
    """Create a modules dictionary from a directory.

    This takes a directory, and searches for the known module names
    located in it.

    Args:
        directory: the directory to search in.

    Returns:
        A dictionary mapping module names to paths.
    """
    result = {}

    for name in known_modules:
        modpath = (directory / name).resolve()
        if (modpath / "zephyr" / "module.yml").is_file():
            result[name] = modpath

    return result


def setup_module_symlinks(output_dir, modules):
    """Setup a directory with symlinks to modules.

    Args:
        output_dir: The directory to place the symlinks in.
        modules: A dictionary of module names mapping to paths.

    Returns:
        The resultant BuildConfig that should be applied to use each
        of these modules.
    """
    if not output_dir.exists():
        output_dir.mkdir(parents=True)

    module_links = []

    for name, path in modules.items():
        link_path = output_dir.resolve() / name
        util.update_symlink(path, link_path)
        module_links.append(link_path)

    if module_links:
        return build_config.BuildConfig(
            cmake_defs={"ZEPHYR_MODULES": ";".join(map(str, module_links))}
        )
    return build_config.BuildConfig()


def default_projects_dirs(modules):
    """Get the default search path for projects.

    Args:
        modules: A dictionary of module names mapping to paths.

    Returns:
        A list of paths that should be searched for projects, if they exist.
    """
    ret = []
    if "ec" in modules:
        ret.append(modules["ec"] / "zephyr" / "program")
    return ret
