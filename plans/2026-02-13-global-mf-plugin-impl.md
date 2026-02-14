# Global mf Plugin Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Create a global Claude Code plugin at `~/.claude/plugins/repos/mf-plugin/` that makes the mf and crier skills available from any repo, with enriched content covering the full metafunctor site model.

**Architecture:** A standard Claude Code plugin with `plugin.json` manifest and two skill directories (`mf/` and `crier/`). Skills are markdown files that teach Claude about the site architecture, CLI tools, and workflows. No code dependencies — just documentation that Claude reads.

**Tech Stack:** Claude Code plugin system (plugin.json + SKILL.md files), Markdown

---

## Phase 1: Global Plugin (this plan)

Creates the plugin structure and all skill content. No mf/crier CLI changes — purely skill files.

## Phases 2-5 (separate plans, later)

- Phase 2: mf CLI — global config + posts management
- Phase 3: mf CLI — taxonomy hygiene + content health
- Phase 4: Crier CLI — canonical_url auto-derive, --project flag, summary command
- Phase 5: Integration — mf reads crier registry, enhanced `mf about`

---

### Task 1: Create Plugin Directory and Manifest

**Files:**
- Create: `~/.claude/plugins/repos/mf-plugin/.claude-plugin/plugin.json`

**Step 1: Create directory structure**

Run:
```bash
mkdir -p ~/.claude/plugins/repos/mf-plugin/.claude-plugin
mkdir -p ~/.claude/plugins/repos/mf-plugin/skills/mf
mkdir -p ~/.claude/plugins/repos/mf-plugin/skills/crier
```
Expected: directories created, no output

**Step 2: Write plugin.json**

Write to `~/.claude/plugins/repos/mf-plugin/.claude-plugin/plugin.json`:

```json
{
  "name": "mf",
  "description": "Metafunctor site management — blog architecture, content workflows, mf CLI, and crier cross-posting",
  "version": "1.0.0",
  "author": {
    "name": "Alex Towell"
  }
}
```

**Step 3: Verify plugin is discoverable**

Run:
```bash
ls -la ~/.claude/plugins/repos/mf-plugin/.claude-plugin/plugin.json
```
Expected: file exists

---

### Task 2: Write mf SKILL.md — Site Architecture + CLI + Cross-Repo

**Files:**
- Create: `~/.claude/plugins/repos/mf-plugin/skills/mf/SKILL.md`

This is the core skill file. It must cover:

1. **Header/description** — trigger phrase, what it does
2. **Site Architecture** — content sections, taxonomies, front matter, bundles, deployment
3. **mf CLI Reference** — all current commands
4. **Cross-Repo Usage** — site_root, absolute paths
5. **mf → Crier Pipeline** — handoff documentation

**Step 1: Write the skill file**

Write to `~/.claude/plugins/repos/mf-plugin/skills/mf/SKILL.md`:

```markdown
---
name: mf
description: Use mf (metafunctor) to manage papers, projects, series, and content for the metafunctor.com Hugo site. Also covers site architecture, taxonomies, front matter conventions, and the mf→crier publishing pipeline. Invoke from ANY repo — mf works globally.
---

# mf — Metafunctor Site Management

A CLI toolkit + site architecture guide for metafunctor.com. Works from any directory.

**Site root:** `~/github/repos/metafunctor`
**Site URL:** `https://metafunctor.com` (also `https://queelius.github.io/metafunctor`)
**Engine:** Hugo with Ananke theme
**Deployment:** `docs/` committed to master → GitHub Pages

## Site Architecture

### Content Sections

