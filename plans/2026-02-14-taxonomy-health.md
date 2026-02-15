# Phase 3: Taxonomy + Health Commands Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add `mf taxonomy` (audit/normalize/orphans/stats) and `mf health` (links/descriptions/images/stale/drafts) command groups to the mf CLI.

**Architecture:** Two new modules following the established pattern: a core analyzer/checker class (like `ContentAnalytics` in analytics/) plus a Click command group (like `analytics/commands.py`). Both scan Hugo content via `ContentScanner` and output via Rich tables + optional `--json`.

**Tech Stack:** Click, Rich, PyYAML, FrontMatterEditor, ContentScanner, difflib (for fuzzy matching)

---

### Task 1: TaxonomyAnalyzer Core

Create the analysis engine that powers all taxonomy commands.

**Files:**
- Create: `scripts/mf/src/mf/taxonomy/__init__.py`
- Create: `scripts/mf/src/mf/taxonomy/analyzer.py`
- Test: `scripts/mf/tests/test_taxonomy/__init__.py`
- Test: `scripts/mf/tests/test_taxonomy/test_analyzer.py`

**Step 1: Write failing tests for TaxonomyAnalyzer**

Create `scripts/mf/tests/test_taxonomy/__init__.py` (empty) and `test_analyzer.py`:

```python
"""Tests for taxonomy analyzer."""

from __future__ import annotations

import pytest
from pathlib import Path

from mf.taxonomy.analyzer import TaxonomyAnalyzer


class TestCollectTaxonomies:
    """Test taxonomy collection from content."""

    def test_collects_tags_from_posts(self, create_content_file):
        create_content_file(slug="post-a", extra_fm={"tags": ["python", "ml"]})
        create_content_file(slug="post-b", extra_fm={"tags": ["python", "rust"]})
        analyzer = TaxonomyAnalyzer()
        result = analyzer.collect()
        assert "python" in result.tag_counts
        assert result.tag_counts["python"] == 2
        assert result.tag_counts["ml"] == 1
        assert result.tag_counts["rust"] == 1

    def test_collects_categories(self, create_content_file):
        create_content_file(slug="post-a", extra_fm={"categories": ["AI"]})
        create_content_file(slug="post-b", extra_fm={"categories": ["AI", "Math"]})
        analyzer = TaxonomyAnalyzer()
        result = analyzer.collect()
        assert result.category_counts["AI"] == 2
        assert result.category_counts["Math"] == 1

    def test_tracks_which_content_uses_each_tag(self, create_content_file):
        create_content_file(slug="post-a", extra_fm={"tags": ["python"]})
        create_content_file(slug="post-b", extra_fm={"tags": ["python"]})
        analyzer = TaxonomyAnalyzer()
        result = analyzer.collect()
        assert len(result.tag_items["python"]) == 2

    def test_skips_drafts_by_default(self, create_content_file):
        create_content_file(slug="post-a", extra_fm={"tags": ["python"]})
        create_content_file(slug="draft-post", extra_fm={"tags": ["hidden"]}, draft=True)
        analyzer = TaxonomyAnalyzer()
        result = analyzer.collect()
        assert "hidden" not in result.tag_counts

    def test_includes_drafts_when_asked(self, create_content_file):
        create_content_file(slug="draft-post", extra_fm={"tags": ["hidden"]}, draft=True)
        analyzer = TaxonomyAnalyzer()
        result = analyzer.collect(include_drafts=True)
        assert "hidden" in result.tag_counts


class TestFindDuplicates:
    """Test near-duplicate detection."""

    def test_detects_case_mismatch(self, create_content_file):
        create_content_file(slug="post-a", extra_fm={"tags": ["Python"]})
        create_content_file(slug="post-b", extra_fm={"tags": ["python"]})
        analyzer = TaxonomyAnalyzer()
        result = analyzer.collect()
        dupes = analyzer.find_duplicates(result)
        # Should find case mismatch between "Python" and "python"
        assert len(dupes) >= 1
        pair = dupes[0]
        assert set(pair["terms"]) == {"Python", "python"}
        assert pair["reason"] == "case_mismatch"

    def test_detects_plural_mismatch(self, create_content_file):
        create_content_file(slug="post-a", extra_fm={"tags": ["algorithm"]})
        create_content_file(slug="post-b", extra_fm={"tags": ["algorithms"]})
        analyzer = TaxonomyAnalyzer()
        result = analyzer.collect()
        dupes = analyzer.find_duplicates(result)
        assert any(
            set(d["terms"]) == {"algorithm", "algorithms"}
            for d in dupes
        )

    def test_detects_hyphen_vs_space(self, create_content_file):
        create_content_file(slug="post-a", extra_fm={"tags": ["machine-learning"]})
        create_content_file(slug="post-b", extra_fm={"tags": ["machine learning"]})
        analyzer = TaxonomyAnalyzer()
        result = analyzer.collect()
        dupes = analyzer.find_duplicates(result)
        assert any(
            set(d["terms"]) == {"machine-learning", "machine learning"}
            for d in dupes
        )

    def test_no_false_positives_for_unrelated_tags(self, create_content_file):
        create_content_file(slug="post-a", extra_fm={"tags": ["python"]})
        create_content_file(slug="post-b", extra_fm={"tags": ["rust"]})
        analyzer = TaxonomyAnalyzer()
        result = analyzer.collect()
        dupes = analyzer.find_duplicates(result)
        assert len(dupes) == 0


class TestFindOrphans:
    """Test orphan detection (tags used by only 1 post)."""

    def test_finds_orphan_tags(self, create_content_file):
        create_content_file(slug="post-a", extra_fm={"tags": ["python", "rare-tag"]})
        create_content_file(slug="post-b", extra_fm={"tags": ["python"]})
        analyzer = TaxonomyAnalyzer()
        result = analyzer.collect()
        orphans = analyzer.find_orphans(result)
        assert "rare-tag" in orphans["tags"]
        assert "python" not in orphans["tags"]

    def test_finds_orphan_categories(self, create_content_file):
        create_content_file(slug="post-a", extra_fm={"categories": ["AI", "Rare"]})
        create_content_file(slug="post-b", extra_fm={"categories": ["AI"]})
        analyzer = TaxonomyAnalyzer()
        result = analyzer.collect()
        orphans = analyzer.find_orphans(result)
        assert "Rare" in orphans["categories"]

    def test_min_count_parameter(self, create_content_file):
        create_content_file(slug="post-a", extra_fm={"tags": ["a", "b"]})
        create_content_file(slug="post-b", extra_fm={"tags": ["a", "b"]})
        create_content_file(slug="post-c", extra_fm={"tags": ["a"]})
        analyzer = TaxonomyAnalyzer()
        result = analyzer.collect()
        orphans = analyzer.find_orphans(result, min_count=3)
        assert "b" in orphans["tags"]
        assert "a" not in orphans["tags"]


class TestStats:
    """Test taxonomy statistics."""

    def test_stats_returns_sorted_counts(self, create_content_file):
        create_content_file(slug="post-a", extra_fm={"tags": ["a", "b", "c"]})
        create_content_file(slug="post-b", extra_fm={"tags": ["a", "b"]})
        create_content_file(slug="post-c", extra_fm={"tags": ["a"]})
        analyzer = TaxonomyAnalyzer()
        result = analyzer.collect()
        stats = analyzer.get_stats(result)
        tag_names = [s["tag"] for s in stats["tags"]]
        assert tag_names == ["a", "b", "c"]

    def test_stats_includes_co_occurrence(self, create_content_file):
        create_content_file(slug="post-a", extra_fm={"tags": ["python", "ml"]})
        create_content_file(slug="post-b", extra_fm={"tags": ["python", "ml"]})
        analyzer = TaxonomyAnalyzer()
        result = analyzer.collect()
        stats = analyzer.get_stats(result)
        cooc = stats["co_occurrences"]
        # python+ml co-occur twice
        key = tuple(sorted(["python", "ml"]))
        assert cooc[key] == 2

    def test_stats_totals(self, create_content_file):
        create_content_file(slug="post-a", extra_fm={"tags": ["x", "y"]})
        analyzer = TaxonomyAnalyzer()
        result = analyzer.collect()
        stats = analyzer.get_stats(result)
        assert stats["total_unique_tags"] == 2
        assert stats["total_tag_usages"] == 2
```

