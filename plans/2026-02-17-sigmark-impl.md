# sigmark Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a standalone CLI tool that GPG-signs the body of YAML-frontmatter markdown files, storing signatures in front matter.

**Architecture:** Click CLI wrapping GPG subprocess calls. A scanner walks directories for `.md` files, a frontmatter module handles YAML round-tripping, and a signer module handles GPG operations and body normalization. No database — all state lives in the front matter of each file.

**Tech Stack:** Python 3.10+, Click, PyYAML, Rich, GPG binary via subprocess.

---

### Task 1: Project Scaffolding

**Files:**
- Create: `~/github/repos/sigmark/pyproject.toml`
- Create: `~/github/repos/sigmark/src/sigmark/__init__.py`
- Create: `~/github/repos/sigmark/src/sigmark/cli.py`
- Create: `~/github/repos/sigmark/tests/__init__.py`
- Create: `~/github/repos/sigmark/tests/conftest.py`

**Step 1: Create directory structure**

```bash
mkdir -p ~/github/repos/sigmark/src/sigmark
mkdir -p ~/github/repos/sigmark/tests
mkdir -p ~/github/repos/sigmark/hugo/layouts/partials
```

**Step 2: Write pyproject.toml**

```toml
[build-system]
requires = ["hatchling"]
build-backend = "hatchling.build"

[project]
name = "sigmark"
version = "0.1.0"
description = "GPG signing for static site markdown content"
readme = "README.md"
license = "MIT"
requires-python = ">=3.10"
authors = [
    { name = "Alex Towell", email = "lex@metafunctor.com" }
]
keywords = ["gpg", "markdown", "static-site", "hugo", "signing", "pgp"]
classifiers = [
    "Development Status :: 3 - Alpha",
    "Environment :: Console",
    "Intended Audience :: Developers",
    "License :: OSI Approved :: MIT License",
    "Programming Language :: Python :: 3",
    "Programming Language :: Python :: 3.10",
    "Programming Language :: Python :: 3.11",
    "Programming Language :: Python :: 3.12",
    "Topic :: Security :: Cryptography",
]

dependencies = [
    "click>=8.0",
    "pyyaml>=6.0",
    "rich>=13.0",
]

[project.optional-dependencies]
dev = [
    "pytest>=7.0",
    "pytest-cov>=4.0",
    "pytest-mock>=3.0",
    "ruff>=0.1.0",
    "mypy>=1.0",
]

[project.scripts]
sigmark = "sigmark.cli:main"

[project.urls]
Repository = "https://github.com/queelius/sigmark"

[tool.hatch.build.targets.wheel]
packages = ["src/sigmark"]

[tool.hatch.build.targets.wheel.sources]
"src" = ""

[tool.hatch.build]
include = ["src/sigmark/**/*.py"]

[tool.pytest.ini_options]
testpaths = ["tests"]
pythonpath = ["src"]
addopts = "-v --tb=short"

[tool.coverage.run]
source = ["src/sigmark"]
branch = true

[tool.coverage.report]
exclude_lines = [
    "pragma: no cover",
    "if TYPE_CHECKING:",
    "raise NotImplementedError",
]

[tool.ruff]
target-version = "py310"
line-length = 100

[tool.ruff.lint]
select = ["E", "F", "I", "N", "W", "UP", "B", "C4", "SIM"]
ignore = ["E501"]

[tool.mypy]
python_version = "3.10"
warn_return_any = true
warn_unused_configs = true
ignore_missing_imports = true
```

**Step 3: Write __init__.py**

```python
# src/sigmark/__init__.py
"""sigmark — GPG signing for static site markdown content."""

__version__ = "0.1.0"
__author__ = "Alex Towell"
__email__ = "lex@metafunctor.com"
```

**Step 4: Write minimal cli.py placeholder**

```python
# src/sigmark/cli.py
"""sigmark CLI entry point."""

from __future__ import annotations

import click

from sigmark import __version__


@click.group()
@click.version_option(version=__version__, prog_name="sigmark")
@click.option("-v", "--verbose", is_flag=True, help="Enable verbose output")
@click.option("-n", "--dry-run", is_flag=True, help="Preview without making changes")
@click.pass_context
def main(ctx: click.Context, verbose: bool, dry_run: bool) -> None:
    """GPG signing for static site markdown content."""
    ctx.ensure_object(dict)
    ctx.obj["verbose"] = verbose
    ctx.obj["dry_run"] = dry_run
```

**Step 5: Write conftest.py with GPG test fixtures**

