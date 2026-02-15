# Phase 2: Global Config + mf Posts Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add global config (`~/.config/mf/config.yaml` with `site_root`) so mf works from any directory, and add `mf posts create/list/set/tag/feature` commands for managing Hugo blog posts without a database.

**Architecture:** Hugo is the source of truth for posts — no `posts_db.json`. mf provides a convenience layer reading/writing front matter directly via `FrontMatterEditor` and `ContentScanner`. Global config adds a fallback to `find_mf_root()` so the CLI works from any directory.

**Tech Stack:** Python 3.11+, Click CLI, PyYAML, Rich (console output), pytest

---

## Context

**Design doc:** `docs/plans/2026-02-13-global-mf-plugin-design.md`

**Key existing code:**
- `scripts/mf/src/mf/core/config.py` — `SitePaths` dataclass, `find_mf_root()`, `get_paths()`
- `scripts/mf/src/mf/cli.py` — Click CLI with `Context` class, command registration at bottom
- `scripts/mf/src/mf/content/frontmatter.py` — `FrontMatterEditor` (load/set/add_to_list/remove_from_list/save)
- `scripts/mf/src/mf/content/scanner.py` — `ContentScanner` with `CONTENT_TYPES` dict, `ContentItem` dataclass
- `scripts/mf/src/mf/config/commands.py` — Existing `mf config` commands (show/get/set/reset/path), local `.mf/config.yaml`
- `scripts/mf/tests/conftest.py` — `mock_site_root`, `create_content_file` fixtures

**Patterns to follow:**
- Command groups: `@click.group(name="posts")` with subcommands (see `series/commands.py`)
- Field ops: `FrontMatterEditor` for in-place front matter edits (no database needed)
- Scanner: `ContentScanner.scan_type("post")` for listing posts
- Tests: `mock_site_root` fixture with `create_content_file` factory
- Registration: `from mf.posts.commands import posts` + `main.add_command(posts)` at bottom of `cli.py`

---

### Task 1: Add Global Config Support to config.py

**Files:**
- Modify: `scripts/mf/src/mf/core/config.py`
- Test: `scripts/mf/tests/test_core/test_config.py`

**Step 1: Write failing tests for global config fallback**

Create/append to `scripts/mf/tests/test_core/test_config.py`:

```python
"""Tests for global config fallback."""

import os
import pytest
import yaml
from pathlib import Path

from mf.core.config import (
    find_mf_root,
    get_paths,
    load_global_config,
    get_global_config_path,
)


class TestGlobalConfig:
    """Tests for ~/.config/mf/config.yaml support."""

    def test_get_global_config_path(self):
        """Global config lives at ~/.config/mf/config.yaml."""
        path = get_global_config_path()
        assert path == Path.home() / ".config" / "mf" / "config.yaml"

    def test_load_global_config_missing_file(self, tmp_path, monkeypatch):
        """Returns empty dict when global config doesn't exist."""
        monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path))
        result = load_global_config()
        assert result == {}

    def test_load_global_config_with_site_root(self, tmp_path, monkeypatch):
        """Reads site_root from global config."""
        config_dir = tmp_path / "mf"
        config_dir.mkdir()
        config_file = config_dir / "config.yaml"
        config_file.write_text(yaml.dump({"site_root": "/home/user/mysite"}))
        monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path))
        result = load_global_config()
        assert result["site_root"] == "/home/user/mysite"

    def test_find_mf_root_env_var_override(self, tmp_path, monkeypatch):
        """MF_SITE_ROOT env var takes highest priority."""
        # Create .mf/ in tmp_path
        (tmp_path / ".mf").mkdir()
        monkeypatch.setenv("MF_SITE_ROOT", str(tmp_path))
        # Clear lru_cache
        from mf.core.config import get_site_root
        get_site_root.cache_clear()
        result = find_mf_root(start_path=Path("/nonexistent"))
        assert result == tmp_path

    def test_find_mf_root_global_config_fallback(self, tmp_path, monkeypatch):
        """Falls back to global config site_root when .mf/ walk fails."""
        site_root = tmp_path / "mysite"
        site_root.mkdir()
        (site_root / ".mf").mkdir()

        # Set up global config
        config_dir = tmp_path / "config_home" / "mf"
        config_dir.mkdir(parents=True)
        (config_dir / "config.yaml").write_text(
            yaml.dump({"site_root": str(site_root)})
        )
        monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path / "config_home"))

        # Start from a directory with no .mf/
        other_dir = tmp_path / "other_repo"
        other_dir.mkdir()

        from mf.core.config import get_site_root
        get_site_root.cache_clear()
        result = find_mf_root(start_path=other_dir)
        assert result == site_root

    def test_find_mf_root_global_config_invalid_path(self, tmp_path, monkeypatch):
        """Raises FileNotFoundError if global config site_root has no .mf/."""
        config_dir = tmp_path / "config_home" / "mf"
        config_dir.mkdir(parents=True)
        (config_dir / "config.yaml").write_text(
            yaml.dump({"site_root": "/nonexistent/path"})
        )
        monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path / "config_home"))

        other_dir = tmp_path / "other_repo"
        other_dir.mkdir()

        from mf.core.config import get_site_root
        get_site_root.cache_clear()
        with pytest.raises(FileNotFoundError):
            find_mf_root(start_path=other_dir)

    def test_find_mf_root_local_takes_priority_over_global(
        self, tmp_path, monkeypatch
    ):
        """Local .mf/ walk takes priority over global config."""
        # Local site with .mf/
        local_site = tmp_path / "local"
        local_site.mkdir()
        (local_site / ".mf").mkdir()

        # Global config points elsewhere
        global_site = tmp_path / "global"
        global_site.mkdir()
        (global_site / ".mf").mkdir()
        config_dir = tmp_path / "config_home" / "mf"
        config_dir.mkdir(parents=True)
        (config_dir / "config.yaml").write_text(
            yaml.dump({"site_root": str(global_site)})
        )
        monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path / "config_home"))

        from mf.core.config import get_site_root
        get_site_root.cache_clear()
        result = find_mf_root(start_path=local_site)
        assert result == local_site
```

**Step 2: Run tests to verify they fail**

