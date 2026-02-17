# Phase 4: Crier CLI Improvements — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add `--project` flag, section tracking, and `crier summary` command to the crier CLI, then update the plugin skill.

**Architecture:** Extend crier's existing `base_path` pattern (already threaded through config and registry) with a CLI-level `--project` option. Add a lightweight `section` field to registry entries inferred from file paths. Add a `summary` command that reads the registry without API calls.

**Tech Stack:** Python 3.12+, Click CLI, PyYAML, pytest with Click CliRunner

**Working directory:** `~/github/repos/crier/`

**Design doc:** `~/github/repos/metafunctor/docs/plans/2026-02-13-global-mf-plugin-design.md`

**Item #1 (auto-derive canonical_url):** Already implemented — `infer_canonical_url()` exists at `src/crier/config.py:374` and is called in `converters/markdown.py:214`.

---

## Context for Implementers

### Crier Codebase Layout

```
src/crier/
  cli.py          # Click CLI (5130 lines), @click.group() at line 86
  config.py       # Two-tier config, get_local_config_path() walks up from cwd
  registry.py     # Registry v2 (YAML), keyed by canonical_url, atomic writes
  converters/
    markdown.py   # Front matter parsing, infer_canonical_url() usage
  platforms/      # 13 platform integrations
```

### Key Patterns

1. **base_path flow:** `get_local_config_path(base_path)` and `get_registry_path(base_path)` already accept an optional `Path` that overrides cwd-based discovery. Currently always `None` from CLI.

2. **Click context:** The `cli()` group at `cli.py:86` is a bare `@click.group()`. Individual commands import and call registry/config functions directly.

3. **Registry entry structure:**
```yaml
version: 2
articles:
  "https://example.com/post/slug/":
    title: "Article Title"
    source_file: "content/post/slug/index.md"
    content_hash: "sha256:abc123"
    platforms:
      devto:
        id: "12345"
        url: "https://dev.to/user/article"
        published_at: "2026-01-01T00:00:00+00:00"
        updated_at: "2026-01-01T00:00:00+00:00"
        content_hash: "sha256:abc123"
```

4. **Test fixtures:** `conftest.py` provides `tmp_registry` (creates `.crier/registry.yaml` in tmp_path, chdir there) and `mock_config_and_registry` for CLI tests. Tests use `CliRunner` for Click testing.

---

## Task 1: Add `--project` option to CLI group

Adds a group-level `--project` Click option that overrides `.crier/` directory discovery. All subcommands access it via `click.get_current_context().obj`.

**Files:**
- Modify: `src/crier/cli.py:86-90` (cli group definition)
- Test: `tests/test_cli.py`

**Step 1: Write the failing test**

In `tests/test_cli.py`, add a test class for the `--project` option:

```python
class TestProjectOption:
    """Tests for --project global option."""

    def test_project_option_accepted(self, runner):
        """CLI accepts --project before subcommand."""
        result = runner.invoke(cli, ["--project", "/tmp/fake", "--version"])
        assert result.exit_code == 0

    def test_project_option_sets_context(self, runner, tmp_path):
        """--project value is accessible via Click context."""
        # Create a .crier directory in the tmp_path
        crier_dir = tmp_path / ".crier"
        crier_dir.mkdir()
        (crier_dir / "registry.yaml").write_text("version: 2\narticles: {}\n")

        result = runner.invoke(cli, ["--project", str(tmp_path), "summary"])
        # summary command should work using the specified project path
        assert result.exit_code == 0
```

**Step 2: Run test to verify it fails**

Run: `pytest tests/test_cli.py::TestProjectOption -v`
Expected: FAIL — `--project` option doesn't exist yet, `summary` command doesn't exist yet.

**Step 3: Implement the --project option**

In `src/crier/cli.py`, modify the `cli()` group:

```python
@click.group()
@click.version_option(version=__version__)
@click.option(
    "--project",
    type=click.Path(exists=True, file_okay=False, resolve_path=True),
    default=None,
    help="Project directory containing .crier/. Overrides directory discovery.",
)
@click.pass_context
def cli(ctx, project):
    """Crier - Cross-post your content everywhere."""
    ctx.ensure_object(dict)
    ctx.obj["project_path"] = Path(project) if project else None
```