```python
# tests/conftest.py
"""Shared test fixtures for sigmark."""

from __future__ import annotations

import os
import subprocess
import tempfile
from pathlib import Path

import pytest


@pytest.fixture
def tmp_content(tmp_path: Path) -> Path:
    """Create a temporary content directory with sample .md files."""
    post = tmp_path / "post" / "hello-world"
    post.mkdir(parents=True)
    (post / "index.md").write_text(
        "---\n"
        "title: Hello World\n"
        "date: 2026-01-01\n"
        "tags:\n"
        "  - test\n"
        "---\n"
        "This is the body of the post.\n"
        "\n"
        "It has multiple paragraphs.\n"
    )

    post2 = tmp_path / "post" / "second-post"
    post2.mkdir(parents=True)
    (post2 / "index.md").write_text(
        "---\n"
        "title: Second Post\n"
        "date: 2026-01-02\n"
        "---\n"
        "Another post body.\n"
    )

    # A file without front matter (should be skipped)
    (tmp_path / "README.md").write_text("# Just a readme\n\nNo front matter here.\n")

    return tmp_path


@pytest.fixture
def gpg_home(tmp_path: Path) -> Path:
    """Create a temporary GPG home with a test key.

    This isolates tests from the user's real keyring.
    """
    gnupg_dir = tmp_path / ".gnupg"
    gnupg_dir.mkdir(mode=0o700)

    # Generate a test key (no passphrase, RSA 2048 for speed)
    key_params = gnupg_dir / "key_params"
    key_params.write_text(
        "%no-protection\n"
        "Key-Type: RSA\n"
        "Key-Length: 2048\n"
        "Name-Real: Test Signer\n"
        "Name-Email: test@example.com\n"
        "Expire-Date: 0\n"
        "%commit\n"
    )

    env = {**os.environ, "GNUPGHOME": str(gnupg_dir)}
    subprocess.run(
        ["gpg", "--batch", "--gen-key", str(key_params)],
        env=env,
        capture_output=True,
        check=True,
    )

    return gnupg_dir
```

**Step 6: Install in editable mode and verify**

```bash
cd ~/github/repos/sigmark && pip install -e ".[dev]"
sigmark --version
```

Expected: `sigmark, version 0.1.0`

**Step 7: Initialize git repo and commit**

```bash
cd ~/github/repos/sigmark
git init
git add pyproject.toml src/ tests/
git commit -m "feat: initial project scaffolding"
```

---

### Task 2: Front Matter Module

**Files:**
- Create: `~/github/repos/sigmark/src/sigmark/frontmatter.py`
- Create: `~/github/repos/sigmark/tests/test_frontmatter.py`

**Step 1: Write the failing tests**

```python
# tests/test_frontmatter.py
"""Tests for front matter parsing and writing."""

from __future__ import annotations

from pathlib import Path

from sigmark.frontmatter import FrontMatter


class TestFrontMatterParsing:
    """Test YAML front matter extraction."""

    def test_parse_standard_front_matter(self, tmp_path: Path) -> None:
        md = tmp_path / "test.md"
        md.write_text(
            "---\ntitle: Hello\ndate: 2026-01-01\n---\nBody text here.\n"
        )
        fm = FrontMatter(md)
        fm.load()
        assert fm.metadata["title"] == "Hello"
        assert fm.body == "Body text here.\n"

    def test_parse_no_front_matter(self, tmp_path: Path) -> None:
        md = tmp_path / "test.md"
        md.write_text("Just plain markdown.\n")
        fm = FrontMatter(md)
        assert fm.load() is False

    def test_parse_empty_body(self, tmp_path: Path) -> None:
        md = tmp_path / "test.md"
        md.write_text("---\ntitle: Empty\n---\n")
        fm = FrontMatter(md)
        fm.load()
        assert fm.metadata["title"] == "Empty"
        assert fm.body == ""

    def test_preserves_body_exactly(self, tmp_path: Path) -> None:
        body = "Line 1\n\n  Indented line\n\n```python\ncode()\n```\n"
        md = tmp_path / "test.md"
        md.write_text(f"---\ntitle: Test\n---\n{body}")
        fm = FrontMatter(md)
        fm.load()
        assert fm.body == body


class TestFrontMatterWriting:
    """Test YAML front matter modification and saving."""

    def test_set_and_save(self, tmp_path: Path) -> None:
        md = tmp_path / "test.md"
        md.write_text("---\ntitle: Hello\n---\nBody.\n")
        fm = FrontMatter(md)
        fm.load()
        fm.set("gpg_sig", "test-signature")
        fm.save()

        # Re-read and verify
        fm2 = FrontMatter(md)
        fm2.load()
        assert fm2.metadata["gpg_sig"] == "test-signature"
        assert fm2.body == "Body.\n"

    def test_save_preserves_body(self, tmp_path: Path) -> None:
        body = "Body with\nmultiple lines\nand stuff.\n"
        md = tmp_path / "test.md"
        md.write_text(f"---\ntitle: Hello\n---\n{body}")
        fm = FrontMatter(md)
        fm.load()
        fm.set("new_field", "value")
        fm.save()

        fm2 = FrontMatter(md)
        fm2.load()
        assert fm2.body == body

    def test_remove_field(self, tmp_path: Path) -> None:
        md = tmp_path / "test.md"
        md.write_text("---\ntitle: Hello\ngpg_sig: old\n---\nBody.\n")
        fm = FrontMatter(md)
        fm.load()
        fm.remove("gpg_sig")
        fm.save()

        fm2 = FrontMatter(md)
        fm2.load()
        assert "gpg_sig" not in fm2.metadata

    def test_dry_run_does_not_write(self, tmp_path: Path) -> None:
        md = tmp_path / "test.md"
        original = "---\ntitle: Hello\n---\nBody.\n"
        md.write_text(original)
        fm = FrontMatter(md)
        fm.load()
        fm.set("gpg_sig", "test")
        fm.save(dry_run=True)
        assert md.read_text() == original


class TestFrontMatterMultilineSig:
    """Test that multiline YAML values (like PGP signatures) round-trip."""

    def test_multiline_value_roundtrip(self, tmp_path: Path) -> None:
        sig = (
            "-----BEGIN PGP SIGNATURE-----\n"
            "\n"
            "iQIzBAABCAAdFiEEtest\n"
            "=abcd\n"
            "-----END PGP SIGNATURE-----\n"
        )
        md = tmp_path / "test.md"
        md.write_text("---\ntitle: Hello\n---\nBody.\n")
        fm = FrontMatter(md)
        fm.load()
        fm.set("gpg_sig", sig)
        fm.save()

        fm2 = FrontMatter(md)
        fm2.load()
        assert fm2.metadata["gpg_sig"] == sig
```