**Step 2: Run tests to verify they fail**

Run: `cd ~/github/repos/metafunctor/scripts/mf && python -m pytest tests/test_taxonomy/ -v`
Expected: FAIL with `ModuleNotFoundError: No module named 'mf.taxonomy'`

**Step 3: Implement TaxonomyAnalyzer**

Create `scripts/mf/src/mf/taxonomy/__init__.py`:
```python
"""Taxonomy analysis and hygiene tools."""
```

Create `scripts/mf/src/mf/taxonomy/analyzer.py`:
```python
"""Taxonomy analyzer for Hugo content.

Collects, analyzes, and reports on tag/category usage across all content.
"""

from __future__ import annotations

from collections import defaultdict
from dataclasses import dataclass, field
from itertools import combinations
from pathlib import Path
from typing import Any

from mf.content.scanner import ContentScanner


@dataclass
class TaxonomyData:
    """Collected taxonomy data from content scan."""

    tag_counts: dict[str, int] = field(default_factory=lambda: defaultdict(int))
    category_counts: dict[str, int] = field(default_factory=lambda: defaultdict(int))
    tag_items: dict[str, list[str]] = field(
        default_factory=lambda: defaultdict(list)
    )
    category_items: dict[str, list[str]] = field(
        default_factory=lambda: defaultdict(list)
    )
    # Per-item tag lists for co-occurrence analysis
    item_tags: list[list[str]] = field(default_factory=list)


class TaxonomyAnalyzer:
    """Analyzes taxonomy usage across Hugo content."""

    def __init__(self, site_root: Path | None = None):
        self.scanner = ContentScanner(site_root)

    def collect(
        self,
        content_types: list[str] | None = None,
        include_drafts: bool = False,
    ) -> TaxonomyData:
        """Scan content and collect all taxonomy data.

        Args:
            content_types: Types to scan (default: all scanner types).
            include_drafts: Include draft content.

        Returns:
            TaxonomyData with counts and item mappings.
        """
        if content_types is None:
            content_types = list(self.scanner.CONTENT_TYPES.keys())

        data = TaxonomyData()

        for ct in content_types:
            items = self.scanner.scan_type(ct, include_drafts=include_drafts)
            for item in items:
                slug = item.slug

                for tag in item.tags:
                    data.tag_counts[tag] += 1
                    data.tag_items[tag].append(slug)

                for cat in item.categories:
                    data.category_counts[cat] += 1
                    data.category_items[cat].append(slug)

                if item.tags:
                    data.item_tags.append(item.tags)

        return data

    def find_duplicates(
        self,
        data: TaxonomyData,
        taxonomy: str = "tags",
    ) -> list[dict[str, Any]]:
        """Find near-duplicate taxonomy terms.

        Detects: case mismatches, plural/singular, hyphen vs space.

        Args:
            data: Collected taxonomy data.
            taxonomy: "tags" or "categories".

        Returns:
            List of dicts with keys: terms, reason, counts.
        """
        counts = data.tag_counts if taxonomy == "tags" else data.category_counts
        terms = list(counts.keys())
        duplicates: list[dict[str, Any]] = []
        seen: set[frozenset[str]] = set()

        for i, a in enumerate(terms):
            for b in terms[i + 1:]:
                pair = frozenset([a, b])
                if pair in seen:
                    continue

                reason = self._check_similarity(a, b)
                if reason:
                    seen.add(pair)
                    duplicates.append({
                        "terms": sorted([a, b]),
                        "reason": reason,
                        "counts": {a: counts[a], b: counts[b]},
                    })

        return duplicates

    def _check_similarity(self, a: str, b: str) -> str | None:
        """Check if two terms are near-duplicates.

        Returns reason string or None.
        """
        if a == b:
            return None

        # Case mismatch
        if a.lower() == b.lower():
            return "case_mismatch"

        # Hyphen vs space
        if a.replace("-", " ") == b.replace("-", " "):
            return "hyphen_space"

        # Plural (simple English: trailing s/es)
        a_low, b_low = a.lower(), b.lower()
        if a_low + "s" == b_low or b_low + "s" == a_low:
            return "plural"
        if a_low + "es" == b_low or b_low + "es" == a_low:
            return "plural"

        # Underscore vs hyphen
        if a.replace("_", "-") == b.replace("_", "-"):
            return "underscore_hyphen"

        return None

    def find_orphans(
        self,
        data: TaxonomyData,
        min_count: int = 2,
    ) -> dict[str, list[str]]:
        """Find taxonomy terms used fewer than min_count times.

        Args:
            data: Collected taxonomy data.
            min_count: Minimum usage count (default 2; terms with count < min_count are orphans).

        Returns:
            Dict with "tags" and "categories" lists of orphan terms.
        """
        return {
            "tags": sorted(
                t for t, c in data.tag_counts.items() if c < min_count
            ),
            "categories": sorted(
                c for c, cnt in data.category_counts.items() if cnt < min_count
            ),
        }

    def get_stats(
        self,
        data: TaxonomyData,
        limit: int = 0,
    ) -> dict[str, Any]:
        """Get taxonomy statistics including co-occurrence.

        Args:
            data: Collected taxonomy data.
            limit: Max tags to return in sorted list (0 = all).

        Returns:
            Dict with tags, categories, co_occurrences, totals.
        """
        # Sorted tag stats
        tag_stats = [
            {"tag": t, "count": c}
            for t, c in sorted(
                data.tag_counts.items(), key=lambda x: x[1], reverse=True
            )
        ]
        if limit:
            tag_stats = tag_stats[:limit]

        cat_stats = [
            {"category": c, "count": cnt}
            for c, cnt in sorted(
                data.category_counts.items(), key=lambda x: x[1], reverse=True
            )
        ]
        if limit:
            cat_stats = cat_stats[:limit]

        # Co-occurrence: count how often pairs of tags appear together
        cooc: dict[tuple[str, str], int] = defaultdict(int)
        for tags in data.item_tags:
            for a, b in combinations(sorted(tags), 2):
                cooc[(a, b)] += 1

        return {
            "tags": tag_stats,
            "categories": cat_stats,
            "co_occurrences": dict(cooc),
            "total_unique_tags": len(data.tag_counts),
            "total_unique_categories": len(data.category_counts),
            "total_tag_usages": sum(data.tag_counts.values()),
            "total_category_usages": sum(data.category_counts.values()),
        }
```