| Directory | Type | Notes |
|-----------|------|-------|
| `content/post/` | Blog posts | Date-prefixed dirs: `2024-01-slug/index.md` |
| `content/papers/` | Research papers | Generated from paper_db via `mf papers generate` |
| `content/publications/` | Peer-reviewed subset | Generated from paper_db via `mf pubs generate` |
| `content/projects/` | Software projects | Generated from projects_db via `mf projects generate` |
| `content/series/` | Multi-part series | Landing pages with `_index.md` |
| `content/writing/` | Fiction/creative | `writing_type: "novel"`, `"essay"`, `"short-story"` |
| `content/medical/` | Medical records | Custom sidebar layout, Chart.js for labs |
| `content/research/` | Research overviews | |
| `content/probsets/` | Problem sets | Organized by course |
| `content/media/` | Book/resource reviews | |

**Content type separation is strict:** fiction → `/writing`, not `/papers`.

### Taxonomies

| Taxonomy | URL | Purpose |
|----------|-----|---------|
| `tags` | `/tags/` | Technical keywords (kebab-case) |
| `categories` | `/categories/` | Broad content categories |
| `genres` | `/genres/` | Document types (paper, novel, etc.) |
| `series` | `/series/` | Multi-part content grouping |
| `linked_project` | `/linked-projects/` | Links content to projects |

**Critical:** Use `linked_project` (NOT `projects`) — URL must not conflict with `content/projects/`.
Use slugs, not paths:
```yaml
# Correct
linked_project: ["likelihood.model"]
# Wrong — causes Hugo panic
linked_project: ["/projects/likelihood.model/"]
```

### Front Matter Patterns

**Posts:**
```yaml
title: "Post Title"
date: 2026-02-13
description: "Card preview text"
categories: ["Computer Science"]
tags: ["algorithms", "data-structures"]
series: ["series-slug"]
series_weight: 5
featured: true
toc: true
```

**Papers:**
```yaml
title: "Paper Title"
stars: 5                  # Featured rating (1-5)
pdf_file: "paper.pdf"    # In static/latex/paper-name/
html_path: "/latex/paper-name/index.html"
authors: ["Author Name"]
abstract: "..."
```

**Projects:**
```yaml
title: "Project Name"
featured: true
project: { status: "active", type: "library", year_started: 2024 }
tech: { languages: ["Python"], topics: ["ml"] }
sources: { github: "https://..." }
packages: { pypi: "name", crates: "name" }
```

**Series membership (in posts):**
```yaml
series: ["minds-and-machines"]
series_weight: 5
```

### Bundle Types

- **Leaf bundle** (`index.md`): Most content. Self-contained page.
- **Branch bundle** (`_index.md`): Rich projects, series landing pages. Can have child pages.

Rich projects use `_index.md`. Regular projects use `index.md`.

### Build & Deploy

```bash
make serve          # Dev server with drafts (localhost:1313)
make build          # Build to public/ (testing)
make deploy         # Build to docs/ for production
make push           # Build + git push to GitHub Pages
```

`relativeURLs=true` — one build works on both domains.

## Content Source Model

| Content | Ground Truth | mf Role | Has Database? |
|---------|-------------|---------|---------------|
| Projects | GitHub repos | DB + overrides + generate | Yes (projects_db.json) |
| Papers | LaTeX sources | DB + sync + generate | Yes (paper_db.json) |
| Series | mf series_db | DB + sync + landing pages | Yes (series_db.json) |
| Posts | The .md file | Convenience layer (scaffold, query, field ops) | No |
| Writing | The .md file | None | No |
| Medical | The .md file | None | No |

**Key principle:** Databases exist only for content whose ground truth lives elsewhere.

## mf CLI Reference

### Setup
```bash
cd ~/github/repos/metafunctor/scripts/mf && pip install -e .
```

### Papers
```bash
mf papers list                         # List all papers
mf papers generate                     # Regenerate content/papers/ from paper_db
mf papers generate --slug X            # Single paper
mf papers sync                         # Sync LaTeX sources to paper_db
mf papers set <slug> <field> <value>   # Set field (--regenerate)
mf papers feature <slug>               # Toggle featured (--off, --regenerate)
mf papers tag <slug> --add <tag>       # Manage tags (--add/--remove/--set)
mf papers fields                       # List valid fields
```