**Step 2: Run tests to verify they fail**

```bash
cd ~/github/repos/sigmark && pytest tests/test_frontmatter.py -v
```

Expected: FAIL — `ModuleNotFoundError: No module named 'sigmark.frontmatter'`

**Step 3: Write the implementation**

```python
# src/sigmark/frontmatter.py
"""YAML front matter parsing and writing for markdown files."""

from __future__ import annotations

import re
from pathlib import Path
from typing import Any

import yaml

# Match YAML front matter: opening ---, content, closing ---
_FM_PATTERN = re.compile(r"\A---\n(.*?)\n---\n?(.*)\Z", re.DOTALL)


class FrontMatter:
    """Read and write YAML front matter in markdown files."""

    def __init__(self, path: Path) -> None:
        self.path = Path(path)
        self.metadata: dict[str, Any] = {}
        self.body: str = ""
        self._loaded = False

    def load(self) -> bool:
        """Parse the file. Returns False if no front matter found."""
        text = self.path.read_text(encoding="utf-8")
        match = _FM_PATTERN.match(text)
        if not match:
            return False

        yaml_str, self.body = match.group(1), match.group(2)
        self.metadata = yaml.safe_load(yaml_str) or {}
        self._loaded = True
        return True

    def get(self, key: str, default: Any = None) -> Any:
        """Get a front matter value."""
        return self.metadata.get(key, default)

    def set(self, key: str, value: Any) -> None:
        """Set a front matter value."""
        self.metadata[key] = value

    def remove(self, key: str) -> bool:
        """Remove a front matter key. Returns True if it existed."""
        if key in self.metadata:
            del self.metadata[key]
            return True
        return False

    def save(self, dry_run: bool = False) -> bool:
        """Write the file back with updated front matter.

        Uses atomic write (write to temp, then replace).
        """
        if dry_run:
            return True

        yaml_str = yaml.dump(
            self.metadata,
            default_flow_style=False,
            allow_unicode=True,
            sort_keys=False,
            width=1000,
        )

        content = f"---\n{yaml_str}---\n{self.body}"

        tmp = self.path.with_suffix(".md.tmp")
        try:
            tmp.write_text(content, encoding="utf-8")
            tmp.replace(self.path)
            return True
        except OSError:
            if tmp.exists():
                tmp.unlink()
            return False
```

**Step 4: Run tests to verify they pass**

```bash
cd ~/github/repos/sigmark && pytest tests/test_frontmatter.py -v
```

Expected: All PASS

**Step 5: Commit**

```bash
cd ~/github/repos/sigmark
git add src/sigmark/frontmatter.py tests/test_frontmatter.py
git commit -m "feat: front matter parsing and writing module"
```

---

### Task 3: Scanner Module

**Files:**
- Create: `~/github/repos/sigmark/src/sigmark/scanner.py`
- Create: `~/github/repos/sigmark/tests/test_scanner.py`

**Step 1: Write the failing tests**

```python
# tests/test_scanner.py
"""Tests for markdown file scanning."""

from __future__ import annotations

from pathlib import Path

from sigmark.scanner import scan_content, ContentFile


class TestScanner:
    """Test content file discovery."""

    def test_finds_md_files_with_frontmatter(self, tmp_content: Path) -> None:
        files = list(scan_content(tmp_content))
        # Should find the two posts, NOT the README (no front matter)
        slugs = {f.slug for f in files}
        assert "hello-world" in slugs
        assert "second-post" in slugs
        assert len(files) == 2

    def test_skips_files_without_frontmatter(self, tmp_content: Path) -> None:
        files = list(scan_content(tmp_content))
        paths = {f.path.name for f in files}
        assert "README.md" not in paths

    def test_glob_filter(self, tmp_content: Path) -> None:
        files = list(scan_content(tmp_content, glob_pattern="**/hello-world/**"))
        assert len(files) == 1
        assert files[0].slug == "hello-world"

    def test_empty_directory(self, tmp_path: Path) -> None:
        files = list(scan_content(tmp_path))
        assert files == []

    def test_content_file_has_body(self, tmp_content: Path) -> None:
        files = list(scan_content(tmp_content))
        hello = next(f for f in files if f.slug == "hello-world")
        assert "body of the post" in hello.body

    def test_content_file_has_metadata(self, tmp_content: Path) -> None:
        files = list(scan_content(tmp_content))
        hello = next(f for f in files if f.slug == "hello-world")
        assert hello.metadata["title"] == "Hello World"

    def test_content_file_signed_status(self, tmp_content: Path) -> None:
        files = list(scan_content(tmp_content))
        for f in files:
            assert f.is_signed is False
            assert f.is_stale is False

    def test_detects_signed_content(self, tmp_path: Path) -> None:
        md = tmp_path / "signed" / "index.md"
        md.parent.mkdir()
        md.write_text(
            "---\n"
            "title: Signed\n"
            'gpg_sig: "test"\n'
            'gpg_body_hash: "sha256:abc"\n'
            "---\n"
            "Body.\n"
        )
        files = list(scan_content(tmp_path))
        assert len(files) == 1
        assert files[0].is_signed is True
```

**Step 2: Run tests to verify they fail**

```bash
cd ~/github/repos/sigmark && pytest tests/test_scanner.py -v
```