**Step 4: Run tests to verify they pass**

Run: `cd ~/github/repos/metafunctor/scripts/mf && python -m pytest tests/test_taxonomy/test_analyzer.py -v`
Expected: All 14 tests PASS

**Step 5: Commit**

```bash
cd ~/github/repos/metafunctor
git add scripts/mf/src/mf/taxonomy/ scripts/mf/tests/test_taxonomy/
git commit -m "feat(mf): add TaxonomyAnalyzer for tag/category analysis

Collects taxonomy data across all content types, detects near-duplicates
(case mismatch, plural/singular, hyphen/space), finds orphans, and
computes co-occurrence statistics.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>"
```

---

### Task 2: Taxonomy CLI Commands

Create Click commands that expose TaxonomyAnalyzer functionality.

**Files:**
- Create: `scripts/mf/src/mf/taxonomy/commands.py`
- Modify: `scripts/mf/src/mf/cli.py` (add 2 lines)
- Test: `scripts/mf/tests/test_taxonomy/test_commands.py`

**Step 1: Write failing tests for CLI commands**

Create `scripts/mf/tests/test_taxonomy/test_commands.py`:

```python
"""Tests for taxonomy CLI commands."""

from __future__ import annotations

import json

import pytest
from click.testing import CliRunner

from mf.taxonomy.commands import taxonomy


@pytest.fixture
def runner():
    return CliRunner()


class TestTaxonomyAudit:
    """Test mf taxonomy audit."""

    def test_detects_case_mismatch(self, runner, create_content_file):
        create_content_file(slug="post-a", extra_fm={"tags": ["Python"]})
        create_content_file(slug="post-b", extra_fm={"tags": ["python"]})
        result = runner.invoke(taxonomy, ["audit"])
        assert result.exit_code == 0
        assert "case_mismatch" in result.output or "Python" in result.output

    def test_audit_json_output(self, runner, create_content_file):
        create_content_file(slug="post-a", extra_fm={"tags": ["Python"]})
        create_content_file(slug="post-b", extra_fm={"tags": ["python"]})
        result = runner.invoke(taxonomy, ["audit", "--json"])
        assert result.exit_code == 0
        data = json.loads(result.output)
        assert isinstance(data, list)
        assert len(data) >= 1

    def test_audit_clean(self, runner, create_content_file):
        create_content_file(slug="post-a", extra_fm={"tags": ["python"]})
        create_content_file(slug="post-b", extra_fm={"tags": ["rust"]})
        result = runner.invoke(taxonomy, ["audit"])
        assert result.exit_code == 0
        assert "No near-duplicate" in result.output or "clean" in result.output.lower()


class TestTaxonomyOrphans:
    """Test mf taxonomy orphans."""

    def test_finds_orphan_tags(self, runner, create_content_file):
        create_content_file(slug="post-a", extra_fm={"tags": ["python", "rare"]})
        create_content_file(slug="post-b", extra_fm={"tags": ["python"]})
        result = runner.invoke(taxonomy, ["orphans"])
        assert result.exit_code == 0
        assert "rare" in result.output

    def test_orphans_json_output(self, runner, create_content_file):
        create_content_file(slug="post-a", extra_fm={"tags": ["python", "rare"]})
        create_content_file(slug="post-b", extra_fm={"tags": ["python"]})
        result = runner.invoke(taxonomy, ["orphans", "--json"])
        assert result.exit_code == 0
        data = json.loads(result.output)
        assert "tags" in data
        assert "rare" in data["tags"]

    def test_orphans_min_count(self, runner, create_content_file):
        create_content_file(slug="post-a", extra_fm={"tags": ["a", "b"]})
        create_content_file(slug="post-b", extra_fm={"tags": ["a", "b"]})
        create_content_file(slug="post-c", extra_fm={"tags": ["a"]})
        result = runner.invoke(taxonomy, ["orphans", "--min-count", "3"])
        assert result.exit_code == 0
        assert "b" in result.output


class TestTaxonomyStats:
    """Test mf taxonomy stats."""

    def test_shows_tag_frequency(self, runner, create_content_file):
        create_content_file(slug="post-a", extra_fm={"tags": ["python", "ml"]})
        create_content_file(slug="post-b", extra_fm={"tags": ["python"]})
        result = runner.invoke(taxonomy, ["stats"])
        assert result.exit_code == 0
        assert "python" in result.output

    def test_stats_json_output(self, runner, create_content_file):
        create_content_file(slug="post-a", extra_fm={"tags": ["python"]})
        result = runner.invoke(taxonomy, ["stats", "--json"])
        assert result.exit_code == 0
        data = json.loads(result.output)
        assert "total_unique_tags" in data

    def test_stats_limit(self, runner, create_content_file):
        for i in range(5):
            create_content_file(
                slug=f"post-{i}",
                extra_fm={"tags": [f"tag-{j}" for j in range(i + 1)]},
            )
        result = runner.invoke(taxonomy, ["stats", "--limit", "3"])
        assert result.exit_code == 0


class TestTaxonomyNormalize:
    """Test mf taxonomy normalize."""

    def test_normalize_dry_run(self, runner, create_content_file):
        create_content_file(slug="post-a", extra_fm={"tags": ["Python"]})
        create_content_file(slug="post-b", extra_fm={"tags": ["python"]})
        result = runner.invoke(taxonomy, ["normalize", "--dry-run"])
        assert result.exit_code == 0
        assert "Would" in result.output or "dry" in result.output.lower()

    def test_normalize_renames_to_target(self, runner, create_content_file, mock_site_root):
        f1 = create_content_file(slug="post-a", extra_fm={"tags": ["Python"]})
        f2 = create_content_file(slug="post-b", extra_fm={"tags": ["python"]})
        result = runner.invoke(
            taxonomy,
            ["normalize", "--from", "Python", "--to", "python", "--yes"],
        )
        assert result.exit_code == 0
        # Verify the file was updated
        content = f1.read_text()
        assert "python" in content

    def test_normalize_requires_from_to(self, runner, create_content_file):
        create_content_file(slug="post-a", extra_fm={"tags": ["Python"]})
        result = runner.invoke(taxonomy, ["normalize"])
        assert result.exit_code != 0 or "Required" in result.output or "Missing" in result.output
```