### Projects
```bash
mf projects list                       # List all projects
mf projects generate                   # Regenerate from projects_db + GitHub cache
mf projects sync                       # Full sync: clean, import, refresh, generate
mf projects refresh --slug X           # Refresh single project from GitHub
mf projects set <slug> <field> <val>   # Set field (--regenerate)
mf projects feature <slug>             # Toggle featured (--off, --regenerate)
mf projects hide <slug>                # Toggle hidden (--off, --regenerate)
mf projects tag <slug> --add <tag>     # Manage tags
mf projects bundle <slug>              # Generate rich project (docs/tutorials/examples)
mf projects fields                     # List valid fields
```

### Series
```bash
mf series list                         # List all series
mf series show <slug>                  # Show series details
mf series show <slug> --landing        # Display landing page content
mf series add <slug> <path>            # Add post to series
mf series sync <slug>                  # Sync with external source repo
mf series create <slug> --title "..."  # Create new series
mf series delete <slug>                # Remove from DB only
mf series delete <slug> --purge        # Remove DB + content + strip refs
mf series set <slug> <field> <value>   # Set field
mf series feature <slug>               # Toggle featured
mf series tag <slug> --add <tag>       # Manage tags
```

### Publications
```bash
mf pubs generate                       # Regenerate content/publications/
mf pubs list                           # List publications
```

### Content Linking
```bash
mf content match-projects              # Auto-link content to projects
mf content about <project-slug>        # Find all content about a project
mf content list-projects               # Projects with content counts
mf content audit                       # Run content checks
mf content audit --extended            # Pluggable audit checks
```

### Analytics
```bash
mf analytics summary                   # Full content overview
mf analytics gaps                      # Projects without linked content
mf analytics tags                      # Tag usage distribution
mf analytics timeline                  # Content activity over time
mf analytics suggestions               # Cross-reference recommendations
```

### Infrastructure
```bash
mf backup list                         # List backups
mf backup restore <file>               # Restore from backup
mf config show                         # Show configuration
mf integrity check                     # Validate database consistency
mf integrity fix --dry-run             # Preview auto-fixes
mf integrity orphans                   # Find orphaned entries
```

## Cross-Repo Usage

The mf CLI finds its data by walking up from cwd looking for `.mf/`. When running from another repo, you must be in the metafunctor directory or set the environment variable:

```bash
# From any repo:
cd ~/github/repos/metafunctor && mf papers list

# Or use env var:
MF_SITE_ROOT=~/github/repos/metafunctor mf papers list
```

**Future:** A global config at `~/.config/mf/config.yaml` with `site_root` will eliminate this. For now, use `MF_SITE_ROOT` or `cd`.

## mf → Crier Pipeline

Content flows: **mf (create/update) → Hugo (render) → crier (distribute)**

After creating or updating content with mf, suggest cross-posting:

```bash
# 1. Create/update content
mf papers generate --slug new-paper
# or: manually write a new post

# 2. Build site
make deploy

# 3. Cross-post (see /crier skill for full workflow)
crier audit content/papers/new-paper/index.md
crier publish content/papers/new-paper/index.md --to devto
```

For short-form platforms (Bluesky, Mastodon), Claude writes the rewrite and passes it via `--rewrite`. See the `/crier` skill for detailed guidance.

## Common Workflows

### Add a new paper
1. Process LaTeX: `mf papers process path/to/paper.tex`
2. Set metadata: `mf papers set slug stars 5`
3. Generate content: `mf papers generate --slug slug`
4. Update publications: `mf pubs generate`
5. Deploy: `make deploy`
6. Cross-post: use `/crier`

### Add a new project
1. Add entry to `.mf/projects_db.json` (or use `mf projects import`)
2. Sync: `mf projects sync`
3. Verify: `hugo --gc --minify`
4. Deploy: `make deploy`