Expected: FAIL — `ModuleNotFoundError: No module named 'sigmark.scanner'`

**Step 3: Write the implementation**

```python
# src/sigmark/scanner.py
"""Scan directories for YAML-frontmatter markdown files."""

from __future__ import annotations

import fnmatch
import hashlib
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Iterator

from sigmark.frontmatter import FrontMatter


@dataclass
class ContentFile:
    """A markdown file with parsed front matter."""

    path: Path
    slug: str
    metadata: dict[str, Any] = field(default_factory=dict)
    body: str = ""

    @property
    def is_signed(self) -> bool:
        return "gpg_sig" in self.metadata

    @property
    def is_stale(self) -> bool:
        """Check if body has changed since signing (cheap hash check)."""
        stored_hash = self.metadata.get("gpg_body_hash", "")
        if not stored_hash:
            return False
        current_hash = compute_body_hash(self.body)
        return stored_hash != current_hash


def compute_body_hash(body: str) -> str:
    """Compute SHA-256 hash of normalized body text."""
    normalized = normalize_body(body)
    digest = hashlib.sha256(normalized.encode("utf-8")).hexdigest()
    return f"sha256:{digest}"


def normalize_body(body: str) -> str:
    """Normalize body text for reproducible hashing/signing.

    - Strip trailing whitespace per line
    - Ensure single trailing newline
    """
    lines = body.rstrip("\n").split("\n")
    lines = [line.rstrip() for line in lines]
    return "\n".join(lines) + "\n" if lines and lines != [""] else ""


def scan_content(
    root: Path,
    glob_pattern: str | None = None,
) -> Iterator[ContentFile]:
    """Walk a directory tree and yield ContentFile for each .md with front matter.

    Args:
        root: Directory to scan recursively
        glob_pattern: Optional glob to filter paths (e.g., "**/post/**")
    """
    root = Path(root)
    for md_path in sorted(root.rglob("*.md")):
        if glob_pattern and not fnmatch.fnmatch(str(md_path), glob_pattern):
            # Also try relative path matching
            rel = str(md_path.relative_to(root))
            if not fnmatch.fnmatch(rel, glob_pattern):
                continue

        fm = FrontMatter(md_path)
        if not fm.load():
            continue

        # Determine slug from path
        if md_path.name in ("index.md", "_index.md"):
            slug = md_path.parent.name
        else:
            slug = md_path.stem

        yield ContentFile(
            path=md_path,
            slug=slug,
            metadata=fm.metadata,
            body=fm.body,
        )
```

**Step 4: Run tests to verify they pass**

```bash
cd ~/github/repos/sigmark && pytest tests/test_scanner.py -v
```

Expected: All PASS

**Step 5: Commit**

```bash
cd ~/github/repos/sigmark
git add src/sigmark/scanner.py tests/test_scanner.py
git commit -m "feat: content scanner for finding markdown files"
```

---

### Task 4: Signer Module (GPG Subprocess Wrapper)

**Files:**
- Create: `~/github/repos/sigmark/src/sigmark/signer.py`
- Create: `~/github/repos/sigmark/tests/test_signer.py`

**Step 1: Write the failing tests**

These tests require the `gpg_home` fixture from conftest.py for an isolated keyring.

```python
# tests/test_signer.py
"""Tests for GPG signing and verification."""

from __future__ import annotations

import os
from pathlib import Path

import pytest

from sigmark.signer import gpg_sign, gpg_verify, GpgError, check_gpg_available
from sigmark.scanner import normalize_body


class TestGpgAvailability:
    """Test GPG binary detection."""

    def test_gpg_is_available(self) -> None:
        assert check_gpg_available() is True

    def test_gpg_not_available_with_bad_path(self, monkeypatch: pytest.MonkeyPatch) -> None:
        monkeypatch.setenv("PATH", "/nonexistent")
        assert check_gpg_available() is False


class TestNormalizeBody:
    """Test body normalization."""

    def test_strips_trailing_whitespace(self) -> None:
        assert normalize_body("hello   \nworld  \n") == "hello\nworld\n"

    def test_ensures_trailing_newline(self) -> None:
        assert normalize_body("hello\nworld") == "hello\nworld\n"

    def test_empty_body(self) -> None:
        assert normalize_body("") == ""

    def test_idempotent(self) -> None:
        text = "hello\nworld\n"
        assert normalize_body(normalize_body(text)) == normalize_body(text)


class TestGpgSign:
    """Test GPG signing operations."""

    def test_sign_text(self, gpg_home: Path) -> None:
        sig = gpg_sign("Hello, world!\n", gnupg_home=gpg_home)
        assert "-----BEGIN PGP SIGNATURE-----" in sig
        assert "-----END PGP SIGNATURE-----" in sig

    def test_sign_empty_fails(self, gpg_home: Path) -> None:
        with pytest.raises(GpgError):
            gpg_sign("", gnupg_home=gpg_home)

    def test_sign_with_key_id(self, gpg_home: Path) -> None:
        sig = gpg_sign("Test text\n", gnupg_home=gpg_home)
        assert "-----BEGIN PGP SIGNATURE-----" in sig


class TestGpgVerify:
    """Test GPG verification."""

    def test_sign_then_verify(self, gpg_home: Path) -> None:
        text = "Hello, world!\n"
        sig = gpg_sign(text, gnupg_home=gpg_home)
        result = gpg_verify(text, sig, gnupg_home=gpg_home)
        assert result.valid is True
        assert "test@example.com" in result.signer

    def test_verify_tampered_text(self, gpg_home: Path) -> None:
        text = "Hello, world!\n"
        sig = gpg_sign(text, gnupg_home=gpg_home)
        result = gpg_verify("Tampered text!\n", sig, gnupg_home=gpg_home)
        assert result.valid is False

    def test_verify_bad_signature(self, gpg_home: Path) -> None:
        result = gpg_verify("text\n", "not a real signature", gnupg_home=gpg_home)
        assert result.valid is False
```