**Step 4: Create a helper to extract project_path from context**

Add a utility function near the top of `cli.py`:

```python
def get_project_path() -> Path | None:
    """Get the --project path from Click context, if set."""
    ctx = click.get_current_context(silent=True)
    if ctx and ctx.obj:
        return ctx.obj.get("project_path")
    return None
```

**Step 5: Run tests to verify they pass**

Run: `pytest tests/test_cli.py::TestProjectOption::test_project_option_accepted -v`
Expected: PASS (the second test still fails — `summary` doesn't exist yet, that's Task 4)

**Step 6: Commit**

```bash
git add src/crier/cli.py tests/test_cli.py
git commit -m "feat: add --project global option to CLI group

Allows specifying the project directory containing .crier/ config,
overriding the default upward directory search. Value stored in
Click context for subcommand access."
```

---

## Task 2: Wire `--project` through to config and registry functions

Thread the `project_path` from Click context into `load_config()` and registry functions. Update key CLI commands to use it.

**Files:**
- Modify: `src/crier/config.py:48-76` (load_config)
- Modify: `src/crier/cli.py` (key commands: audit, status, publish, check, stats)
- Test: `tests/test_config.py`, `tests/test_cli.py`

**Step 1: Write the failing tests**

In `tests/test_config.py`:

```python
class TestLoadConfigWithBasePath:
    """Tests for load_config with explicit base_path."""

    def test_load_config_uses_base_path(self, tmp_path, monkeypatch):
        """load_config(base_path=X) uses X/.crier/config.yaml."""
        config_dir = tmp_path / ".crier"
        config_dir.mkdir()
        local_config = config_dir / "config.yaml"
        local_config.write_text(yaml.dump({"content_paths": ["my-content"]}))

        # Don't chdir — the point is base_path overrides cwd
        monkeypatch.chdir("/tmp")

        from crier.config import load_config
        config = load_config(base_path=tmp_path)
        assert config.get("content_paths") == ["my-content"]
```

**Step 2: Run test to verify it fails**

Run: `pytest tests/test_config.py::TestLoadConfigWithBasePath -v`
Expected: FAIL — `load_config()` doesn't accept `base_path` yet.

**Step 3: Add base_path parameter to load_config**

In `src/crier/config.py`, modify `load_config()`:

```python
def load_config(base_path: Path | None = None) -> dict[str, Any]:
    """Load configuration, merging local and global configs.

    Args:
        base_path: Override directory for local config discovery.
                   If None, uses cwd (existing behavior).
    """
    config: dict[str, Any] = {}

    # Load global config first (for API keys)
    global_path = get_config_path()
    if global_path.exists():
        with open(global_path) as f:
            config = yaml.safe_load(f) or {}

    # Merge local config (for content_paths, profiles)
    local_path = get_local_config_path(base_path)
    if local_path.exists():
        with open(local_path) as f:
            local_config = yaml.safe_load(f) or {}
            if "content_paths" in local_config:
                config["content_paths"] = local_config["content_paths"]
            if "profiles" in local_config:
                config.setdefault("profiles", {})
                config["profiles"].update(local_config["profiles"])

    return config
```

**Step 4: Update key CLI commands to pass project_path**

For each major command in `cli.py` that calls `load_config()`, `load_registry()`, or `get_registry_path()`, add `base_path=get_project_path()`. The commands to update:

- `audit` — calls `load_config()`, `load_registry()`, `record_publication()` etc.
- `publish` — calls `load_config()`, `record_publication()`
- `status` — calls `load_registry()`
- `check` — calls `load_config()`, `load_registry()`
- `stats` — calls `load_registry()`

For each, find calls like `load_config()` and change to `load_config(base_path=get_project_path())`, and similarly for registry functions.

Search for: `load_config()` → `load_config(base_path=get_project_path())`
Search for: `load_registry()` → `load_registry(base_path=get_project_path())`
Search for: `save_registry(` → ensure base_path is passed
Search for: `record_publication(` → ensure base_path kwarg is passed
Search for: `get_registry_path()` → `get_registry_path(base_path=get_project_path())`

**Note:** These are spread across the 5130-line cli.py. Use `grep` to find all call sites. Most already accept `base_path=` as keyword — just need to pass it.