Run: `cd /home/spinoza/github/repos/metafunctor/scripts/mf && python -m pytest tests/test_core/test_config.py -v`
Expected: FAIL — `load_global_config` and `get_global_config_path` don't exist yet, `find_mf_root` doesn't check env var or global config.

**Step 3: Implement global config support**

Modify `scripts/mf/src/mf/core/config.py`:

```python
"""
Configuration and path management.

Provides site root detection and standard paths for the Hugo site.
Uses .mf/ directory for mf-specific data (databases, cache, backups).

Resolution order for site root:
1. MF_SITE_ROOT environment variable
2. Walk up from cwd looking for .mf/ directory
3. Global config at ~/.config/mf/config.yaml (site_root key)
"""

from __future__ import annotations

import os
from dataclasses import dataclass
from functools import lru_cache
from pathlib import Path

import yaml


@dataclass(frozen=True)
class SitePaths:
    """Standard paths for the Hugo site and mf data."""

    root: Path
    mf_dir: Path
    content: Path
    static: Path

    # Content directories
    papers: Path
    projects: Path
    publications: Path
    posts: Path

    # Static directories
    latex: Path

    # Data files (in .mf/)
    paper_db: Path
    projects_db: Path
    projects_cache: Path
    config_file: Path

    # Backup directories (in .mf/)
    paper_backups: Path
    projects_backups: Path
    series_backups: Path

    # Series database
    series_db: Path


def get_global_config_path() -> Path:
    """Get path to global config file.

    Uses XDG_CONFIG_HOME if set, otherwise ~/.config.
    """
    config_home = os.environ.get("XDG_CONFIG_HOME", str(Path.home() / ".config"))
    return Path(config_home) / "mf" / "config.yaml"


def load_global_config() -> dict:
    """Load global configuration from ~/.config/mf/config.yaml.

    Returns:
        Config dict (empty if file doesn't exist)
    """
    config_path = get_global_config_path()
    if not config_path.exists():
        return {}

    try:
        content = config_path.read_text()
        loaded = yaml.safe_load(content)
        return loaded if isinstance(loaded, dict) else {}
    except Exception:
        return {}


def find_mf_root(start_path: Path | None = None) -> Path:
    """Find project root by looking for .mf/ directory.

    Resolution order:
    1. MF_SITE_ROOT environment variable (if set and valid)
    2. Walk up from start_path looking for .mf/
    3. Global config site_root from ~/.config/mf/config.yaml

    Args:
        start_path: Starting path for search (defaults to cwd)

    Returns:
        Path to project root

    Raises:
        FileNotFoundError: If .mf/ directory not found by any method
    """
    # 1. Check MF_SITE_ROOT env var
    env_root = os.environ.get("MF_SITE_ROOT")
    if env_root:
        env_path = Path(env_root).resolve()
        if (env_path / ".mf").is_dir():
            return env_path

    # 2. Walk up the directory tree looking for .mf/
    if start_path is None:
        start_path = Path.cwd()

    current = Path(start_path).resolve()
    while current != current.parent:
        if (current / ".mf").is_dir():
            return current
        current = current.parent

    # 3. Fall back to global config
    global_config = load_global_config()
    site_root_str = global_config.get("site_root")
    if site_root_str:
        site_root = Path(site_root_str).resolve()
        if (site_root / ".mf").is_dir():
            return site_root

    raise FileNotFoundError(
        f"Could not find .mf/ directory starting from {start_path}. "
        f"Options: run 'mf init', set MF_SITE_ROOT, or configure "
        f"site_root in {get_global_config_path()}"
    )


# Keep old name as alias for compatibility
find_site_root = find_mf_root


@lru_cache(maxsize=1)
def get_site_root() -> Path:
    """Get the cached site root path."""
    return find_mf_root()


def get_paths(site_root: Path | None = None) -> SitePaths:
    """Get all standard paths for the site."""
    if site_root is None:
        site_root = get_site_root()

    site_root = Path(site_root)
    mf_dir = site_root / ".mf"

    return SitePaths(
        root=site_root,
        mf_dir=mf_dir,
        content=site_root / "content",
        static=site_root / "static",
        # Content directories
        papers=site_root / "content" / "papers",
        projects=site_root / "content" / "projects",
        publications=site_root / "content" / "publications",
        posts=site_root / "content" / "post",
        # Static directories
        latex=site_root / "static" / "latex",
        # Data files in .mf/
        paper_db=mf_dir / "paper_db.json",
        projects_db=mf_dir / "projects_db.json",
        projects_cache=mf_dir / "cache" / "projects.json",
        config_file=mf_dir / "config.yaml",
        # Backup directories in .mf/
        paper_backups=mf_dir / "backups" / "papers",
        projects_backups=mf_dir / "backups" / "projects",
        series_backups=mf_dir / "backups" / "series",
        # Series database
        series_db=mf_dir / "series_db.json",
    )
```

**Step 4: Run tests to verify they pass**

Run: `cd /home/spinoza/github/repos/metafunctor/scripts/mf && python -m pytest tests/test_core/test_config.py -v`
Expected: All PASS

**Step 5: Run full test suite to check for regressions**

Run: `cd /home/spinoza/github/repos/metafunctor/scripts/mf && python -m pytest --tb=short`
Expected: All pass. The new `posts` field on `SitePaths` may cause some test fixtures to need updating — specifically `mock_site_root` in `conftest.py` doesn't create `content/post` (wait, it already does at line 115). Any test that constructs `SitePaths` directly will need the new `posts` parameter, but since most tests use `get_paths()`, this should be fine.

**Step 6: Commit**

```bash
git add scripts/mf/src/mf/core/config.py scripts/mf/tests/test_core/test_config.py
git commit -m "feat(mf): add global config fallback and posts path to SitePaths

Resolution order: MF_SITE_ROOT env var > local .mf/ walk > ~/.config/mf/config.yaml
Add posts field to SitePaths pointing to content/post/"
```

---

### Task 2: Update conftest.py for Posts Content

**Files:**
- Modify: `scripts/mf/tests/conftest.py`

**Step 1: Update mock_site_root if needed**