**Step 2: Run tests to verify they fail**

```bash
cd ~/github/repos/sigmark && pytest tests/test_signer.py -v
```

Expected: FAIL — `ModuleNotFoundError: No module named 'sigmark.signer'`

**Step 3: Write the implementation**

```python
# src/sigmark/signer.py
"""GPG signing and verification via subprocess."""

from __future__ import annotations

import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path


class GpgError(Exception):
    """Raised when a GPG operation fails."""


@dataclass
class VerifyResult:
    """Result of a GPG verification."""

    valid: bool
    signer: str = ""
    error: str = ""


def check_gpg_available() -> bool:
    """Check if gpg binary is available on PATH."""
    try:
        subprocess.run(
            ["gpg", "--version"],
            capture_output=True,
            check=True,
        )
        return True
    except (FileNotFoundError, subprocess.CalledProcessError):
        return False


def gpg_sign(
    text: str,
    key_id: str | None = None,
    gnupg_home: Path | None = None,
) -> str:
    """Create a detached ASCII-armored GPG signature for the given text.

    Args:
        text: The text to sign
        key_id: Optional GPG key ID (uses default key if not specified)
        gnupg_home: Optional path to GNUPGHOME (for testing)

    Returns:
        ASCII-armored detached signature string

    Raises:
        GpgError: If signing fails or text is empty
    """
    if not text:
        raise GpgError("Cannot sign empty text")

    cmd = ["gpg", "--batch", "--yes", "--detach-sign", "--armor"]
    if key_id:
        cmd.extend(["--local-user", key_id])

    env = _gpg_env(gnupg_home)

    try:
        result = subprocess.run(
            cmd,
            input=text.encode("utf-8"),
            capture_output=True,
            env=env,
            check=True,
        )
        return result.stdout.decode("utf-8").strip()
    except subprocess.CalledProcessError as e:
        raise GpgError(f"GPG signing failed: {e.stderr.decode()}") from e


def gpg_verify(
    text: str,
    signature: str,
    gnupg_home: Path | None = None,
) -> VerifyResult:
    """Verify a detached GPG signature against text.

    Args:
        text: The original text
        signature: ASCII-armored detached signature
        gnupg_home: Optional path to GNUPGHOME (for testing)

    Returns:
        VerifyResult with valid status and signer info
    """
    env = _gpg_env(gnupg_home)

    with tempfile.TemporaryDirectory() as tmpdir:
        tmp = Path(tmpdir)
        sig_file = tmp / "content.sig"
        body_file = tmp / "content.txt"

        sig_file.write_text(signature, encoding="utf-8")
        body_file.write_text(text, encoding="utf-8")

        try:
            result = subprocess.run(
                ["gpg", "--batch", "--verify", str(sig_file), str(body_file)],
                capture_output=True,
                env=env,
            )

            stderr = result.stderr.decode("utf-8", errors="replace")

            if result.returncode == 0:
                signer = _extract_signer(stderr)
                return VerifyResult(valid=True, signer=signer)
            else:
                return VerifyResult(valid=False, error=stderr)
        except FileNotFoundError:
            return VerifyResult(valid=False, error="gpg binary not found")


def _gpg_env(gnupg_home: Path | None) -> dict[str, str] | None:
    """Build environment dict with optional GNUPGHOME override."""
    if gnupg_home is None:
        return None
    import os
    env = {**os.environ, "GNUPGHOME": str(gnupg_home)}
    return env


def _extract_signer(stderr: str) -> str:
    """Extract signer identity from GPG stderr output."""
    for line in stderr.splitlines():
        if "Good signature from" in line:
            # Extract the quoted name/email
            start = line.find('"')
            end = line.rfind('"')
            if start != -1 and end > start:
                return line[start + 1 : end]
    return "unknown"
```

**Step 4: Run tests to verify they pass**

```bash
cd ~/github/repos/sigmark && pytest tests/test_signer.py -v
```

Expected: All PASS

**Step 5: Commit**

```bash
cd ~/github/repos/sigmark
git add src/sigmark/signer.py tests/test_signer.py
git commit -m "feat: GPG signing and verification via subprocess"
```

---

### Task 5: CLI Commands — sign, verify, status, strip

**Files:**
- Modify: `~/github/repos/sigmark/src/sigmark/cli.py`
- Create: `~/github/repos/sigmark/tests/test_cli.py`

**Step 1: Write the failing tests**