**Step 5: Write integration test**

In `tests/test_cli.py`:

```python
def test_project_flag_uses_correct_registry(self, runner, tmp_path):
    """--project makes commands use the specified project's registry."""
    # Set up a project with a populated registry
    project_dir = tmp_path / "my-project"
    project_dir.mkdir()
    crier_dir = project_dir / ".crier"
    crier_dir.mkdir()

    registry = {
        "version": 2,
        "articles": {
            "https://example.com/post/test/": {
                "title": "Test Article",
                "source_file": "content/post/test/index.md",
                "content_hash": "sha256:abc123",
                "platforms": {
                    "devto": {
                        "id": "123",
                        "url": "https://dev.to/user/test",
                        "published_at": "2026-01-01T00:00:00+00:00",
                        "updated_at": "2026-01-01T00:00:00+00:00",
                        "content_hash": "sha256:abc123",
                    }
                },
            }
        },
    }
    (crier_dir / "registry.yaml").write_text(yaml.dump(registry))

    # Run summary from a DIFFERENT directory, but pointing at project
    result = runner.invoke(cli, ["--project", str(project_dir), "summary"])
    assert result.exit_code == 0
    assert "1" in result.output  # Should see 1 article
```

**Step 6: Run tests**

Run: `pytest tests/test_config.py::TestLoadConfigWithBasePath tests/test_cli.py::TestProjectOption -v`
Expected: PASS

**Step 7: Commit**

```bash
git add src/crier/config.py src/crier/cli.py tests/test_config.py tests/test_cli.py
git commit -m "feat: wire --project option through config and registry

load_config() now accepts base_path parameter. Key CLI commands
(audit, publish, status, check, stats) pass --project path to
config and registry functions."
```

---

## Task 3: Add section tracking to registry

Add a `section` field to registry article entries, inferred from the source file path (e.g., `content/post/slug/index.md` → `"post"`).

**Files:**
- Modify: `src/crier/registry.py:150-218` (record_publication)
- Create helper: `src/crier/registry.py` (infer_section function)
- Test: `tests/test_registry.py`

**Step 1: Write the failing tests**

In `tests/test_registry.py`:

```python
class TestInferSection:
    """Tests for infer_section()."""

    def test_infer_section_from_post_path(self):
        from crier.registry import infer_section
        assert infer_section("content/post/2026-01-01-slug/index.md") == "post"

    def test_infer_section_from_papers_path(self):
        from crier.registry import infer_section
        assert infer_section("content/papers/my-paper/index.md") == "papers"

    def test_infer_section_from_projects_path(self):
        from crier.registry import infer_section
        assert infer_section("content/projects/my-project/index.md") == "projects"

    def test_infer_section_no_content_prefix(self):
        from crier.registry import infer_section
        assert infer_section("posts/my-post.md") == "posts"

    def test_infer_section_none_for_empty(self):
        from crier.registry import infer_section
        assert infer_section(None) is None

    def test_infer_section_none_for_bare_file(self):
        from crier.registry import infer_section
        assert infer_section("index.md") is None


class TestRecordPublicationSection:
    """Tests for section field in record_publication()."""

    def test_section_recorded_for_new_article(self, tmp_registry):
        record_publication(
            canonical_url="https://example.com/post/test/",
            platform="devto",
            article_id="123",
            url="https://dev.to/user/test",
            title="Test",
            source_file="content/post/test/index.md",
        )
        registry = load_registry()
        article = registry["articles"]["https://example.com/post/test/"]
        assert article["section"] == "post"

    def test_section_not_overwritten_on_update(self, tmp_registry):
        """Section should be set on first record, not changed later."""
        record_publication(
            canonical_url="https://example.com/post/test/",
            platform="devto",
            article_id="123",
            url="https://dev.to/user/test",
            source_file="content/post/test/index.md",
        )
        # Second publication to different platform — section stays
        record_publication(
            canonical_url="https://example.com/post/test/",
            platform="hashnode",
            article_id="456",
            url="https://hashnode.dev/test",
            source_file="content/post/test/index.md",
        )
        registry = load_registry()
        article = registry["articles"]["https://example.com/post/test/"]
        assert article["section"] == "post"

    def test_existing_articles_without_section_unaffected(self, tmp_registry):
        """Backward compat: existing entries without section work fine."""
        registry = load_registry()
        registry["articles"]["https://example.com/old/"] = {
            "title": "Old Article",
            "source_file": None,
            "content_hash": None,
            "platforms": {},
        }
        save_registry(registry)

        loaded = load_registry()
        assert "section" not in loaded["articles"]["https://example.com/old/"]
```

