"""Check a scratch-staged native CMake install without touching host services.

Run with SSO_TEST_PREFIX=/scratch/stage/usr. Optional SSO_TEST_OAUTH_PLUGIN
and SSO_TEST_WALLET_PLUGIN paths exercise independently built consumers.
"""

import os
from pathlib import Path
import select
import shlex
import struct
import subprocess
import tempfile
import time
import unittest


class CMakeInstall(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not os.environ.get("SSO_TEST_PREFIX"):
            raise unittest.SkipTest("Set SSO_TEST_PREFIX to a scratch-staged install")
        cls.prefix = Path(os.environ["SSO_TEST_PREFIX"]).resolve()
        matches = list(cls.prefix.glob("lib*/pkgconfig/signon-plugins.pc"))
        if len(matches) != 1:
            raise AssertionError("Expected exactly one installed plugin SDK")
        cls.libdir = matches[0].parent.parent
        cls.env = dict(os.environ, PKG_CONFIG_PATH=str(matches[0].parent))
        cls.env["LD_LIBRARY_PATH"] = str(cls.libdir)

    def pkgconfig(self, *args):
        return subprocess.check_output(
            ["pkg-config", "--define-prefix", *args], env=self.env, text=True
        ).strip()

    def test_sdk_metadata_and_headers(self):
        packages = ("libsignon-qt6", "signon-plugins", "signon-plugins-common", "SignOnExtension")
        self.pkgconfig("--validate", *packages)
        for header in (
            "signon-qt6/SignOn/AuthService",
            "signon-plugins/SignOn/AuthPluginInterface",
            "signon-plugins/SignOn/UiSessionData",
            "signon-plugins/SignOn/blobiohandler.h",
            "signon-extension/SignOn/ExtensionInterface",
            "signon-extension/SignOn/AbstractSecretsStorage",
        ):
            self.assertTrue((self.prefix / "include" / header).is_file(), header)
        plugin_dir = Path(self.pkgconfig("--variable=plugindir", "signon-plugins"))
        self.assertEqual(plugin_dir, self.libdir / "signond")
        self.assertEqual(
            Path(self.pkgconfig("--variable=plugindir", "SignOnExtension")),
            plugin_dir / "extensions",
        )
        self.assertTrue((plugin_dir / "libpasswordplugin.so").is_file())
        self.assertTrue((self.prefix / "libexec/signond").is_file())

    def compile_and_run(self, source, packages, *args):
        with tempfile.TemporaryDirectory() as tmp:
            binary = str(Path(tmp) / "consumer")
            subprocess.run(
                [os.environ.get("CXX", "c++"), "-std=c++17", "-fPIC", "-x", "c++", "-",
                 "-o", binary, *shlex.split(self.pkgconfig("--cflags", "--libs", *packages))],
                input=source, text=True, env=self.env, check=True,
            )
            subprocess.run([binary, *args], env=self.env, check=True, timeout=10)

    def test_installed_headers_compile_and_link(self):
        source = """
        #include <SignOn/AuthPluginInterface>
        #include <SignOn/UiSessionData>
        #include <SignOn/blobiohandler.h>
        #include <SignOn/ExtensionInterface>
        #include <SignOn/AbstractSecretsStorage>
        #include <SignOn/AuthService>
        int main() {
            SignOn::SessionData data;
            return data.propertyNames().size();
        }
        """
        packages = ("signon-plugins", "signon-plugins-common", "SignOnExtension")
        self.compile_and_run(source, packages)

    def check_helper(self, plugin, plugin_dir):
        if os.getuid() == 0:
            self.skipTest("The plugin helper intentionally refuses root")
        env = dict(self.env, SSO_PLUGINS_DIR=str(plugin_dir))
        process = subprocess.Popen(
            [str(self.prefix / "bin/signonpluginprocess"), plugin],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            env=env,
        )

        def read_exact(size):
            result = b""
            deadline = time.monotonic() + 5
            while len(result) < size:
                ready, _, _ = select.select([process.stdout], [], [], max(0, deadline - time.monotonic()))
                if not ready:
                    self.fail(f"{plugin}: plugin helper did not answer its IPC request")
                chunk = os.read(process.stdout.fileno(), size - len(result))
                if not chunk:
                    self.fail(f"{plugin}: helper exited before completing its IPC reply")
                result += chunk
            return result

        try:
            self.assertEqual(read_exact(15), b"process started")
            process.stdin.write(struct.pack(">I", 1))  # PLUGIN_OP_TYPE
            process.stdin.flush()
            size, = struct.unpack(">I", read_exact(4))
            self.assertLess(size, 1024)
            self.assertEqual(read_exact(size).decode("utf-16-be"), plugin)
            process.stdin.write(struct.pack(">I", 7))  # PLUGIN_OP_STOP
            process.stdin.flush()
            self.assertEqual(process.wait(timeout=5), 0)
        finally:
            if process.poll() is None:
                process.kill()
            process.communicate(timeout=5)

    def test_password_helper_ipc(self):
        self.check_helper("password", self.libdir / "signond")

    def test_oauth_helper_ipc(self):
        plugin = os.environ.get("SSO_TEST_OAUTH_PLUGIN")
        if not plugin:
            self.skipTest("Set SSO_TEST_OAUTH_PLUGIN to the built OAuth2 plugin")
        self.check_helper("oauth2", Path(plugin).resolve().parent)

    def test_wallet_extension_loads(self):
        plugin = os.environ.get("SSO_TEST_WALLET_PLUGIN")
        if not plugin:
            self.skipTest("Set SSO_TEST_WALLET_PLUGIN to the built wallet extension")
        # Instantiate the extension interface without opening a real wallet.
        self.compile_and_run(
            """
            #include <QCoreApplication>
            #include <QPluginLoader>
            #include <QDebug>
            #include <SignOn/ExtensionInterface>
            int main(int argc, char **argv) {
                QCoreApplication app(argc, argv);
                QPluginLoader loader(QString::fromLocal8Bit(argv[1]));
                loader.setLoadHints(QLibrary::ResolveAllSymbolsHint);
                auto extension = qobject_cast<SignOn::ExtensionInterface3 *>(loader.instance());
                if (!extension) {
                    qCritical() << loader.errorString();
                    return 1;
                }
                return 0;
            }
            """, ("SignOnExtension",), plugin,
        )


if __name__ == "__main__":
    unittest.main()