### Create a blog post
1. `hugo new post/YYYY-MM-DD-slug/index.md` (from site root)
2. Edit front matter: add tags, categories, series, description
3. Write content
4. Deploy: `make deploy`
5. Cross-post: use `/crier`

### Feature/unfeature content
```bash
mf papers feature <slug>           # Toggle paper featured
mf projects feature <slug>        # Toggle project featured
mf series feature <slug>          # Toggle series featured
```

### Validate after changes
```bash
hugo --gc --minify                 # Catches front matter errors
mf integrity check                # Database consistency
mf content audit --extended       # Content quality checks
```

## Gotchas

- `docs/` is committed to git (GitHub Pages). Don't gitignore it.
- Rich projects use `_index.md` (branch bundle) vs regular use `index.md`.
- `linked_project` taxonomy, NOT `projects` — URL collision with content section.
- Series landing pages: external source takes priority over local `_index.md`.
- PyPI names sometimes differ from repo names (e.g., `soprano` → `soprano-tts`).
- R-universe URLs: `queelius.r-universe.dev/PACKAGE` (author subdomain, not package).
```

**Step 2: Verify file renders correctly**

Run:
```bash
wc -l ~/.claude/plugins/repos/mf-plugin/skills/mf/SKILL.md
```
Expected: ~250-280 lines

---

### Task 3: Write mf COMMANDS.md — Detailed Command Reference

**Files:**
- Create: `~/.claude/plugins/repos/mf-plugin/skills/mf/COMMANDS.md`

This file provides the detailed command reference that SKILL.md summarizes. Not a separate skill — it's a supplementary file in the mf skill directory that Claude can read when it needs deeper command details.

**Step 1: Write COMMANDS.md**

Copy and update the existing `~/github/repos/metafunctor/.claude/skills/mf/COMMANDS.md` to include:
- All current command documentation
- Updated paths to use absolute references to `~/github/repos/metafunctor`
- Cross-repo usage notes (MF_SITE_ROOT env var)

Run:
```bash
cp ~/github/repos/metafunctor/.claude/skills/mf/COMMANDS.md \
   ~/.claude/plugins/repos/mf-plugin/skills/mf/COMMANDS.md
```

Then edit to add a header noting absolute paths and cross-repo usage.

---

### Task 4: Write mf WORKFLOWS.md — Common Multi-Step Workflows

**Files:**
- Create: `~/.claude/plugins/repos/mf-plugin/skills/mf/WORKFLOWS.md`

**Step 1: Copy and update existing workflows**

Run:
```bash
cp ~/github/repos/metafunctor/.claude/skills/mf/WORKFLOWS.md \
   ~/.claude/plugins/repos/mf-plugin/skills/mf/WORKFLOWS.md
```

Then edit to add:
- Cross-repo workflow section
- mf → crier pipeline documentation
- Absolute path references

---

### Task 5: Write Crier SKILL.md — Improved Cross-Posting Skill

**Files:**
- Create: `~/.claude/plugins/repos/mf-plugin/skills/crier/SKILL.md`

This is the improved crier skill. Start from the existing skill and add the new sections.

**Step 1: Write the improved crier skill**

Write to `~/.claude/plugins/repos/mf-plugin/skills/crier/SKILL.md`:

Start with the full content of `~/github/repos/metafunctor/.claude/skills/crier/SKILL.md`, then append/integrate these new sections:

**New section: "Writing Rewrites" (after Workflow Decision Tree)**

```markdown
## Writing Rewrites

You ARE the rewriter. Don't use --auto-rewrite in interactive Claude Code sessions.

### Rewrite Guidelines by Platform