**Step 2: Run tests to verify they fail**

Run: `pytest tests/test_registry.py::TestInferSection tests/test_registry.py::TestRecordPublicationSection -v`
Expected: FAIL — `infer_section` doesn't exist, `section` not recorded.

**Step 3: Implement infer_section**

In `src/crier/registry.py`, add before `record_publication()`:

```python
def infer_section(source_file: str | Path | None) -> str | None:
    """Infer content section from source file path.

    Extracts the first directory component after 'content/' if present,
    or the first directory component otherwise.

    Examples:
        "content/post/2026-01-slug/index.md" -> "post"
        "content/papers/my-paper/index.md" -> "papers"
        "posts/my-post.md" -> "posts"
        "index.md" -> None
        None -> None
    """
    if source_file is None:
        return None

    parts = Path(source_file).parts

    # Find "content" directory and take next part
    try:
        content_idx = list(parts).index("content")
        if content_idx + 1 < len(parts) - 1:  # Must have section + filename
            return parts[content_idx + 1]
    except ValueError:
        pass

    # No "content/" prefix — use first directory if there is one
    if len(parts) > 1:
        return parts[0]

    return None
```

**Step 4: Add section to record_publication**

In `src/crier/registry.py`, modify `record_publication()`:

After the `registry["articles"][canonical_url] = { ... }` block (line ~182-187), add:

```python
    # Infer and set section for new articles
    section = infer_section(source_file)
    if section:
        registry["articles"][canonical_url]["section"] = section
```

The existing article update path (lines 189-197) should NOT overwrite section — only set it on new entries.

**Step 5: Export infer_section**

Ensure `infer_section` is importable from the test file. Add to `__init__.py` if needed, or just import directly from `crier.registry`.

**Step 6: Run tests**

Run: `pytest tests/test_registry.py::TestInferSection tests/test_registry.py::TestRecordPublicationSection -v`
Expected: PASS

**Step 7: Commit**

```bash
git add src/crier/registry.py tests/test_registry.py
git commit -m "feat: add section tracking to registry entries

New articles get a 'section' field inferred from source_file path
(e.g., content/post/slug/index.md -> 'post'). Backward compatible:
existing entries without section continue to work."
```

---

## Task 4: Add `crier summary` command

A read-only command that shows registry statistics without making API calls.

**Files:**
- Modify: `src/crier/cli.py` (add `summary` command)
- Test: `tests/test_cli.py`

**Step 1: Write the failing tests**

In `tests/test_cli.py`:

```python
class TestSummaryCommand:
    """Tests for crier summary command."""

    def test_summary_empty_registry(self, runner, mock_config_and_registry):
        result = runner.invoke(cli, ["summary"])
        assert result.exit_code == 0
        assert "0" in result.output  # 0 articles

    def test_summary_with_articles(self, runner, tmp_path, monkeypatch):
        """Summary shows article and platform counts."""
        crier_dir = tmp_path / ".crier"
        crier_dir.mkdir()
        registry = {
            "version": 2,
            "articles": {
                "https://example.com/post/a/": {
                    "title": "Article A",
                    "source_file": "content/post/a/index.md",
                    "section": "post",
                    "content_hash": "sha256:aaa",
                    "platforms": {
                        "devto": {"id": "1", "url": "https://dev.to/a", "published_at": "2026-01-01T00:00:00+00:00", "updated_at": "2026-01-01T00:00:00+00:00", "content_hash": "sha256:aaa"},
                        "hashnode": {"id": "2", "url": "https://hashnode.dev/a", "published_at": "2026-01-01T00:00:00+00:00", "updated_at": "2026-01-01T00:00:00+00:00", "content_hash": "sha256:aaa"},
                    },
                },
                "https://example.com/papers/b/": {
                    "title": "Paper B",
                    "source_file": "content/papers/b/index.md",
                    "section": "papers",
                    "content_hash": "sha256:bbb",
                    "platforms": {
                        "devto": {"id": "3", "url": "https://dev.to/b", "published_at": "2026-01-01T00:00:00+00:00", "updated_at": "2026-01-01T00:00:00+00:00", "content_hash": "sha256:bbb"},
                    },
                },
            },
        }
        (crier_dir / "registry.yaml").write_text(yaml.dump(registry))
        monkeypatch.chdir(tmp_path)

        # Patch config path
        config_dir = tmp_path / "config"
        config_dir.mkdir()
        (config_dir / "config.yaml").write_text("")
        monkeypatch.setattr("crier.config.DEFAULT_CONFIG_FILE", config_dir / "config.yaml")
        monkeypatch.setattr("crier.config.DEFAULT_CONFIG_DIR", config_dir)

        result = runner.invoke(cli, ["summary"])
        assert result.exit_code == 0
        assert "2" in result.output  # 2 articles

    def test_summary_json_output(self, runner, mock_config_and_registry):
        result = runner.invoke(cli, ["summary", "--json"])
        assert result.exit_code == 0
        import json
        data = json.loads(result.output)
        assert "total_articles" in data

    def test_summary_respects_project_flag(self, runner, tmp_path, monkeypatch):
        """--project makes summary use the specified project's registry."""
        project_dir = tmp_path / "my-project"
        project_dir.mkdir()
        crier_dir = project_dir / ".crier"
        crier_dir.mkdir()
        registry = {
            "version": 2,
            "articles": {
                "https://example.com/test/": {
                    "title": "Test",
                    "source_file": "content/post/test/index.md",
                    "section": "post",
                    "content_hash": None,
                    "platforms": {},
                },
            },
        }
        (crier_dir / "registry.yaml").write_text(yaml.dump(registry))

        # Patch config
        config_dir = tmp_path / "config"
        config_dir.mkdir()
        (config_dir / "config.yaml").write_text("")
        monkeypatch.setattr("crier.config.DEFAULT_CONFIG_FILE", config_dir / "config.yaml")
        monkeypatch.setattr("crier.config.DEFAULT_CONFIG_DIR", config_dir)

        # Run from different dir, pointing at project
        monkeypatch.chdir(tmp_path)
        result = runner.invoke(cli, ["--project", str(project_dir), "summary"])
        assert result.exit_code == 0
        assert "1" in result.output
```

**Step 2: Run tests to verify they fail**

Run: `pytest tests/test_cli.py::TestSummaryCommand -v`
Expected: FAIL — `summary` command doesn't exist.

**Step 3: Implement the summary command**

In `src/crier/cli.py`, add the `summary` command (insert after the `stats` command or near end of file):

```python
@cli.command()
@click.option("--json", "json_output", is_flag=True, help="Output as JSON.")
def summary(json_output):
    """Show registry summary statistics. No API calls.

    Displays article counts by section and platform, and identifies
    articles with no platform publications.
    """
    import json as json_mod

    base_path = get_project_path()
    registry = load_registry(base_path)
    articles = registry.get("articles", {})

    total = len(articles)
    by_section: dict[str, int] = {}
    by_platform: dict[str, int] = {}
    unposted = []

    for canonical_url, article in articles.items():
        # Count by section
        section = article.get("section", "unknown")
        by_section[section] = by_section.get(section, 0) + 1

        # Count by platform
        platforms = article.get("platforms", {})
        if not platforms:
            unposted.append(canonical_url)
        for platform_name in platforms:
            by_platform[platform_name] = by_platform.get(platform_name, 0) + 1

    if json_output:
        data = {
            "total_articles": total,
            "by_section": by_section,
            "by_platform": by_platform,
            "unposted_count": len(unposted),
            "unposted": unposted,
        }
        console.print(json_mod.dumps(data, indent=2))
        return

    # Rich table output
    console.print(f"\n[bold]Registry Summary[/bold]\n")
    console.print(f"Total articles: [bold]{total}[/bold]")
    console.print(f"Unposted: [bold]{len(unposted)}[/bold]\n")

    if by_section:
        from rich.table import Table

        section_table = Table(title="Articles by Section")
        section_table.add_column("Section", style="cyan")
        section_table.add_column("Count", justify="right")
        for section, count in sorted(by_section.items(), key=lambda x: -x[1]):
            section_table.add_row(section, str(count))
        console.print(section_table)
        console.print()

    if by_platform:
        platform_table = Table(title="Publications by Platform")
        platform_table.add_column("Platform", style="green")
        platform_table.add_column("Count", justify="right")
        for platform, count in sorted(by_platform.items(), key=lambda x: -x[1]):
            platform_table.add_row(platform, str(count))
        console.print(platform_table)
```