**Step 2: Run tests to verify they fail**

Run: `cd ~/github/repos/metafunctor/scripts/mf && python -m pytest tests/test_taxonomy/test_commands.py -v`
Expected: FAIL with `ImportError`

**Step 3: Implement taxonomy CLI commands**

Create `scripts/mf/src/mf/taxonomy/commands.py`:

```python
"""CLI commands for taxonomy hygiene.

Find near-duplicates, normalize terms, find orphans, and view stats.
"""

from __future__ import annotations

import json as json_module

import click
from rich.console import Console
from rich.table import Table

console = Console()


@click.group(name="taxonomy")
def taxonomy() -> None:
    """Taxonomy hygiene: audit, normalize, orphans, stats."""
    pass


@taxonomy.command(name="audit")
@click.option("--json", "as_json", is_flag=True, help="Output as JSON")
@click.option("--include-drafts", is_flag=True, help="Include drafts")
@click.option(
    "--taxonomy",
    "tax_type",
    type=click.Choice(["tags", "categories", "both"]),
    default="both",
    help="Which taxonomy to audit",
)
def audit_cmd(as_json: bool, include_drafts: bool, tax_type: str) -> None:
    """Find near-duplicate taxonomy terms (case mismatches, plurals, etc.)."""
    from mf.taxonomy.analyzer import TaxonomyAnalyzer

    analyzer = TaxonomyAnalyzer()
    data = analyzer.collect(include_drafts=include_drafts)

    all_dupes = []
    if tax_type in ("tags", "both"):
        all_dupes.extend(analyzer.find_duplicates(data, taxonomy="tags"))
    if tax_type in ("categories", "both"):
        all_dupes.extend(analyzer.find_duplicates(data, taxonomy="categories"))

    if as_json:
        click.echo(json_module.dumps(all_dupes, indent=2))
        return

    if not all_dupes:
        console.print("[green]No near-duplicate taxonomy terms found.[/green]")
        return

    table = Table(title=f"Near-Duplicate Terms ({len(all_dupes)})")
    table.add_column("Term A", style="cyan")
    table.add_column("Term B", style="cyan")
    table.add_column("Reason", style="yellow")
    table.add_column("Counts", style="dim")

    for d in all_dupes:
        a, b = d["terms"]
        counts_str = f"{a}: {d['counts'][a]}, {b}: {d['counts'][b]}"
        table.add_row(a, b, d["reason"], counts_str)

    console.print(table)
    console.print()
    console.print(
        "[dim]Use 'mf taxonomy normalize --from TERM --to TERM' to merge.[/dim]"
    )


@taxonomy.command(name="orphans")
@click.option("--json", "as_json", is_flag=True, help="Output as JSON")
@click.option("--include-drafts", is_flag=True, help="Include drafts")
@click.option(
    "--min-count",
    default=2,
    type=int,
    help="Tags used fewer than this are orphans (default: 2)",
)
def orphans_cmd(as_json: bool, include_drafts: bool, min_count: int) -> None:
    """Find taxonomy terms used by fewer than --min-count content items."""
    from mf.taxonomy.analyzer import TaxonomyAnalyzer

    analyzer = TaxonomyAnalyzer()
    data = analyzer.collect(include_drafts=include_drafts)
    orphans = analyzer.find_orphans(data, min_count=min_count)

    if as_json:
        click.echo(json_module.dumps(orphans, indent=2))
        return

    for tax_name in ("tags", "categories"):
        terms = orphans[tax_name]
        if not terms:
            console.print(f"[green]No orphan {tax_name} (min_count={min_count}).[/green]")
            continue

        table = Table(title=f"Orphan {tax_name.capitalize()} ({len(terms)})")
        table.add_column("Term", style="cyan")
        table.add_column("Count", justify="right")
        table.add_column("Used By", style="dim")

        counts = data.tag_counts if tax_name == "tags" else data.category_counts
        items = data.tag_items if tax_name == "tags" else data.category_items
        for term in terms:
            table.add_row(term, str(counts[term]), ", ".join(items[term][:3]))

        console.print(table)
        console.print()


@taxonomy.command(name="stats")
@click.option("--json", "as_json", is_flag=True, help="Output as JSON")
@click.option("--include-drafts", is_flag=True, help="Include drafts")
@click.option("--limit", default=20, type=int, help="Max terms to show (0=all)")
def stats_cmd(as_json: bool, include_drafts: bool, limit: int) -> None:
    """Show taxonomy frequency and co-occurrence statistics."""
    from mf.taxonomy.analyzer import TaxonomyAnalyzer

    analyzer = TaxonomyAnalyzer()
    data = analyzer.collect(include_drafts=include_drafts)
    stats = analyzer.get_stats(data, limit=limit)

    if as_json:
        # Convert tuple keys to strings for JSON
        cooc = {f"{a}+{b}": v for (a, b), v in stats["co_occurrences"].items()}
        stats["co_occurrences"] = cooc
        click.echo(json_module.dumps(stats, indent=2))
        return

    # Summary
    console.print(f"[bold]Unique tags:[/bold] {stats['total_unique_tags']}")
    console.print(f"[bold]Unique categories:[/bold] {stats['total_unique_categories']}")
    console.print(f"[bold]Total tag usages:[/bold] {stats['total_tag_usages']}")
    console.print()

    # Tag frequency table
    if stats["tags"]:
        table = Table(title="Tag Frequency")
        table.add_column("#", justify="right", style="dim")
        table.add_column("Tag", style="cyan")
        table.add_column("Count", justify="right")

        for i, t in enumerate(stats["tags"], 1):
            table.add_row(str(i), t["tag"], str(t["count"]))

        console.print(table)

    # Category frequency table
    if stats["categories"]:
        console.print()
        table = Table(title="Category Frequency")
        table.add_column("#", justify="right", style="dim")
        table.add_column("Category", style="cyan")
        table.add_column("Count", justify="right")

        for i, c in enumerate(stats["categories"], 1):
            table.add_row(str(i), c["category"], str(c["count"]))

        console.print(table)

    # Top co-occurrences
    cooc = stats["co_occurrences"]
    if cooc:
        console.print()
        top_pairs = sorted(cooc.items(), key=lambda x: x[1], reverse=True)[:10]
        table = Table(title="Top Co-occurring Tags")
        table.add_column("Tag A", style="cyan")
        table.add_column("Tag B", style="cyan")
        table.add_column("Times", justify="right")

        for (a, b), count in top_pairs:
            table.add_row(a, b, str(count))

        console.print(table)


@taxonomy.command(name="normalize")
@click.option("--from", "from_term", required=True, help="Term to rename from")
@click.option("--to", "to_term", required=True, help="Term to rename to")
@click.option("--field", default="tags", type=click.Choice(["tags", "categories"]))
@click.option("--dry-run", is_flag=True, help="Preview without changes")
@click.option("-y", "--yes", is_flag=True, help="Skip confirmation")
def normalize_cmd(
    from_term: str,
    to_term: str,
    field: str,
    dry_run: bool,
    yes: bool,
) -> None:
    """Rename a taxonomy term across all content files."""
    from mf.content.frontmatter import FrontMatterEditor
    from mf.taxonomy.analyzer import TaxonomyAnalyzer

    analyzer = TaxonomyAnalyzer()
    data = analyzer.collect(include_drafts=True)

    items_map = data.tag_items if field == "tags" else data.category_items
    affected_slugs = items_map.get(from_term, [])

    if not affected_slugs:
        console.print(f"[yellow]Term '{from_term}' not found in any content.[/yellow]")
        return

    console.print(
        f"Renaming [cyan]{from_term}[/cyan] → [green]{to_term}[/green] "
        f"in {len(affected_slugs)} file(s)"
    )

    if dry_run:
        for slug in affected_slugs:
            console.print(f"  [dim]Would update:[/dim] {slug}")
        return

    if not yes:
        from rich.prompt import Confirm
        if not Confirm.ask("Proceed?", default=True):
            return

    # Find and update files
    from mf.content.scanner import ContentScanner

    scanner = ContentScanner()
    updated = 0
    failed = 0

    for ct in scanner.CONTENT_TYPES:
        for item in scanner.scan_type(ct, include_drafts=True):
            terms = item.front_matter.get(field, [])
            if from_term in terms:
                editor = FrontMatterEditor(item.path)
                if not editor.load():
                    failed += 1
                    continue

                editor.remove_from_list(field, from_term)
                editor.add_to_list(field, to_term)

                if editor.save():
                    updated += 1
                else:
                    failed += 1

    console.print(f"[green]Updated {updated} file(s)[/green]")
    if failed:
        console.print(f"[red]Failed: {failed} file(s)[/red]")
```