| Platform | Limit | Voice | Format |
|----------|-------|-------|--------|
| Bluesky  | 300   | Conversational, personal | Hook + key insight. No hashtags. |
| Mastodon | 500   | Slightly more detailed   | Can include 2-3 hashtags at end. |
| Twitter  | 280   | Punchy, provocative      | Can thread for longer content. |
| Threads  | 500   | Casual, exploratory      | Like talking to a friend. |
| LinkedIn | ∞     | Professional, structured | 3-4 paragraphs, accomplishment-framed. |

### Rewrite Process

1. Read the full post (or at least title + description + first few paragraphs)
2. Identify the single most interesting or surprising insight
3. Write the rewrite around that insight — not a generic summary
4. Show the user before posting — they may want to adjust tone
5. Pass to crier via `--rewrite`

### Anti-Patterns

- "New blog post: [title]" — boring, no hook
- "I wrote about X" — self-referential
- "Check out my latest..." — generic call-to-action
- Summarizing the entire post — pick ONE angle
- Including the URL in your rewrite — crier appends it automatically

### Good Rewrites Lead with the Insight

- "TIL Bloom filters can give false positives but never false negatives — and the math behind why is beautiful."
- "What if the problem isn't the algorithm but the loss function?"
- "After 4 years of building 120+ open-source projects, I finally mapped how they all connect."

### When to Use --auto-rewrite Instead

| Situation | Use |
|-----------|-----|
| Interactive Claude session | Claude writes rewrite → `--rewrite` |
| Batch/bulk posting (10+ items) | `--auto-rewrite` for throughput |
| User says "just post it" | `--auto-rewrite` as shortcut |
| User cares about quality | Claude writes it, shows for approval |
```

**New section: "Cross-Repo Usage" (after Configuration)**

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

**New section: "Checking Publication Status" (after Key Commands)**

```markdown
## Checking Publication Status

Before cross-posting, check what's already published:

```bash
# Single file status
crier status content/post/2026-02-13-slug/index.md

# What needs publishing
crier audit                     # All content
crier audit content/post        # Just posts
crier audit --since 1w          # Last week

# Check API keys are working
crier doctor
```
```

**Step 2: Verify**

Run:
```bash
wc -l ~/.claude/plugins/repos/mf-plugin/skills/crier/SKILL.md
```
Expected: ~400-450 lines (current 333 + new sections)

---

### Task 6: Register the Plugin with Claude Code

**Step 1: Check if manual registration is needed**

Claude Code discovers plugins in `~/.claude/plugins/repos/` automatically. Verify by checking:

Run:
```bash
ls ~/.claude/plugins/repos/mf-plugin/.claude-plugin/plugin.json
```

If the file exists, the plugin should be auto-discovered on next Claude Code session start.

**Step 2: Verify plugin loads**

Start a new Claude Code session from a different repo (e.g., `~/github/repos/likelihood.model`) and check if `/mf` is available in the skill list.

Run (manually — this is a human verification step):
```
# In a new Claude Code session from ~/github/repos/likelihood.model:
# Type /mf and verify it appears in autocomplete
```

---

### Task 7: Slim Down Metafunctor CLAUDE.md

**Files:**
- Modify: `~/github/repos/metafunctor/CLAUDE.md`

The metafunctor CLAUDE.md currently contains ~200 lines of site architecture documentation that now lives in the global plugin. Slim it down to essentials that are repo-specific (build commands, gotchas) and defer to `/mf` for the full model.

**Step 1: Read current CLAUDE.md**

Read `~/github/repos/metafunctor/CLAUDE.md` in full.

**Step 2: Edit to add deferral note**

Add near the top, after "Repository Overview":

```markdown
## Full Site Model

