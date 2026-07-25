"""Copy the freshly built upstream mbedTLS archives into the Arduino libs package.

pioarduino's hybrid `custom_sdkconfig` build recompiles the IDF and then copies
`build/esp-idf/<component>/lib<component>.a` into
framework-arduinoespressif32-libs. That copy only walks one directory deep, so
the mbedTLS *submodule* targets — which live at
`build/esp-idf/mbedtls/mbedtls/library/lib{mbedtls,mbedcrypto,mbedx509}.a` plus
the 3rdparty everest/p256-m archives — are never picked up. The stale
prebuilt copies stay on the link line, so every `CONFIG_MBEDTLS_*` option that
changes upstream mbedTLS code (buffer lengths, ASYMMETRIC_CONTENT_LEN,
SSL_KEEP_PEER_CERTIFICATE, DYNAMIC_BUFFER) silently has no effect while
`libesp-tls.a` *is* rebuilt against the new config — which also breaks the link
outright once the two disagree about a symbol.

The platform deletes the IDF build tree immediately after its own copy, so this
runs as a post-action on `checkprogsize` registered from a `pre:` script: SCons
executes post-actions in registration order, and the platform registers its
copy+rmtree while processing the framework, i.e. after every `pre:` script.

Upstream names collide with the mbedTLS *component* archive (esp_crt_bundle),
which the platform already copies as `libmbedtls.a`; the Arduino package
disambiguates the submodule one as `libmbedtls_2.a`.
"""

import shutil
from pathlib import Path

Import("env")  # noqa: F821

# Path under build/esp-idf/mbedtls/ -> name it has inside the Arduino libs
# package. Spelled out rather than globbed: the mbedTLS *component* archive is
# also called libmbedtls.a and sits at the root of that tree.
ARCHIVES = {
    "mbedtls/library/libmbedtls.a": "libmbedtls_2.a",
    "mbedtls/library/libmbedcrypto.a": "libmbedcrypto.a",
    "mbedtls/library/libmbedx509.a": "libmbedx509.a",
    "mbedtls/3rdparty/everest/libeverest.a": "libeverest.a",
    "mbedtls/3rdparty/p256-m/libp256m.a": "libp256m.a",
}


def copy_upstream_mbedtls(*_args, **_kwargs):
    src_root = Path(env["PROJECT_BUILD_DIR"]) / env["PIOENV"] / "esp-idf" / "mbedtls"
    if not src_root.is_dir():
        # No hybrid IDF pass this run (plain Arduino build, or the second
        # compile the platform spawns after wiping the tree).
        return

    libs_pkg = env.PioPlatform().get_package_dir("framework-arduinoespressif32-libs")
    if not libs_pkg:
        return
    dst_dir = Path(libs_pkg) / env.BoardConfig().get("build.mcu", "esp32c3") / "lib"
    if not dst_dir.is_dir():
        return

    copied = []
    for src_rel, dst_name in ARCHIVES.items():
        src = src_root / src_rel
        if not src.is_file():
            continue
        shutil.copyfile(str(src), str(dst_dir / dst_name))
        copied.append(dst_name)

    missing = [n for n in ARCHIVES.values() if n not in copied]
    if missing:
        # Leaving stale archives behind links a mbedTLS built against a
        # different sdkconfig, so fail loudly instead of silently regressing.
        raise Exception("upstream mbedTLS archives not found in %s: %s" % (src_root, " ".join(missing)))
    print("*** Copied upstream mbedTLS archives: %s ***" % " ".join(copied))


env.AddPostAction("checkprogsize", copy_upstream_mbedtls)  # noqa: F821