**Step 4: Run tests**

Run: `pytest tests/test_cli.py::TestSummaryCommand -v`
Expected: PASS

**Step 5: Commit**

```bash
git add src/crier/cli.py tests/test_cli.py
git commit -m "feat: add crier summary command

Registry-only stats command showing article counts by section
and platform, plus unposted content. No API calls.
Supports --json output and --project flag."
```

---

## Task 5: Update crier plugin skill

Update the crier skill in the mf-plugin with new capabilities.

**Files:**
- Modify: `~/.claude/plugins/repos/mf-plugin/skills/crier/SKILL.md`

**Step 1: Review current skill**

The current skill has a "Running from Other Repos" section (lines 318-332) with a "Future" note about `--project`. Also lacks `summary` command and section info.

**Step 2: Update the skill**

Changes to make:

1. **Replace "Future" workaround** in "Running from Other Repos" (lines 318-332):

Replace:
```markdown
## Running from Other Repos

Crier finds its `.crier/` config by walking up from the current directory (like git finds `.git/`). When running from another repo, crier won't find the metafunctor config.

**Workaround (current):**
```bash
cd ~/github/repos/metafunctor && crier audit
```

**Future:** A `--project` flag will allow:
```bash
crier --project ~/github/repos/metafunctor audit
```

Until then, always `cd` to the metafunctor repo for crier operations, or run crier commands with an explicit path prefix.
```

With:
```markdown
## Running from Other Repos

Crier finds its `.crier/` config by walking up from the current directory (like git finds `.git/`). Use `--project` to point at a different directory:

```bash
# From any directory
crier --project ~/github/repos/metafunctor audit
crier --project ~/github/repos/metafunctor summary

# All commands support --project
crier --project ~/github/repos/metafunctor publish content/post/slug/index.md --to devto
crier --project ~/github/repos/metafunctor status content/post/slug/index.md
```

`--project` overrides config and registry discovery — config, registry, and content paths all resolve relative to the specified directory.
```

2. **Add `crier summary` to Key Commands** section (after `crier doctor`):

```markdown
# Registry summary (no API calls)
crier summary
crier summary --json
```

3. **Add `crier summary` to "Checking Publication Status"** section (after `crier doctor`):

```markdown
# Site-wide registry overview (no API calls)
crier summary
```

4. **Add section tracking note** at the end of the "Configuration" > "Local Config" section:

```markdown
### Section Tracking

Crier automatically tracks the content section (e.g., `post`, `papers`, `projects`) for each registered article, inferred from the source file path. This powers the `crier summary` breakdown by section. No configuration needed.
```

**Step 3: Apply edits**

Apply the four edits listed above to `~/.claude/plugins/repos/mf-plugin/skills/crier/SKILL.md`.

**Step 4: Commit (in mf-plugin repo)**

```bash
cd ~/.claude/plugins/repos/mf-plugin
git add skills/crier/SKILL.md
git commit -m "Update crier skill with --project flag, summary command, section tracking"
```

---

## Task 6: Run full test suite and verify

**Step 1: Run all crier tests**

```bash
cd ~/github/repos/crier
pytest -v --tb=short
```

Expected: All 1088+ tests pass (existing + new).

**Step 2: Verify --project works end-to-end**

From the metafunctor directory:

```bash
cd ~/github/repos/metafunctor
crier --project . summary
```

Should show the registry summary for metafunctor's `.crier/registry.yaml`.

**Step 3: Verify from a different directory**

```bash
cd ~/github/repos/likelihood.model
crier --project ~/github/repos/metafunctor summary
```

Should show the same output.

**Step 4: Commit final state if any fixups needed**

If any tests needed fixes, commit them:

```bash
cd ~/github/repos/crier
git add -A
git commit -m "fix: test adjustments for --project and summary"
```