**Step 4: Register taxonomy in cli.py**

Add at `scripts/mf/src/mf/cli.py` line 120 (after series import):

```python
from mf.taxonomy.commands import taxonomy  # noqa: E402
```

And add after line 131:

```python
main.add_command(taxonomy)
```

**Step 5: Run tests to verify they pass**

Run: `cd ~/github/repos/metafunctor/scripts/mf && python -m pytest tests/test_taxonomy/ -v`
Expected: All tests PASS

Run: `cd ~/github/repos/metafunctor/scripts/mf && python -m pytest -v`
Expected: All existing tests still PASS

**Step 6: Commit**

```bash
cd ~/github/repos/metafunctor
git add scripts/mf/src/mf/taxonomy/commands.py scripts/mf/src/mf/cli.py \
       scripts/mf/tests/test_taxonomy/test_commands.py
git commit -m "feat(mf): add taxonomy CLI commands (audit/normalize/orphans/stats)

- audit: detect case mismatches, plural/singular, hyphen/space
- normalize: batch-rename terms via FrontMatterEditor
- orphans: find terms used by only 1 content item
- stats: frequency tables and co-occurrence analysis

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>"
```

---

### Task 3: HealthChecker Core

Create the health check engine for content quality checks.

**Files:**
- Create: `scripts/mf/src/mf/health/__init__.py`
- Create: `scripts/mf/src/mf/health/checks.py`
- Test: `scripts/mf/tests/test_health/__init__.py`
- Test: `scripts/mf/tests/test_health/test_checks.py`

**Step 1: Write failing tests for HealthChecker**

Create `scripts/mf/tests/test_health/__init__.py` (empty) and `test_checks.py`:

```python
"""Tests for health checks."""

from __future__ import annotations

import pytest
from pathlib import Path

from mf.health.checks import HealthChecker


class TestBrokenLinks:
    """Test broken internal link detection."""

    def test_detects_broken_internal_link(self, create_content_file):
        create_content_file(
            slug="post-a",
            body="See [other post](/post/nonexistent/) for details.",
        )
        checker = HealthChecker()
        issues = checker.check_links()
        assert len(issues) >= 1
        assert any("/post/nonexistent/" in i["link"] for i in issues)

    def test_valid_link_no_issue(self, create_content_file):
        create_content_file(slug="post-a", body="Normal text, no links.")
        create_content_file(slug="post-b", body="See [post A](/post/post-a/).")
        checker = HealthChecker()
        issues = checker.check_links()
        # post-b links to post-a which exists
        broken = [i for i in issues if i["slug"] == "post-b"]
        assert len(broken) == 0

    def test_ignores_external_links(self, create_content_file):
        create_content_file(
            slug="post-a",
            body="See [Google](https://google.com) for info.",
        )
        checker = HealthChecker()
        issues = checker.check_links()
        assert len(issues) == 0


class TestMissingDescriptions:
    """Test missing description detection."""

    def test_detects_missing_description(self, create_content_file):
        create_content_file(slug="post-a")  # No description
        checker = HealthChecker()
        issues = checker.check_descriptions()
        assert len(issues) >= 1
        assert any(i["slug"] == "post-a" for i in issues)

    def test_has_description_no_issue(self, create_content_file):
        create_content_file(
            slug="post-a",
            extra_fm={"description": "A good post about things."},
        )
        checker = HealthChecker()
        issues = checker.check_descriptions()
        assert not any(i["slug"] == "post-a" for i in issues)

    def test_empty_description_is_issue(self, create_content_file):
        create_content_file(slug="post-a", extra_fm={"description": ""})
        checker = HealthChecker()
        issues = checker.check_descriptions()
        assert any(i["slug"] == "post-a" for i in issues)


class TestMissingImages:
    """Test missing featured_image detection."""

    def test_detects_missing_image(self, create_content_file):
        create_content_file(slug="post-a")  # No featured_image
        checker = HealthChecker()
        issues = checker.check_images()
        assert any(i["slug"] == "post-a" for i in issues)

    def test_has_image_no_issue(self, create_content_file):
        create_content_file(
            slug="post-a",
            extra_fm={"featured_image": "/images/hero.jpg"},
        )
        checker = HealthChecker()
        issues = checker.check_images()
        assert not any(i["slug"] == "post-a" for i in issues)


class TestDrafts:
    """Test draft listing with age."""

    def test_lists_drafts(self, create_content_file):
        create_content_file(slug="my-draft", draft=True)
        checker = HealthChecker()
        drafts = checker.check_drafts()
        assert len(drafts) >= 1
        assert any(d["slug"] == "my-draft" for d in drafts)

    def test_skips_non_drafts(self, create_content_file):
        create_content_file(slug="published", draft=False)
        checker = HealthChecker()
        drafts = checker.check_drafts()
        assert not any(d["slug"] == "published" for d in drafts)

    def test_draft_includes_age(self, create_content_file):
        create_content_file(slug="old-draft", draft=True)
        checker = HealthChecker()
        drafts = checker.check_drafts()
        draft = next(d for d in drafts if d["slug"] == "old-draft")
        assert "days_old" in draft


class TestStaleProjects:
    """Test stale project detection."""

    def test_detects_stale_project(self, mock_site_root):
        """Create a project page with different desc than DB."""
        import json
        import yaml

        # Set up projects_db with a description
        db_path = mock_site_root / ".mf" / "projects_db.json"
        db_data = {
            "my-proj": {
                "title": "My Project",
                "description": "New description from GitHub",
            }
        }
        db_path.write_text(json.dumps(db_data))

        # Create project content with old description
        proj_dir = mock_site_root / "content" / "projects" / "my-proj"
        proj_dir.mkdir(parents=True)
        fm = {
            "title": "My Project",
            "description": "Old description",
        }
        (proj_dir / "index.md").write_text(
            f"---\n{yaml.dump(fm)}---\n\nContent.\n"
        )

        checker = HealthChecker(site_root=mock_site_root)
        issues = checker.check_stale()
        assert len(issues) >= 1
        assert any(i["slug"] == "my-proj" for i in issues)
```