```python
# tests/test_cli.py
"""Tests for sigmark CLI commands."""

from __future__ import annotations

import os
from pathlib import Path

from click.testing import CliRunner

from sigmark.cli import main


class TestSignCommand:
    """Test `sigmark sign`."""

    def test_sign_all_content(self, tmp_content: Path, gpg_home: Path) -> None:
        runner = CliRunner(env={"GNUPGHOME": str(gpg_home)})
        result = runner.invoke(main, ["sign", str(tmp_content)])
        assert result.exit_code == 0
        assert "Signed 2" in result.output or "2 signed" in result.output.lower()

        # Verify front matter was updated
        from sigmark.frontmatter import FrontMatter

        fm = FrontMatter(tmp_content / "post" / "hello-world" / "index.md")
        fm.load()
        assert "gpg_sig" in fm.metadata
        assert "BEGIN PGP SIGNATURE" in fm.metadata["gpg_sig"]
        assert "gpg_sig_date" in fm.metadata
        assert "gpg_body_hash" in fm.metadata

    def test_sign_skips_already_signed(self, tmp_content: Path, gpg_home: Path) -> None:
        runner = CliRunner(env={"GNUPGHOME": str(gpg_home)})
        # Sign once
        runner.invoke(main, ["sign", str(tmp_content)])
        # Sign again — should skip
        result = runner.invoke(main, ["sign", str(tmp_content)])
        assert result.exit_code == 0
        assert "skip" in result.output.lower() or "already" in result.output.lower() or "0" in result.output

    def test_sign_force_resigns(self, tmp_content: Path, gpg_home: Path) -> None:
        runner = CliRunner(env={"GNUPGHOME": str(gpg_home)})
        runner.invoke(main, ["sign", str(tmp_content)])
        result = runner.invoke(main, ["sign", "--force", str(tmp_content)])
        assert result.exit_code == 0

    def test_sign_dry_run(self, tmp_content: Path, gpg_home: Path) -> None:
        runner = CliRunner(env={"GNUPGHOME": str(gpg_home)})
        result = runner.invoke(main, ["-n", "sign", str(tmp_content)])
        assert result.exit_code == 0

        # Verify nothing was actually written
        from sigmark.frontmatter import FrontMatter

        fm = FrontMatter(tmp_content / "post" / "hello-world" / "index.md")
        fm.load()
        assert "gpg_sig" not in fm.metadata


class TestVerifyCommand:
    """Test `sigmark verify`."""

    def test_verify_signed_content(self, tmp_content: Path, gpg_home: Path) -> None:
        runner = CliRunner(env={"GNUPGHOME": str(gpg_home)})
        runner.invoke(main, ["sign", str(tmp_content)])
        result = runner.invoke(main, ["verify", str(tmp_content)])
        assert result.exit_code == 0

    def test_verify_unsigned_content(self, tmp_content: Path, gpg_home: Path) -> None:
        runner = CliRunner(env={"GNUPGHOME": str(gpg_home)})
        result = runner.invoke(main, ["verify", str(tmp_content)])
        assert result.exit_code == 0
        assert "no signed" in result.output.lower() or "0" in result.output


class TestStatusCommand:
    """Test `sigmark status`."""

    def test_status_shows_counts(self, tmp_content: Path, gpg_home: Path) -> None:
        runner = CliRunner(env={"GNUPGHOME": str(gpg_home)})
        result = runner.invoke(main, ["status", str(tmp_content)])
        assert result.exit_code == 0

    def test_status_json_output(self, tmp_content: Path, gpg_home: Path) -> None:
        runner = CliRunner(env={"GNUPGHOME": str(gpg_home)})
        result = runner.invoke(main, ["status", "--json", str(tmp_content)])
        assert result.exit_code == 0
        import json
        data = json.loads(result.output)
        assert "total" in data
        assert "unsigned" in data


class TestStripCommand:
    """Test `sigmark strip`."""

    def test_strip_removes_signatures(self, tmp_content: Path, gpg_home: Path) -> None:
        runner = CliRunner(env={"GNUPGHOME": str(gpg_home)})
        # Sign first
        runner.invoke(main, ["sign", str(tmp_content)])
        # Strip
        result = runner.invoke(main, ["strip", str(tmp_content)])
        assert result.exit_code == 0

        from sigmark.frontmatter import FrontMatter

        fm = FrontMatter(tmp_content / "post" / "hello-world" / "index.md")
        fm.load()
        assert "gpg_sig" not in fm.metadata
        assert "gpg_sig_date" not in fm.metadata
        assert "gpg_body_hash" not in fm.metadata
```

**Step 2: Run tests to verify they fail**

```bash
cd ~/github/repos/sigmark && pytest tests/test_cli.py -v
```

Expected: FAIL — commands not registered yet

**Step 3: Write the full CLI implementation**

Replace `src/sigmark/cli.py` with the complete implementation:

```python
# src/sigmark/cli.py
"""sigmark CLI — GPG signing for static site markdown content."""

from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path

import click
from rich.console import Console
from rich.table import Table

from sigmark import __version__
from sigmark.frontmatter import FrontMatter
from sigmark.scanner import ContentFile, compute_body_hash, normalize_body, scan_content
from sigmark.signer import GpgError, VerifyResult, check_gpg_available, gpg_sign, gpg_verify

console = Console()


@click.group()
@click.version_option(version=__version__, prog_name="sigmark")
@click.option("-v", "--verbose", is_flag=True, help="Enable verbose output")
@click.option("-n", "--dry-run", is_flag=True, help="Preview without making changes")
@click.pass_context
def main(ctx: click.Context, verbose: bool, dry_run: bool) -> None:
    """GPG signing for static site markdown content."""
    ctx.ensure_object(dict)
    ctx.obj["verbose"] = verbose
    ctx.obj["dry_run"] = dry_run


@main.command()
@click.argument("path", default=".", type=click.Path(exists=True, path_type=Path))
@click.option("--key-id", help="GPG key ID (uses default key if not specified)")
@click.option("--force", is_flag=True, help="Re-sign even if signature is current")
@click.option("--glob", "glob_pattern", help="Glob pattern to filter files")
@click.pass_context
def sign(
    ctx: click.Context,
    path: Path,
    key_id: str | None,
    force: bool,
    glob_pattern: str | None,
) -> None:
    """Sign markdown content with GPG."""
    dry_run = ctx.obj["dry_run"]
    verbose = ctx.obj["verbose"]

    if not check_gpg_available():
        console.print("[red]Error:[/red] gpg binary not found on PATH")
        ctx.exit(1)
        return

    signed_count = 0
    skipped_count = 0
    error_count = 0

    for content in scan_content(path, glob_pattern=glob_pattern):
        if content.is_signed and not force:
            # Check staleness
            if not content.is_stale:
                skipped_count += 1
                if verbose:
                    console.print(f"  [dim]skip[/dim] {content.slug} (current)")
                continue

        normalized = normalize_body(content.body)
        if not normalized:
            skipped_count += 1
            if verbose:
                console.print(f"  [dim]skip[/dim] {content.slug} (empty body)")
            continue

        try:
            gnupg_home = _gnupg_home_from_env()
            sig = gpg_sign(normalized, key_id=key_id, gnupg_home=gnupg_home)
        except GpgError as e:
            error_count += 1
            console.print(f"  [red]error[/red] {content.slug}: {e}")
            continue

        fm = FrontMatter(content.path)
        fm.load()
        fm.set("gpg_sig", sig + "\n")
        fm.set("gpg_sig_date", datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"))
        fm.set("gpg_body_hash", compute_body_hash(content.body))

        if fm.save(dry_run=dry_run):
            signed_count += 1
            label = "[yellow]dry-run[/yellow]" if dry_run else "[green]signed[/green]"
            console.print(f"  {label} {content.slug}")
        else:
            error_count += 1
            console.print(f"  [red]error[/red] {content.slug}: failed to save")

    console.print()
    console.print(
        f"Signed {signed_count}, skipped {skipped_count}, errors {error_count}"
    )


@main.command()
@click.argument("path", default=".", type=click.Path(exists=True, path_type=Path))
@click.option("--glob", "glob_pattern", help="Glob pattern to filter files")
@click.pass_context
def verify(ctx: click.Context, path: Path, glob_pattern: str | None) -> None:
    """Verify GPG signatures on markdown content."""
    verbose = ctx.obj["verbose"]

    if not check_gpg_available():
        console.print("[red]Error:[/red] gpg binary not found on PATH")
        ctx.exit(1)
        return

    valid_count = 0
    invalid_count = 0
    unsigned_count = 0

    for content in scan_content(path, glob_pattern=glob_pattern):
        if not content.is_signed:
            unsigned_count += 1
            if verbose:
                console.print(f"  [dim]unsigned[/dim] {content.slug}")
            continue

        normalized = normalize_body(content.body)
        sig = content.metadata["gpg_sig"]
        gnupg_home = _gnupg_home_from_env()
        result = gpg_verify(normalized, sig, gnupg_home=gnupg_home)

        if result.valid:
            valid_count += 1
            console.print(f"  [green]valid[/green] {content.slug} — {result.signer}")
        else:
            invalid_count += 1
            console.print(f"  [red]INVALID[/red] {content.slug}")
            if verbose:
                console.print(f"    {result.error}")

    console.print()
    if unsigned_count > 0 and valid_count == 0 and invalid_count == 0:
        console.print(f"No signed content found ({unsigned_count} unsigned)")
    else:
        console.print(
            f"Valid {valid_count}, invalid {invalid_count}, unsigned {unsigned_count}"
        )


@main.command()
@click.argument("path", default=".", type=click.Path(exists=True, path_type=Path))
@click.option("--glob", "glob_pattern", help="Glob pattern to filter files")
@click.option("--json", "as_json", is_flag=True, help="Output as JSON")
def status(path: Path, glob_pattern: str | None, as_json: bool) -> None:
    """Show signing status of markdown content."""
    files = list(scan_content(path, glob_pattern=glob_pattern))

    signed = [f for f in files if f.is_signed]
    stale = [f for f in signed if f.is_stale]
    unsigned = [f for f in files if not f.is_signed]

    if as_json:
        data = {
            "total": len(files),
            "signed": len(signed),
            "stale": len(stale),
            "unsigned": len(unsigned),
            "files": [
                {
                    "slug": f.slug,
                    "path": str(f.path),
                    "signed": f.is_signed,
                    "stale": f.is_stale,
                }
                for f in files
            ],
        }
        click.echo(json.dumps(data, indent=2))
        return

    table = Table(title=f"Signing Status ({len(files)} files)")
    table.add_column("Slug", style="cyan")
    table.add_column("Status")
    table.add_column("Path", style="dim")

    for f in files:
        if f.is_signed and f.is_stale:
            status_str = "[yellow]stale[/yellow]"
        elif f.is_signed:
            status_str = "[green]signed[/green]"
        else:
            status_str = "[red]unsigned[/red]"

        rel_path = str(f.path)
        table.add_row(f.slug, status_str, rel_path)

    console.print(table)
    console.print()
    console.print(
        f"[green]{len(signed)}[/green] signed, "
        f"[yellow]{len(stale)}[/yellow] stale, "
        f"[red]{len(unsigned)}[/red] unsigned"
    )


@main.command()
@click.argument("path", default=".", type=click.Path(exists=True, path_type=Path))
@click.option("--glob", "glob_pattern", help="Glob pattern to filter files")
@click.pass_context
def strip(ctx: click.Context, path: Path, glob_pattern: str | None) -> None:
    """Remove GPG signatures from markdown content."""
    dry_run = ctx.obj["dry_run"]
    stripped_count = 0

    for content in scan_content(path, glob_pattern=glob_pattern):
        if not content.is_signed:
            continue

        fm = FrontMatter(content.path)
        fm.load()
        fm.remove("gpg_sig")
        fm.remove("gpg_sig_date")
        fm.remove("gpg_body_hash")

        if fm.save(dry_run=dry_run):
            stripped_count += 1
            label = "[yellow]dry-run[/yellow]" if dry_run else "[green]stripped[/green]"
            console.print(f"  {label} {content.slug}")

    console.print()
    console.print(f"Stripped {stripped_count} signatures")


def _gnupg_home_from_env() -> Path | None:
    """Get GNUPGHOME from environment if set."""
    import os

    home = os.environ.get("GNUPGHOME")
    return Path(home) if home else None
```