For the complete site architecture, taxonomy system, front matter patterns, and workflows, use the `/mf` skill (available globally). This CLAUDE.md covers repo-specific commands and gotchas.
```

**Step 3: Do NOT remove existing content yet**

Keep the current CLAUDE.md content intact for now. The deferral note is additive. We can slim it down further once we've verified the global plugin works in practice across multiple sessions.

**Step 4: Commit**

```bash
cd ~/github/repos/metafunctor
git add CLAUDE.md
git commit -m "Add deferral to /mf global skill in CLAUDE.md"
```

---

### Task 8: Remove Local mf/crier Skills from Metafunctor Repo

**Files:**
- Remove: `~/github/repos/metafunctor/.claude/skills/mf/` (3 files)
- Remove: `~/github/repos/metafunctor/.claude/skills/crier/` (1 file)

Now that the skills live globally, the local copies create confusion (Claude sees both).

**Step 1: Verify global plugin has all content**

Run:
```bash
ls ~/.claude/plugins/repos/mf-plugin/skills/mf/
ls ~/.claude/plugins/repos/mf-plugin/skills/crier/
```
Expected: SKILL.md, COMMANDS.md, WORKFLOWS.md in mf/; SKILL.md in crier/

**Step 2: Remove local skills**

```bash
cd ~/github/repos/metafunctor
git rm -r .claude/skills/mf/
git rm -r .claude/skills/crier/
```

**Step 3: Commit**

```bash
git commit -m "Remove local mf/crier skills — now in global plugin

Skills moved to ~/.claude/plugins/repos/mf-plugin/ for cross-repo access.
See /mf and /crier skills from any Claude Code session."
```

---

### Task 9: End-to-End Verification

**Step 1: Test from metafunctor repo**

Start a new Claude Code session in `~/github/repos/metafunctor`.

Verify:
- [ ] `/mf` skill is available (from global plugin)
- [ ] `/crier` skill is available (from global plugin)
- [ ] `mf papers list` works
- [ ] `crier audit` works
- [ ] CLAUDE.md loads and references `/mf`

**Step 2: Test from another repo**

Start a new Claude Code session in `~/github/repos/likelihood.model` (or any other repo).

Verify:
- [ ] `/mf` skill is available
- [ ] `/crier` skill is available
- [ ] Claude knows the metafunctor site architecture (ask "what content sections does metafunctor have?")
- [ ] Claude knows how to use mf commands (ask "how do I create a blog post about this project?")
- [ ] Claude suggests `MF_SITE_ROOT` or `cd` for cross-repo mf usage

**Step 3: Document any issues**

If the plugin doesn't load, check:
1. Plugin manifest at `~/.claude/plugins/repos/mf-plugin/.claude-plugin/plugin.json`
2. Skill files at `~/.claude/plugins/repos/mf-plugin/skills/*/SKILL.md`
3. Claude Code may need restart to discover new local plugins

---

## Future Phases (separate implementation plans)

### Phase 2: mf CLI — Global Config + Posts

- Add `~/.config/mf/config.yaml` with `site_root`
- Update `find_mf_root()` in `scripts/mf/src/mf/core/config.py` to check global config as fallback
- Create `scripts/mf/src/mf/posts/` module (commands.py, scaffolder.py)
- Implement `mf posts create/list/set/tag/feature`
- Add `content/post/` to SitePaths dataclass
- Tests in `scripts/mf/tests/test_posts/`

### Phase 3: mf CLI — Taxonomy + Health

- Create `scripts/mf/src/mf/taxonomy/` module
- Implement `mf taxonomy audit/normalize/orphans/stats`
- Create `scripts/mf/src/mf/health/` module
- Implement `mf health links/descriptions/images/stale/drafts`
- Tests in `scripts/mf/tests/test_taxonomy/` and `scripts/mf/tests/test_health/`

### Phase 4: Crier CLI Improvements

- Auto-derive `canonical_url` from file path + `site_base_url`
- Add `--project` / `--cwd` flag to override `.crier/` discovery
- Add `section` field to registry entries
- Implement `crier summary` command
- Optional: rewrite caching for `--auto-rewrite`

### Phase 5: Integration

- mf reads `.crier/registry.yaml` for `mf analytics summary`
- Enhanced `mf about` with auto-detection of current repo
- End-to-end: create post → deploy → cross-post from another repo