**Step 2: Run tests to verify they fail**

Run: `cd ~/github/repos/metafunctor/scripts/mf && python -m pytest tests/test_health/test_checks.py -v`
Expected: FAIL with `ModuleNotFoundError`

**Step 3: Implement HealthChecker**

Create `scripts/mf/src/mf/health/__init__.py`:
```python
"""Content health checking tools."""
```

Create `scripts/mf/src/mf/health/checks.py`:
```python
"""Content health checks.

Checks for broken links, missing descriptions/images, stale projects, and drafts.
"""

from __future__ import annotations

import json
from datetime import datetime
from pathlib import Path
from typing import Any

from mf.content.scanner import ContentScanner
from mf.core.config import get_paths


class HealthChecker:
    """Runs content health checks."""

    # Content types to check (excludes projects for most checks)
    POST_TYPES = ["post"]
    ALL_TYPES = ["post", "papers", "writing", "publications", "research"]

    def __init__(self, site_root: Path | None = None):
        if site_root is None:
            site_root = get_paths().root
        self.site_root = site_root
        self.scanner = ContentScanner(site_root)

    def _build_known_paths(self) -> set[str]:
        """Build set of known Hugo content paths."""
        paths: set[str] = set()
        for ct in self.scanner.CONTENT_TYPES:
            for item in self.scanner.scan_type(ct, include_drafts=True):
                paths.add(item.hugo_path)
        return paths

    def check_links(
        self,
        content_types: list[str] | None = None,
    ) -> list[dict[str, Any]]:
        """Find broken internal links.

        Returns list of dicts: slug, title, link, content_type.
        """
        if content_types is None:
            content_types = self.ALL_TYPES

        known = self._build_known_paths()
        issues: list[dict[str, Any]] = []

        for ct in content_types:
            for item in self.scanner.scan_type(ct):
                for link in item.extract_internal_links():
                    # Skip external, anchor-only, static
                    if link.startswith(("#", "http://", "https://")):
                        continue
                    static_prefixes = (
                        "/images/", "/latex/", "/css/", "/js/", "/files/",
                    )
                    if any(link.startswith(p) for p in static_prefixes):
                        continue

                    # Normalize
                    path = link.rstrip("/")
                    if not path.startswith("/"):
                        path = "/" + path
                    path += "/"

                    if path not in known:
                        issues.append({
                            "slug": item.slug,
                            "title": item.title,
                            "link": link,
                            "content_type": ct,
                        })

        return issues

    def check_descriptions(
        self,
        content_types: list[str] | None = None,
    ) -> list[dict[str, Any]]:
        """Find posts missing description field.

        Returns list of dicts: slug, title, content_type.
        """
        if content_types is None:
            content_types = self.POST_TYPES

        issues: list[dict[str, Any]] = []

        for ct in content_types:
            for item in self.scanner.scan_type(ct):
                desc = item.front_matter.get("description", "")
                if not desc or (isinstance(desc, str) and not desc.strip()):
                    issues.append({
                        "slug": item.slug,
                        "title": item.title,
                        "content_type": ct,
                    })

        return issues

    def check_images(
        self,
        content_types: list[str] | None = None,
    ) -> list[dict[str, Any]]:
        """Find posts missing featured_image.

        Returns list of dicts: slug, title, content_type.
        """
        if content_types is None:
            content_types = self.POST_TYPES

        issues: list[dict[str, Any]] = []

        for ct in content_types:
            for item in self.scanner.scan_type(ct):
                img = item.front_matter.get("featured_image", "")
                if not img:
                    issues.append({
                        "slug": item.slug,
                        "title": item.title,
                        "content_type": ct,
                    })

        return issues

    def check_drafts(self) -> list[dict[str, Any]]:
        """List all drafts with age.

        Returns list of dicts: slug, title, date, days_old, content_type.
        """
        results: list[dict[str, Any]] = []

        for ct in self.scanner.CONTENT_TYPES:
            for item in self.scanner.scan_type(ct, include_drafts=True):
                if not item.is_draft:
                    continue

                date_val = item.front_matter.get("date")
                days_old = None
                date_str = None

                if date_val:
                    try:
                        if isinstance(date_val, datetime):
                            dt = date_val
                        else:
                            dt = datetime.fromisoformat(str(date_val)[:10])
                        days_old = (datetime.now() - dt).days
                        date_str = str(date_val)[:10]
                    except (ValueError, TypeError):
                        date_str = str(date_val)

                results.append({
                    "slug": item.slug,
                    "title": item.title,
                    "date": date_str,
                    "days_old": days_old,
                    "content_type": ct,
                })

        return sorted(results, key=lambda x: x.get("days_old") or 0, reverse=True)

    def check_stale(self) -> list[dict[str, Any]]:
        """Find projects where content description diverged from DB.

        Compares description in content/projects/ front matter against
        projects_db.json entries.

        Returns list of dicts: slug, content_desc, db_desc.
        """
        paths = get_paths()
        db_path = paths.projects_db

        if not db_path.exists():
            return []

        try:
            db_data = json.loads(db_path.read_text())
        except (json.JSONDecodeError, OSError):
            return []

        issues: list[dict[str, Any]] = []
        projects = self.scanner.scan_type("projects", include_drafts=True)

        for item in projects:
            slug = item.slug
            entry = db_data.get(slug, {})
            if isinstance(entry, str) or not entry:
                continue

            db_desc = entry.get("description", "")
            content_desc = item.front_matter.get("description", "")

            if db_desc and content_desc and db_desc != content_desc:
                issues.append({
                    "slug": slug,
                    "title": item.title,
                    "content_desc": content_desc,
                    "db_desc": db_desc,
                })

        return issues
```