Check if `content/post` directory is already created in `mock_site_root`. Looking at conftest.py line 115: `(tmp_path / "content" / "post").mkdir(parents=True)` — yes, it already exists.

No changes needed unless tests from Task 1 revealed fixture issues. If all tests pass from Task 1, skip this task.

**Step 2: Commit (if changes made)**

```bash
git add scripts/mf/tests/conftest.py
git commit -m "test(mf): update fixtures for posts support"
```

---

### Task 3: Create Posts Module — `mf posts list`

**Files:**
- Create: `scripts/mf/src/mf/posts/__init__.py`
- Create: `scripts/mf/src/mf/posts/commands.py`
- Create: `scripts/mf/tests/test_posts/__init__.py`
- Create: `scripts/mf/tests/test_posts/test_commands.py`

**Step 1: Write failing tests for `mf posts list`**

Create `scripts/mf/tests/test_posts/__init__.py` (empty).

Create `scripts/mf/tests/test_posts/test_commands.py`:

```python
"""Tests for mf posts commands."""

from __future__ import annotations

from datetime import datetime

import pytest
from click.testing import CliRunner

from mf.cli import main


@pytest.fixture
def runner():
    return CliRunner()


@pytest.fixture
def posts_setup(create_content_file):
    """Create several test posts."""
    create_content_file(
        content_type="post",
        slug="2024-01-15-first-post",
        title="First Post",
        extra_fm={
            "date": "2024-01-15",
            "tags": ["python", "testing"],
            "categories": ["Programming"],
            "featured": True,
        },
    )
    create_content_file(
        content_type="post",
        slug="2024-02-20-second-post",
        title="Second Post",
        extra_fm={
            "date": "2024-02-20",
            "tags": ["rust", "systems"],
            "categories": ["Programming"],
            "series": ["stepanov"],
        },
    )
    create_content_file(
        content_type="post",
        slug="2024-03-10-draft-post",
        title="Draft Post",
        extra_fm={
            "date": "2024-03-10",
            "tags": ["draft-tag"],
        },
        draft=True,
    )


class TestPostsList:
    """Tests for mf posts list."""

    def test_list_all_posts(self, runner, posts_setup):
        result = runner.invoke(main, ["posts", "list"])
        assert result.exit_code == 0
        assert "First Post" in result.output
        assert "Second Post" in result.output
        # Drafts excluded by default
        assert "Draft Post" not in result.output

    def test_list_with_drafts(self, runner, posts_setup):
        result = runner.invoke(main, ["posts", "list", "--include-drafts"])
        assert result.exit_code == 0
        assert "Draft Post" in result.output

    def test_list_filter_by_tag(self, runner, posts_setup):
        result = runner.invoke(main, ["posts", "list", "--tag", "python"])
        assert result.exit_code == 0
        assert "First Post" in result.output
        assert "Second Post" not in result.output

    def test_list_filter_by_series(self, runner, posts_setup):
        result = runner.invoke(main, ["posts", "list", "--series", "stepanov"])
        assert result.exit_code == 0
        assert "Second Post" in result.output
        assert "First Post" not in result.output

    def test_list_filter_featured(self, runner, posts_setup):
        result = runner.invoke(main, ["posts", "list", "--featured"])
        assert result.exit_code == 0
        assert "First Post" in result.output
        assert "Second Post" not in result.output

    def test_list_json_output(self, runner, posts_setup):
        result = runner.invoke(main, ["posts", "list", "--json"])
        assert result.exit_code == 0
        import json
        data = json.loads(result.output)
        assert isinstance(data, list)
        assert len(data) == 2  # Excludes drafts
        slugs = [p["slug"] for p in data]
        assert "2024-01-15-first-post" in slugs

    def test_list_filter_by_query(self, runner, posts_setup):
        result = runner.invoke(main, ["posts", "list", "-q", "First"])
        assert result.exit_code == 0
        assert "First Post" in result.output
        assert "Second Post" not in result.output
```

**Step 2: Run tests to verify they fail**

Run: `cd /home/spinoza/github/repos/metafunctor/scripts/mf && python -m pytest tests/test_posts/test_commands.py -v`
Expected: FAIL — `mf posts` command group doesn't exist yet.

**Step 3: Create posts module**

Create `scripts/mf/src/mf/posts/__init__.py`:

```python
"""Posts management — convenience layer over Hugo content files."""
```

Create `scripts/mf/src/mf/posts/commands.py`:

```python
"""CLI commands for post management.

Posts are Hugo-native content. No database — front matter is the source of truth.
mf provides convenience operations: list, create, set, tag, feature.
"""

from __future__ import annotations

import json as json_module
from datetime import datetime
from pathlib import Path
from typing import Any

import click
from rich.console import Console
from rich.table import Table

from mf.content.scanner import ContentScanner

console = Console()


@click.group(name="posts")
def posts() -> None:
    """Manage blog posts.

    Posts live in content/post/ as Hugo leaf bundles.
    No database — front matter is the source of truth.
    """
    pass


@posts.command(name="list")
@click.option("-q", "--query", help="Search in title/body")
@click.option("-t", "--tag", multiple=True, help="Filter by tag(s)")
@click.option("-c", "--category", multiple=True, help="Filter by category")
@click.option("-s", "--series", "series_filter", help="Filter by series slug")
@click.option("--featured", is_flag=True, help="Only featured posts")
@click.option("--include-drafts", is_flag=True, help="Include draft posts")
@click.option("--since", help="Posts since date (YYYY-MM-DD or 30d/4w/3m)")
@click.option("--json", "as_json", is_flag=True, help="Output as JSON")
@click.pass_obj
def list_posts(
    ctx,
    query: str | None,
    tag: tuple[str, ...],
    category: tuple[str, ...],
    series_filter: str | None,
    featured: bool,
    include_drafts: bool,
    since: str | None,
    as_json: bool,
) -> None:
    """List blog posts with optional filters.

    Examples:
        mf posts list
        mf posts list --tag python --tag ml
        mf posts list --series stepanov
        mf posts list --featured
        mf posts list --since 30d
        mf posts list --json
    """
    scanner = ContentScanner()
    items = scanner.scan_type("post", include_drafts=include_drafts)

    # Apply filters
    results = []
    since_date = _parse_since(since) if since else None

    for item in items:
        # Query filter
        if query and not item.mentions_text(query):
            continue

        # Tag filter (any match)
        if tag:
            item_tags = item.tags
            if not any(t in item_tags for t in tag):
                continue

        # Category filter (any match)
        if category:
            item_cats = item.categories
            if not any(c.lower() in [ic.lower() for ic in item_cats] for c in category):
                continue

        # Series filter
        if series_filter:
            item_series = item.front_matter.get("series", [])
            if isinstance(item_series, str):
                item_series = [item_series]
            if series_filter not in item_series:
                continue

        # Featured filter
        if featured and not item.front_matter.get("featured", False):
            continue

        # Since filter
        if since_date and item.date:
            try:
                item_date = datetime.fromisoformat(item.date[:10])
                if item_date < since_date:
                    continue
            except (ValueError, TypeError):
                pass

        results.append(item)

    # Sort by date (newest first)
    results.sort(key=lambda x: x.date or "", reverse=True)

    if as_json:
        output = []
        for item in results:
            output.append({
                "slug": item.slug,
                "title": item.title,
                "date": item.date,
                "tags": item.tags,
                "categories": item.categories,
                "series": item.front_matter.get("series", []),
                "featured": item.front_matter.get("featured", False),
                "draft": item.is_draft,
                "path": str(item.path),
            })
        console.print(json_module.dumps(output, indent=2, default=str))
        return

    if not results:
        console.print("[yellow]No posts found matching criteria[/yellow]")
        return

    table = Table(title=f"Posts ({len(results)} found)")
    table.add_column("Date", style="dim", width=10)
    table.add_column("Title")
    table.add_column("Tags", style="cyan")
    table.add_column("Flags")

    for item in results:
        flags = ""
        if item.front_matter.get("featured"):
            flags += "F "
        if item.is_draft:
            flags += "D "
        series_list = item.front_matter.get("series", [])
        if series_list:
            flags += "S "

        tag_str = ", ".join(item.tags[:3])
        if len(item.tags) > 3:
            tag_str += f" +{len(item.tags) - 3}"

        title_display = item.title
        if len(title_display) > 50:
            title_display = title_display[:47] + "..."

        table.add_row(
            (item.date or "")[:10],
            title_display,
            tag_str,
            flags.strip(),
        )

    console.print(table)
    console.print("\n[dim]F = Featured, D = Draft, S = In series[/dim]")


def _parse_since(since: str) -> datetime | None:
    """Parse a --since value into a datetime.

    Accepts:
        - ISO date: 2024-01-15
        - Relative: 30d, 4w, 3m, 1y
    """
    from datetime import timedelta

    now = datetime.now()

    # Try ISO date first
    try:
        return datetime.fromisoformat(since)
    except ValueError:
        pass

    # Try relative format
    since = since.strip().lower()
    if since.endswith("d"):
        days = int(since[:-1])
        return now - timedelta(days=days)
    elif since.endswith("w"):
        weeks = int(since[:-1])
        return now - timedelta(weeks=weeks)
    elif since.endswith("m"):
        months = int(since[:-1])
        return now - timedelta(days=months * 30)
    elif since.endswith("y"):
        years = int(since[:-1])
        return now - timedelta(days=years * 365)

    return None
```

**Step 4: Register posts command in cli.py**

Add to `scripts/mf/src/mf/cli.py` at the bottom, with the other imports:

```python
from mf.posts.commands import posts  # noqa: E402
```

And add:

```python
main.add_command(posts)
```

**Step 5: Run tests to verify they pass**

Run: `cd /home/spinoza/github/repos/metafunctor/scripts/mf && python -m pytest tests/test_posts/test_commands.py -v`
Expected: All PASS

**Step 6: Run full test suite**

Run: `cd /home/spinoza/github/repos/metafunctor/scripts/mf && python -m pytest --tb=short`
Expected: All pass (watch for `SitePaths` constructor changes from Task 1 causing issues in other tests)

**Step 7: Commit**

```bash
git add scripts/mf/src/mf/posts/ scripts/mf/tests/test_posts/ scripts/mf/src/mf/cli.py
git commit -m "feat(mf): add mf posts list command

List blog posts with filters: --tag, --series, --featured, --since, --query.
No database — reads front matter directly from content/post/."
```

---

### Task 4: Implement `mf posts create`

**Files:**
- Modify: `scripts/mf/src/mf/posts/commands.py`
- Modify: `scripts/mf/tests/test_posts/test_commands.py`

**Step 1: Write failing tests**

Append to `scripts/mf/tests/test_posts/test_commands.py`:

```python
class TestPostsCreate:
    """Tests for mf posts create."""

    def test_create_basic_post(self, runner, mock_site_root):
        result = runner.invoke(main, [
            "posts", "create",
            "--title", "My New Post",
            "--slug", "my-new-post",
        ])
        assert result.exit_code == 0
        assert "Created" in result.output

        # Verify file exists
        today = datetime.now().strftime("%Y-%m-%d")
        post_dir = mock_site_root / "content" / "post" / f"{today}-my-new-post"
        assert post_dir.exists()
        index_file = post_dir / "index.md"
        assert index_file.exists()

        # Verify front matter
        content = index_file.read_text()
        assert "title: My New Post" in content or "title: 'My New Post'" in content
        assert f"date: '{today}'" in content or f"date: {today}" in content or f"date: \"{today}\"" in content

    def test_create_post_with_metadata(self, runner, mock_site_root):
        result = runner.invoke(main, [
            "posts", "create",
            "--title", "Tagged Post",
            "--slug", "tagged-post",
            "--tag", "python",
            "--tag", "testing",
            "--category", "Programming",
            "--series", "stepanov",
        ])
        assert result.exit_code == 0

        today = datetime.now().strftime("%Y-%m-%d")
        index_file = (
            mock_site_root / "content" / "post" / f"{today}-tagged-post" / "index.md"
        )
        content = index_file.read_text()
        assert "python" in content
        assert "testing" in content
        assert "Programming" in content
        assert "stepanov" in content

    def test_create_post_auto_slug(self, runner, mock_site_root):
        """Slug is auto-generated from title if not provided."""
        result = runner.invoke(main, [
            "posts", "create",
            "--title", "My Amazing Blog Post!",
        ])
        assert result.exit_code == 0

        today = datetime.now().strftime("%Y-%m-%d")
        post_dir = mock_site_root / "content" / "post" / f"{today}-my-amazing-blog-post"
        assert post_dir.exists()

    def test_create_post_duplicate_slug(self, runner, mock_site_root):
        """Refuses to overwrite existing post."""
        runner.invoke(main, [
            "posts", "create", "--title", "First", "--slug", "dupe-test",
        ])
        result = runner.invoke(main, [
            "posts", "create", "--title", "Second", "--slug", "dupe-test",
        ])
        assert result.exit_code != 0 or "already exists" in result.output

    def test_create_post_with_description(self, runner, mock_site_root):
        result = runner.invoke(main, [
            "posts", "create",
            "--title", "Described Post",
            "--slug", "described",
            "--description", "A short summary for cards.",
        ])
        assert result.exit_code == 0

        today = datetime.now().strftime("%Y-%m-%d")
        index_file = (
            mock_site_root / "content" / "post" / f"{today}-described" / "index.md"
        )
        content = index_file.read_text()
        assert "A short summary for cards" in content
```