**Step 4: Run tests to verify they pass**

```bash
cd ~/github/repos/sigmark && pytest tests/test_cli.py -v
```

Expected: All PASS

**Step 5: Run full test suite**

```bash
cd ~/github/repos/sigmark && pytest --cov=sigmark --cov-report=term-missing
```

Expected: All tests pass, good coverage

**Step 6: Commit**

```bash
cd ~/github/repos/sigmark
git add src/sigmark/cli.py tests/test_cli.py
git commit -m "feat: CLI commands — sign, verify, status, strip"
```

---

### Task 6: Hugo Template

**Files:**
- Create: `~/github/repos/sigmark/hugo/layouts/partials/gpg-badge.html`

**Step 1: Write the Hugo partial**

```html
{{/* GPG Signature Badge for sigmark */}}
{{/* Usage: {{ partial "gpg-badge.html" . }} */}}
{{ with .Params.gpg_sig }}
<div class="gpg-badge" style="margin: 1em 0; padding: 0.5em 1em; border: 1px solid #4a9; border-radius: 4px; background: #f0faf5; font-size: 0.85em;">
  <span style="color: #2a7; font-weight: bold;">&#x1f512; GPG Signed</span>
  {{ with $.Params.gpg_sig_date }}
    <span style="color: #666; margin-left: 0.5em;">
      {{ dateFormat "Jan 2, 2006" (time .) }}
    </span>
  {{ end }}
  <details style="margin-top: 0.5em;">
    <summary style="cursor: pointer; color: #666;">View signature</summary>
    <pre style="margin-top: 0.5em; padding: 0.5em; background: #f5f5f5; overflow-x: auto; font-size: 0.8em;"><code>{{ . }}</code></pre>
  </details>
</div>
{{ end }}
```

**Step 2: Commit**

```bash
cd ~/github/repos/sigmark
git add hugo/
git commit -m "feat: Hugo partial for GPG signature badge"
```

---

### Task 7: Final Polish and README

**Files:**
- Create: `~/github/repos/sigmark/README.md`

**Step 1: Write the README**

```markdown
# sigmark

GPG signing for static site markdown content. Works with Hugo, Jekyll, Zola, Eleventy — any static site generator that uses YAML front matter.

## Install

```bash
pip install sigmark
```

Requires `gpg` binary on your PATH.

## Usage

```bash
# Sign all markdown content in the current directory
sigmark sign

# Sign a specific directory
sigmark sign content/

# Check signing status
sigmark status

# Verify all signatures
sigmark verify

# Re-sign everything (including already-signed)
sigmark sign --force

# Remove all signatures
sigmark strip

# Dry run (preview without changes)
sigmark -n sign
```

## What it does

sigmark signs the **body** of your markdown files (everything below the YAML front matter `---` delimiters) using GPG, and stores the signature in the front matter:

```yaml
title: "My Post"
date: 2026-02-17
tags: ["cryptography"]
gpg_sig: |
  -----BEGIN PGP SIGNATURE-----
  iQIzBAABCAAdFiEE...
  -----END PGP SIGNATURE-----
gpg_sig_date: "2026-02-17T14:30:00Z"
gpg_body_hash: "sha256:a1b2c3d4..."
```

Only the body is signed — you can freely change tags, categories, and other metadata without invalidating the signature.

## Hugo Integration

Copy `hugo/layouts/partials/gpg-badge.html` into your Hugo site's `layouts/partials/` directory, then add to your templates:

```html
{{ partial "gpg-badge.html" . }}
```

This renders a "GPG Signed" badge with an expandable signature on signed posts.

## Verification

Readers can verify your content independently:

```bash
# Extract the body and signature, then verify
sigmark verify content/post/my-post/
```

Or manually with GPG:

```bash
# Extract body (everything after the second ---) and save to body.txt
# Copy gpg_sig value to sig.asc
gpg --verify sig.asc body.txt
```

## License

MIT
```

**Step 2: Run full test suite one last time**

```bash
cd ~/github/repos/sigmark && pytest --cov=sigmark --cov-report=term-missing -v
```

**Step 3: Commit**

```bash
cd ~/github/repos/sigmark
git add README.md
git commit -m "docs: add README"
```

---

### Task 8: Create GitHub Repository and Push

**Step 1: Create the repo**

```bash
cd ~/github/repos/sigmark
gh repo create queelius/sigmark --public --source=. --push --description "GPG signing for static site markdown content"
```

**Step 2: Verify**

```bash
gh repo view queelius/sigmark
```