**Step 4: Run tests to verify they pass**

Run: `cd ~/github/repos/metafunctor/scripts/mf && python -m pytest tests/test_health/test_checks.py -v`
Expected: All 12 tests PASS

**Step 5: Commit**

```bash
cd ~/github/repos/metafunctor
git add scripts/mf/src/mf/health/ scripts/mf/tests/test_health/
git commit -m "feat(mf): add HealthChecker for content quality checks

Checks for broken internal links, missing descriptions, missing images,
stale project descriptions (diverged from DB), and draft age.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>"
```

---

### Task 4: Health CLI Commands

Create Click commands that expose HealthChecker functionality.

**Files:**
- Create: `scripts/mf/src/mf/health/commands.py`
- Modify: `scripts/mf/src/mf/cli.py` (add 2 lines)
- Test: `scripts/mf/tests/test_health/test_commands.py`

**Step 1: Write failing tests for health CLI**

Create `scripts/mf/tests/test_health/test_commands.py`:

```python
"""Tests for health CLI commands."""

from __future__ import annotations

import json

import pytest
from click.testing import CliRunner

from mf.health.commands import health


@pytest.fixture
def runner():
    return CliRunner()


class TestHealthLinks:
    """Test mf health links."""

    def test_reports_broken_links(self, runner, create_content_file):
        create_content_file(
            slug="post-a",
            body="See [missing](/post/nonexistent/) link.",
        )
        result = runner.invoke(health, ["links"])
        assert result.exit_code == 0
        assert "nonexistent" in result.output

    def test_links_json(self, runner, create_content_file):
        create_content_file(
            slug="post-a",
            body="Link to [bad](/post/nope/).",
        )
        result = runner.invoke(health, ["links", "--json"])
        assert result.exit_code == 0
        data = json.loads(result.output)
        assert isinstance(data, list)

    def test_no_broken_links(self, runner, create_content_file):
        create_content_file(slug="post-a", body="No links here.")
        result = runner.invoke(health, ["links"])
        assert result.exit_code == 0
        assert "No broken" in result.output or "clean" in result.output.lower()


class TestHealthDescriptions:
    """Test mf health descriptions."""

    def test_finds_missing(self, runner, create_content_file):
        create_content_file(slug="post-a")
        result = runner.invoke(health, ["descriptions"])
        assert result.exit_code == 0
        assert "post-a" in result.output

    def test_descriptions_json(self, runner, create_content_file):
        create_content_file(slug="post-a")
        result = runner.invoke(health, ["descriptions", "--json"])
        assert result.exit_code == 0
        data = json.loads(result.output)
        assert len(data) >= 1


class TestHealthImages:
    """Test mf health images."""

    def test_finds_missing(self, runner, create_content_file):
        create_content_file(slug="post-a")
        result = runner.invoke(health, ["images"])
        assert result.exit_code == 0
        assert "post-a" in result.output

    def test_images_json(self, runner, create_content_file):
        create_content_file(slug="post-a")
        result = runner.invoke(health, ["images", "--json"])
        assert result.exit_code == 0
        data = json.loads(result.output)
        assert len(data) >= 1


class TestHealthDrafts:
    """Test mf health drafts."""

    def test_lists_drafts(self, runner, create_content_file):
        create_content_file(slug="my-draft", draft=True)
        result = runner.invoke(health, ["drafts"])
        assert result.exit_code == 0
        assert "my-draft" in result.output

    def test_drafts_json(self, runner, create_content_file):
        create_content_file(slug="my-draft", draft=True)
        result = runner.invoke(health, ["drafts", "--json"])
        assert result.exit_code == 0
        data = json.loads(result.output)
        assert len(data) >= 1

    def test_no_drafts(self, runner, create_content_file):
        create_content_file(slug="published")
        result = runner.invoke(health, ["drafts"])
        assert result.exit_code == 0
        assert "No drafts" in result.output or "0" in result.output


class TestHealthStale:
    """Test mf health stale."""

    def test_stale_json(self, runner, mock_site_root):
        result = runner.invoke(health, ["stale", "--json"])
        assert result.exit_code == 0
        data = json.loads(result.output)
        assert isinstance(data, list)
```

**Step 2: Run tests to verify they fail**

Run: `cd ~/github/repos/metafunctor/scripts/mf && python -m pytest tests/test_health/test_commands.py -v`
Expected: FAIL

**Step 3: Implement health CLI commands**

Create `scripts/mf/src/mf/health/commands.py`:

```python
"""CLI commands for content health checks.

Check links, descriptions, images, stale projects, and drafts.
"""

from __future__ import annotations

import json as json_module

import click
from rich.console import Console
from rich.table import Table

console = Console()


@click.group(name="health")
def health() -> None:
    """Content health checks: links, descriptions, images, stale, drafts."""
    pass


@health.command(name="links")
@click.option("--json", "as_json", is_flag=True, help="Output as JSON")
def links_cmd(as_json: bool) -> None:
    """Find broken internal links in content."""
    from mf.health.checks import HealthChecker

    checker = HealthChecker()
    issues = checker.check_links()

    if as_json:
        click.echo(json_module.dumps(issues, indent=2))
        return

    if not issues:
        console.print("[green]No broken internal links found.[/green]")
        return

    table = Table(title=f"Broken Internal Links ({len(issues)})")
    table.add_column("Content", style="cyan", no_wrap=False)
    table.add_column("Broken Link", style="red")
    table.add_column("Type", style="dim")

    for i in issues:
        table.add_row(i["title"], i["link"], i["content_type"])

    console.print(table)


@health.command(name="descriptions")
@click.option("--json", "as_json", is_flag=True, help="Output as JSON")
def descriptions_cmd(as_json: bool) -> None:
    """Find posts missing the description field."""
    from mf.health.checks import HealthChecker

    checker = HealthChecker()
    issues = checker.check_descriptions()

    if as_json:
        click.echo(json_module.dumps(issues, indent=2))
        return

    if not issues:
        console.print("[green]All posts have descriptions.[/green]")
        return

    table = Table(title=f"Missing Descriptions ({len(issues)})")
    table.add_column("Slug", style="cyan")
    table.add_column("Title", no_wrap=False)

    for i in issues:
        table.add_row(i["slug"], i["title"])

    console.print(table)
    console.print()
    console.print(
        "[dim]Fix with: mf posts set <slug> description \"...\"[/dim]"
    )


@health.command(name="images")
@click.option("--json", "as_json", is_flag=True, help="Output as JSON")
def images_cmd(as_json: bool) -> None:
    """Find posts missing featured_image."""
    from mf.health.checks import HealthChecker

    checker = HealthChecker()
    issues = checker.check_images()

    if as_json:
        click.echo(json_module.dumps(issues, indent=2))
        return

    if not issues:
        console.print("[green]All posts have featured images.[/green]")
        return

    table = Table(title=f"Missing Featured Images ({len(issues)})")
    table.add_column("Slug", style="cyan")
    table.add_column("Title", no_wrap=False)

    for i in issues:
        table.add_row(i["slug"], i["title"])

    console.print(table)


@health.command(name="drafts")
@click.option("--json", "as_json", is_flag=True, help="Output as JSON")
def drafts_cmd(as_json: bool) -> None:
    """List all drafts with age."""
    from mf.health.checks import HealthChecker

    checker = HealthChecker()
    drafts = checker.check_drafts()

    if as_json:
        click.echo(json_module.dumps(drafts, indent=2))
        return

    if not drafts:
        console.print("[green]No drafts found.[/green]")
        return

    table = Table(title=f"Drafts ({len(drafts)})")
    table.add_column("Slug", style="cyan")
    table.add_column("Title", no_wrap=False)
    table.add_column("Date", style="dim")
    table.add_column("Age (days)", justify="right")
    table.add_column("Type", style="dim")

    for d in drafts:
        age = str(d["days_old"]) if d["days_old"] is not None else "?"
        table.add_row(d["slug"], d["title"], d.get("date", "?"), age, d["content_type"])

    console.print(table)


@health.command(name="stale")
@click.option("--json", "as_json", is_flag=True, help="Output as JSON")
def stale_cmd(as_json: bool) -> None:
    """Find projects where content description diverged from database."""
    from mf.health.checks import HealthChecker

    checker = HealthChecker()
    issues = checker.check_stale()

    if as_json:
        click.echo(json_module.dumps(issues, indent=2))
        return

    if not issues:
        console.print("[green]No stale project descriptions found.[/green]")
        return

    table = Table(title=f"Stale Project Descriptions ({len(issues)})")
    table.add_column("Project", style="cyan")
    table.add_column("Content Desc", no_wrap=False, max_width=40)
    table.add_column("DB Desc", no_wrap=False, max_width=40)

    for i in issues:
        table.add_row(i["slug"], i["content_desc"][:40], i["db_desc"][:40])

    console.print(table)
    console.print()
    console.print("[dim]Fix with: mf projects generate --slug <slug>[/dim]")
```

**Step 4: Register health in cli.py**

Add at `scripts/mf/src/mf/cli.py` (after the taxonomy import):

```python
from mf.health.commands import health  # noqa: E402
```

And add after `main.add_command(taxonomy)`:

```python
main.add_command(health)
```

**Step 5: Run tests to verify they pass**

Run: `cd ~/github/repos/metafunctor/scripts/mf && python -m pytest tests/test_health/ -v`
Expected: All tests PASS

Run: `cd ~/github/repos/metafunctor/scripts/mf && python -m pytest -v`
Expected: All tests PASS (including all previous tests)

**Step 6: Commit**

```bash
cd ~/github/repos/metafunctor
git add scripts/mf/src/mf/health/commands.py scripts/mf/src/mf/cli.py \
       scripts/mf/tests/test_health/test_commands.py
git commit -m "feat(mf): add health CLI commands (links/descriptions/images/stale/drafts)

- links: broken internal link detection
- descriptions: posts missing description field
- images: posts missing featured_image
- stale: project descriptions diverged from DB
- drafts: list all drafts with age

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>"
```

---

### Task 5: Update Plugin Skills

Update the global mf plugin skill files with new commands.

**Files:**
- Modify: `~/.claude/plugins/repos/mf-plugin/skills/mf/SKILL.md`
- Modify: `~/.claude/plugins/repos/mf-plugin/skills/mf/COMMANDS.md`
- Modify: `~/.claude/plugins/repos/mf-plugin/skills/mf/WORKFLOWS.md`

**Step 1: Add taxonomy and health sections to SKILL.md**

Add to the CLI Reference section (after Posts):

```markdown
### Taxonomy
```bash
mf taxonomy audit                      # Find near-duplicates, case mismatches
mf taxonomy normalize --from X --to Y  # Rename term across all content
mf taxonomy orphans                    # Tags used by only 1 content item
mf taxonomy stats                      # Frequency, co-occurrence tables
```

### Health
```bash
mf health links                        # Find broken internal links
mf health descriptions                 # Posts missing description
mf health images                       # Posts missing featured_image
mf health stale                        # Projects where desc diverged from DB
mf health drafts                       # List all drafts with age
```
```

**Step 2: Add taxonomy and health sections to COMMANDS.md**

Add full command reference with all options for each subcommand.

**Step 3: Add taxonomy and health workflows to WORKFLOWS.md**

Add sections:
- Taxonomy Hygiene Workflow (audit → review → normalize → verify)
- Content Health Workflow (run checks → fix issues → deploy)

**Step 4: Commit plugin changes**

```bash
cd ~/.claude/plugins/repos/mf-plugin
git add -A && git commit -m "feat: add taxonomy and health command docs

Phase 3: taxonomy audit/normalize/orphans/stats + health links/descriptions/images/stale/drafts"
```

---

### Task 6: Smoke Test & Integration Verification

End-to-end verification on the real metafunctor site.

**Step 1: Run full test suite**

```bash
cd ~/github/repos/metafunctor/scripts/mf && python -m pytest -v --tb=short
```

Expected: All tests pass (previous ~1054 + new ~40)

**Step 2: Smoke test taxonomy commands on real site**

```bash
mf taxonomy stats --limit 10
mf taxonomy audit
mf taxonomy orphans
mf taxonomy orphans --json | python -m json.tool | head -20
```

**Step 3: Smoke test health commands on real site**

```bash
mf health descriptions
mf health drafts
mf health links
mf health stale
mf health images --json | python -m json.tool | head -20
```

**Step 4: Cross-directory test**

```bash
cd /tmp && mf taxonomy stats --limit 5
cd /tmp && mf health drafts
```

**Step 5: Commit any fixes discovered during smoke testing**

If any issues are found, fix and commit before marking complete.