**Step 2: Run tests to verify they fail**

Run: `cd /home/spinoza/github/repos/metafunctor/scripts/mf && python -m pytest tests/test_posts/test_commands.py::TestPostsCreate -v`
Expected: FAIL — `create` command doesn't exist yet.

**Step 3: Implement create command**

Add to `scripts/mf/src/mf/posts/commands.py`:

```python
@posts.command(name="create")
@click.option("--title", required=True, help="Post title")
@click.option("--slug", help="URL slug (auto-generated from title if omitted)")
@click.option("--date", help="Post date (default: today, format: YYYY-MM-DD)")
@click.option("-t", "--tag", multiple=True, help="Tags (repeatable)")
@click.option("-c", "--category", multiple=True, help="Categories (repeatable)")
@click.option("-s", "--series", multiple=True, help="Series membership (repeatable)")
@click.option("--description", help="Card preview text")
@click.option("--featured", is_flag=True, help="Mark as featured")
@click.pass_obj
def create_post(
    ctx,
    title: str,
    slug: str | None,
    date: str | None,
    tag: tuple[str, ...],
    category: tuple[str, ...],
    series: tuple[str, ...],
    description: str | None,
    featured: bool,
) -> None:
    """Create a new blog post.

    Scaffolds a Hugo leaf bundle at content/post/YYYY-MM-DD-slug/index.md
    with front matter populated from the provided options.

    Examples:
        mf posts create --title "My New Post"
        mf posts create --title "Tagged" --tag python --tag ml --category AI
        mf posts create --title "Series Post" --series stepanov
    """
    import re

    import yaml

    from mf.core.config import get_paths

    paths = get_paths()

    # Generate slug from title if not provided
    if not slug:
        slug = _slugify(title)

    # Use today's date if not provided
    if not date:
        date = datetime.now().strftime("%Y-%m-%d")

    # Build directory name: YYYY-MM-DD-slug
    dir_name = f"{date}-{slug}"
    post_dir = paths.posts / dir_name

    if post_dir.exists():
        console.print(f"[red]Post already exists: {post_dir.relative_to(paths.root)}[/red]")
        raise SystemExit(1)

    # Build front matter
    fm: dict[str, Any] = {
        "title": title,
        "date": date,
    }

    if description:
        fm["description"] = description

    if tag:
        fm["tags"] = list(tag)

    if category:
        fm["categories"] = list(category)

    if series:
        fm["series"] = list(series)

    if featured:
        fm["featured"] = True

    fm["draft"] = True

    # Check dry_run
    dry_run = getattr(ctx, "dry_run", False) if ctx else False
    if dry_run:
        console.print(f"[dim]Would create: {post_dir.relative_to(paths.root)}/index.md[/dim]")
        return

    # Create directory and file
    post_dir.mkdir(parents=True, exist_ok=True)
    index_file = post_dir / "index.md"

    yaml_str = yaml.dump(fm, default_flow_style=False, allow_unicode=True, sort_keys=False)
    content = f"---\n{yaml_str}---\n\n"

    index_file.write_text(content, encoding="utf-8")

    rel_path = post_dir.relative_to(paths.root)
    console.print(f"[green]Created[/green] {rel_path}/index.md")
    console.print(f"\n[dim]Next steps:[/dim]")
    console.print(f"  [dim]→ Edit the post[/dim]")
    console.print(f"  [dim]→ Remove draft: true when ready[/dim]")
    console.print(f"  [dim]→ Cross-post: crier publish {rel_path}/index.md[/dim]")


def _slugify(title: str) -> str:
    """Convert title to URL-safe slug.

    Examples:
        "My Amazing Blog Post!" -> "my-amazing-blog-post"
        "C++ Templates" -> "cpp-templates"
        "What's New in 2024?" -> "whats-new-in-2024"
    """
    import re

    slug = title.lower()
    slug = slug.replace("c++", "cpp")
    slug = slug.replace("'", "")
    slug = slug.replace("'", "")
    slug = re.sub(r"[^a-z0-9]+", "-", slug)
    slug = slug.strip("-")
    return slug
```

**Step 4: Run tests**

Run: `cd /home/spinoza/github/repos/metafunctor/scripts/mf && python -m pytest tests/test_posts/test_commands.py::TestPostsCreate -v`
Expected: All PASS

**Step 5: Commit**

```bash
git add scripts/mf/src/mf/posts/commands.py scripts/mf/tests/test_posts/test_commands.py
git commit -m "feat(mf): add mf posts create command

Scaffolds content/post/YYYY-MM-DD-slug/index.md with front matter.
Supports --tag, --category, --series, --description, --featured.
Auto-generates slug from title when --slug is omitted."
```

---

### Task 5: Implement `mf posts set`

**Files:**
- Modify: `scripts/mf/src/mf/posts/commands.py`
- Modify: `scripts/mf/tests/test_posts/test_commands.py`

**Step 1: Write failing tests**

Append to `scripts/mf/tests/test_posts/test_commands.py`:

```python
class TestPostsSet:
    """Tests for mf posts set."""

    def test_set_field(self, runner, posts_setup, mock_site_root):
        result = runner.invoke(main, [
            "posts", "set", "2024-01-15-first-post", "description", "New description",
        ])
        assert result.exit_code == 0
        assert "Set" in result.output or "Updated" in result.output

        # Verify the change
        import yaml
        post_file = (
            mock_site_root / "content" / "post" / "2024-01-15-first-post" / "index.md"
        )
        content = post_file.read_text()
        assert "New description" in content

    def test_set_boolean_field(self, runner, posts_setup, mock_site_root):
        result = runner.invoke(main, [
            "posts", "set", "2024-02-20-second-post", "featured", "true",
        ])
        assert result.exit_code == 0

        post_file = (
            mock_site_root / "content" / "post" / "2024-02-20-second-post" / "index.md"
        )
        content = post_file.read_text()
        assert "featured: true" in content or "featured: True" in content

    def test_set_nonexistent_post(self, runner, posts_setup):
        result = runner.invoke(main, [
            "posts", "set", "nonexistent-slug", "title", "Foo",
        ])
        assert result.exit_code != 0 or "not found" in result.output.lower()

    def test_unset_field(self, runner, posts_setup, mock_site_root):
        result = runner.invoke(main, [
            "posts", "unset", "2024-01-15-first-post", "featured",
        ])
        assert result.exit_code == 0

        post_file = (
            mock_site_root / "content" / "post" / "2024-01-15-first-post" / "index.md"
        )
        content = post_file.read_text()
        assert "featured:" not in content
```

**Step 2: Run tests to verify they fail**

Run: `cd /home/spinoza/github/repos/metafunctor/scripts/mf && python -m pytest tests/test_posts/test_commands.py::TestPostsSet -v`
Expected: FAIL

**Step 3: Implement set and unset commands**

Add to `scripts/mf/src/mf/posts/commands.py`:

```python
def _find_post_file(slug: str) -> Path | None:
    """Find the index.md for a post by slug.

    Searches content/post/ for a directory ending with the slug.
    """
    from mf.core.config import get_paths

    posts_dir = get_paths().posts
    if not posts_dir.exists():
        return None

    # Try exact match first (slug is the full dir name)
    exact = posts_dir / slug / "index.md"
    if exact.exists():
        return exact

    # Search for directories ending with the slug
    for entry in posts_dir.iterdir():
        if entry.is_dir() and entry.name.endswith(slug):
            index = entry / "index.md"
            if index.exists():
                return index

    # Also try treating slug as a partial match on directory name
    for entry in posts_dir.iterdir():
        if entry.is_dir() and slug in entry.name:
            index = entry / "index.md"
            if index.exists():
                return index

    return None


def _coerce_value(value: str) -> Any:
    """Coerce a string value to the appropriate Python type."""
    if value.lower() in ("true", "yes"):
        return True
    if value.lower() in ("false", "no"):
        return False
    try:
        return int(value)
    except ValueError:
        pass
    try:
        return float(value)
    except ValueError:
        pass
    return value


@posts.command(name="set")
@click.argument("slug")
@click.argument("field")
@click.argument("value")
@click.pass_obj
def set_field(ctx, slug: str, field: str, value: str) -> None:
    """Set a front matter field on a post.

    Examples:
        mf posts set 2024-01-15-my-post description "New description"
        mf posts set my-post featured true
        mf posts set my-post series_weight 5
    """
    from mf.content.frontmatter import FrontMatterEditor

    post_file = _find_post_file(slug)
    if not post_file:
        console.print(f"[red]Post not found: {slug}[/red]")
        raise SystemExit(1)

    editor = FrontMatterEditor(post_file)
    if not editor.load():
        raise SystemExit(1)

    typed_value = _coerce_value(value)
    editor.set(field, typed_value)

    dry_run = getattr(ctx, "dry_run", False) if ctx else False
    if editor.save(dry_run=dry_run):
        console.print(f"[green]Set[/green] {field} = {typed_value} on {slug}")
    else:
        console.print(f"[red]Failed to save changes[/red]")
        raise SystemExit(1)


@posts.command(name="unset")
@click.argument("slug")
@click.argument("field")
@click.pass_obj
def unset_field(ctx, slug: str, field: str) -> None:
    """Remove a front matter field from a post.

    Examples:
        mf posts unset my-post featured
        mf posts unset my-post description
    """
    from mf.content.frontmatter import FrontMatterEditor

    post_file = _find_post_file(slug)
    if not post_file:
        console.print(f"[red]Post not found: {slug}[/red]")
        raise SystemExit(1)

    editor = FrontMatterEditor(post_file)
    if not editor.load():
        raise SystemExit(1)

    if field in editor.front_matter:
        del editor.front_matter[field]
    else:
        console.print(f"[dim]{field} not set on {slug}[/dim]")
        return

    dry_run = getattr(ctx, "dry_run", False) if ctx else False
    if editor.save(dry_run=dry_run):
        console.print(f"[green]Removed[/green] {field} from {slug}")
    else:
        console.print(f"[red]Failed to save changes[/red]")
        raise SystemExit(1)
```

**Step 4: Run tests**

Run: `cd /home/spinoza/github/repos/metafunctor/scripts/mf && python -m pytest tests/test_posts/test_commands.py::TestPostsSet -v`
Expected: All PASS

**Step 5: Commit**

```bash
git add scripts/mf/src/mf/posts/commands.py scripts/mf/tests/test_posts/test_commands.py
git commit -m "feat(mf): add mf posts set/unset commands

Edit front matter fields in-place using FrontMatterEditor.
Auto-coerces values (bool, int, float, string).
Finds posts by slug with fuzzy directory matching."
```

---

### Task 6: Implement `mf posts tag`

**Files:**
- Modify: `scripts/mf/src/mf/posts/commands.py`
- Modify: `scripts/mf/tests/test_posts/test_commands.py`

**Step 1: Write failing tests**

Append to `scripts/mf/tests/test_posts/test_commands.py`:

```python
class TestPostsTag:
    """Tests for mf posts tag."""

    def test_add_tag(self, runner, posts_setup, mock_site_root):
        result = runner.invoke(main, [
            "posts", "tag", "2024-01-15-first-post", "--add", "new-tag",
        ])
        assert result.exit_code == 0

        post_file = (
            mock_site_root / "content" / "post" / "2024-01-15-first-post" / "index.md"
        )
        content = post_file.read_text()
        assert "new-tag" in content
        # Original tags preserved
        assert "python" in content

    def test_remove_tag(self, runner, posts_setup, mock_site_root):
        result = runner.invoke(main, [
            "posts", "tag", "2024-01-15-first-post", "--remove", "python",
        ])
        assert result.exit_code == 0

        post_file = (
            mock_site_root / "content" / "post" / "2024-01-15-first-post" / "index.md"
        )
        content = post_file.read_text()
        assert "python" not in content
        # Other tags preserved
        assert "testing" in content

    def test_set_tags(self, runner, posts_setup, mock_site_root):
        result = runner.invoke(main, [
            "posts", "tag", "2024-01-15-first-post", "--set", "a,b,c",
        ])
        assert result.exit_code == 0

        post_file = (
            mock_site_root / "content" / "post" / "2024-01-15-first-post" / "index.md"
        )
        content = post_file.read_text()
        assert "a" in content
        assert "python" not in content

    def test_add_duplicate_tag(self, runner, posts_setup):
        result = runner.invoke(main, [
            "posts", "tag", "2024-01-15-first-post", "--add", "python",
        ])
        assert result.exit_code == 0
        assert "already" in result.output.lower() or "no change" in result.output.lower() or result.exit_code == 0
```

**Step 2: Run tests to verify they fail**

Run: `cd /home/spinoza/github/repos/metafunctor/scripts/mf && python -m pytest tests/test_posts/test_commands.py::TestPostsTag -v`
Expected: FAIL

**Step 3: Implement tag command**

Add to `scripts/mf/src/mf/posts/commands.py`:

```python
@posts.command(name="tag")
@click.argument("slug")
@click.option("--add", multiple=True, help="Add tag(s)")
@click.option("--remove", multiple=True, help="Remove tag(s)")
@click.option("--set", "set_tags", help="Replace all tags (comma-separated)")
@click.pass_obj
def tag_post(ctx, slug: str, add: tuple[str, ...], remove: tuple[str, ...], set_tags: str | None) -> None:
    """Manage tags on a post.

    Examples:
        mf posts tag my-post --add python --add ml
        mf posts tag my-post --remove old-tag
        mf posts tag my-post --set "python,ml,statistics"
    """
    from mf.content.frontmatter import FrontMatterEditor

    post_file = _find_post_file(slug)
    if not post_file:
        console.print(f"[red]Post not found: {slug}[/red]")
        raise SystemExit(1)

    editor = FrontMatterEditor(post_file)
    if not editor.load():
        raise SystemExit(1)

    changed = False

    if set_tags is not None:
        new_tags = [t.strip() for t in set_tags.split(",") if t.strip()]
        editor.set("tags", new_tags)
        changed = True
    else:
        for t in add:
            if editor.add_to_list("tags", t):
                changed = True
            else:
                console.print(f"[dim]Tag '{t}' already present[/dim]")

        for t in remove:
            if editor.remove_from_list("tags", t):
                changed = True
            else:
                console.print(f"[dim]Tag '{t}' not found[/dim]")

    if not changed:
        console.print("[dim]No changes made[/dim]")
        return

    dry_run = getattr(ctx, "dry_run", False) if ctx else False
    if editor.save(dry_run=dry_run):
        tags = editor.front_matter.get("tags", [])
        console.print(f"[green]Updated tags[/green] on {slug}: {', '.join(tags)}")
```

**Step 4: Run tests**

Run: `cd /home/spinoza/github/repos/metafunctor/scripts/mf && python -m pytest tests/test_posts/test_commands.py::TestPostsTag -v`
Expected: All PASS

**Step 5: Commit**

```bash
git add scripts/mf/src/mf/posts/commands.py scripts/mf/tests/test_posts/test_commands.py
git commit -m "feat(mf): add mf posts tag command

Add, remove, or replace tags on posts via FrontMatterEditor.
Supports --add (repeatable), --remove (repeatable), --set (comma-separated)."
```

---

### Task 7: Implement `mf posts feature`

**Files:**
- Modify: `scripts/mf/src/mf/posts/commands.py`
- Modify: `scripts/mf/tests/test_posts/test_commands.py`

**Step 1: Write failing tests**

Append to `scripts/mf/tests/test_posts/test_commands.py`:

```python
class TestPostsFeature:
    """Tests for mf posts feature."""

    def test_feature_post(self, runner, posts_setup, mock_site_root):
        result = runner.invoke(main, [
            "posts", "feature", "2024-02-20-second-post",
        ])
        assert result.exit_code == 0

        post_file = (
            mock_site_root / "content" / "post" / "2024-02-20-second-post" / "index.md"
        )
        content = post_file.read_text()
        assert "featured: true" in content or "featured: True" in content

    def test_unfeature_post(self, runner, posts_setup, mock_site_root):
        result = runner.invoke(main, [
            "posts", "feature", "2024-01-15-first-post", "--off",
        ])
        assert result.exit_code == 0

        post_file = (
            mock_site_root / "content" / "post" / "2024-01-15-first-post" / "index.md"
        )
        content = post_file.read_text()
        assert "featured: true" not in content.lower()
```

**Step 2: Run tests to verify they fail**

Run: `cd /home/spinoza/github/repos/metafunctor/scripts/mf && python -m pytest tests/test_posts/test_commands.py::TestPostsFeature -v`
Expected: FAIL

**Step 3: Implement feature command**

Add to `scripts/mf/src/mf/posts/commands.py`:

```python
@posts.command(name="feature")
@click.argument("slug")
@click.option("--off", is_flag=True, help="Remove featured status")
@click.pass_obj
def feature_post(ctx, slug: str, off: bool) -> None:
    """Toggle featured status on a post.

    Examples:
        mf posts feature my-post       # Mark as featured
        mf posts feature my-post --off # Remove featured
    """
    from mf.content.frontmatter import FrontMatterEditor

    post_file = _find_post_file(slug)
    if not post_file:
        console.print(f"[red]Post not found: {slug}[/red]")
        raise SystemExit(1)

    editor = FrontMatterEditor(post_file)
    if not editor.load():
        raise SystemExit(1)

    if off:
        if "featured" in editor.front_matter:
            del editor.front_matter["featured"]
        else:
            console.print(f"[dim]{slug} is not featured[/dim]")
            return
    else:
        editor.set("featured", True)

    dry_run = getattr(ctx, "dry_run", False) if ctx else False
    if editor.save(dry_run=dry_run):
        status = "unfeatured" if off else "featured"
        console.print(f"[green]{slug}[/green] is now {status}")
```

**Step 4: Run tests**

Run: `cd /home/spinoza/github/repos/metafunctor/scripts/mf && python -m pytest tests/test_posts/test_commands.py::TestPostsFeature -v`
Expected: All PASS

**Step 5: Commit**

```bash
git add scripts/mf/src/mf/posts/commands.py scripts/mf/tests/test_posts/test_commands.py
git commit -m "feat(mf): add mf posts feature command

Toggle featured status on posts. Use --off to unfeature."
```

---

### Task 8: Update Global Plugin Skill with New Commands

**Files:**
- Modify: `~/.claude/plugins/repos/mf-plugin/skills/mf/SKILL.md`
- Modify: `~/.claude/plugins/repos/mf-plugin/skills/mf/COMMANDS.md`
- Modify: `~/.claude/plugins/repos/mf-plugin/skills/mf/WORKFLOWS.md`

**Step 1: Add posts section to SKILL.md**

In the "mf CLI Reference" section, add after the Publications subsection:

```markdown
### Posts
```bash
mf posts list                          # List all posts
mf posts list --tag X --series Y       # Filter by tag/series
mf posts list --since 30d --featured   # Recent featured posts
mf posts create --title "..." [opts]   # Scaffold new post
mf posts set <slug> <field> <value>    # Edit front matter in-place
mf posts unset <slug> <field>          # Remove front matter field
mf posts tag <slug> --add <tag>        # Manage tags (--add/--remove/--set)
mf posts feature <slug>               # Toggle featured (--off)
```
```

**Step 2: Add cross-repo usage section**

Update the "Cross-Repo Usage" section to mention global config:

```markdown
## Cross-Repo Usage

The mf CLI resolves the site root in this order:
1. `MF_SITE_ROOT` environment variable
2. Walk up from cwd looking for `.mf/` directory
3. Global config at `~/.config/mf/config.yaml`

```bash
# One-time setup (from any directory)
mkdir -p ~/.config/mf
echo "site_root: /home/spinoza/github/repos/metafunctor" > ~/.config/mf/config.yaml

# Now all commands work from any directory
cd ~/github/repos/likelihood.model
mf posts create --title "New release"     # Creates in metafunctor/content/post/
mf posts list --tag python                # Lists metafunctor posts
```
```

**Step 3: Add posts workflow to WORKFLOWS.md**

Add a new "Post Workflows" section:

```markdown
## Post Workflows

### Creating a New Post

```bash
# 1. Create the post scaffold
mf posts create --title "My New Post" --tag python --category Programming

# 2. Edit the content
# (open the file in your editor)

# 3. Remove draft flag when ready
mf posts set YYYY-MM-DD-my-new-post draft false

# 4. Deploy
make deploy

# 5. Cross-post (see /crier skill)
crier publish content/post/YYYY-MM-DD-my-new-post/index.md
```

### Finding and Filtering Posts

```bash
# Recent posts
mf posts list --since 30d

# Posts in a series
mf posts list --series stepanov

# Featured posts
mf posts list --featured

# Search by keyword
mf posts list -q "bloom filter"

# JSON output for scripting
mf posts list --json
```

### Managing Post Metadata

```bash
# Set description for social cards
mf posts set my-post description "A short summary"

# Add to a series
mf posts set my-post series '["stepanov"]'

# Add tags
mf posts tag my-post --add python --add algorithms

# Feature a post
mf posts feature my-post
```
```

**Step 4: Add to COMMANDS.md**

Add a "Posts" section in the commands reference.

**Step 5: Commit**

```bash
cd ~/.claude/plugins/repos/mf-plugin
git add skills/
git commit -m "docs: update mf plugin skills with posts commands and global config"
```

---

### Task 9: Set Up Global Config for This Machine

**Files:**
- Create: `~/.config/mf/config.yaml`

**Step 1: Create the global config**

```bash
mkdir -p ~/.config/mf
cat > ~/.config/mf/config.yaml << 'EOF'
# Global mf configuration
# This allows mf to work from any directory
site_root: /home/spinoza/github/repos/metafunctor
EOF
```

**Step 2: Verify mf works from another directory**

```bash
cd /tmp
mf posts list 2>&1 | head -5
```

Expected: Lists posts from metafunctor (not "Could not find .mf/")

**Step 3: No commit needed** (user machine config, not repo)

---

### Task 10: Final Integration Test + Full Suite

**Step 1: Run full test suite**

```bash
cd /home/spinoza/github/repos/metafunctor/scripts/mf
python -m pytest --tb=short -v
```

Expected: All tests pass.

**Step 2: Run coverage check**

```bash
cd /home/spinoza/github/repos/metafunctor/scripts/mf
python -m pytest --cov=mf.posts --cov=mf.core.config --cov-report=term-missing
```

Expected: Good coverage (>80%) on posts commands and config module.

**Step 3: Manual smoke test**

```bash
# From metafunctor repo
cd /home/spinoza/github/repos/metafunctor

# List posts
mf posts list --since 30d

# Create a test post (will delete after)
mf posts create --title "Test Post Please Ignore" --slug test-ignore --tag test
ls content/post/*test-ignore*/
cat content/post/*test-ignore*/index.md

# Clean up
rm -rf content/post/*test-ignore*

# From another directory (tests global config)
cd /tmp
mf posts list | head -5
```

**Step 4: Verify Hugo builds**

```bash
cd /home/spinoza/github/repos/metafunctor
hugo --gc --minify 2>&1 | tail -5
```

Expected: No errors related to posts or config changes.
