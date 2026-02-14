# Design: Global mf Plugin with Crier Integration

**Date:** 2026-02-13
**Status:** Approved
**Scope:** Global Claude Code plugin for metafunctor site management + cross-posting

## Problem

The mf CLI and crier CLI are powerful tools for managing metafunctor.com, but their Claude Code skills only work when inside the metafunctor repo. The user works across 100+ repos and frequently needs to:

- Create blog posts about work done in the current repo
- Check what content exists about a project
- Sync project metadata from any directory
- Cross-post new content to social platforms

Additionally, the mf skill is CLI-reference-only and doesn't encode the site's content architecture, taxonomy conventions, or deployment model. Claude Code sessions in the metafunctor repo rely on CLAUDE.md for this knowledge, but sessions in other repos have no access to it.

## Design Principles

1. **Tools are dumb pipes, Claude Code is the brain.** mf and crier provide deterministic operations (CRUD, API calls, scaffolding). Claude provides judgment (what to post, how to rewrite, what's stale).

2. **Hugo is the source of truth for posts.** No posts_db.json. mf provides a convenience layer (scaffold, query, field ops) over Hugo content files. Databases exist only for content whose ground truth lives elsewhere (projects → GitHub, papers → LaTeX).

3. **mf reads crier, never controls it.** mf can read `.crier/registry.yaml` for analytics. All publishing delegates to crier. The skill teaches Claude the full pipeline.

4. **Absolute paths from any directory.** Both mf and crier resolve all relative paths against a configured `site_root`, not `$PWD`.

## Plugin Structure

```
~/.claude/plugins/repos/mf-plugin/
├── plugin.json              # Plugin manifest
└── skills/
    ├── mf/
    │   ├── SKILL.md         # Site model + CLI + workflows
    │   ├── COMMANDS.md      # mf command reference
    │   └── WORKFLOWS.md     # Common multi-step workflows
    └── crier/
        └── SKILL.md         # Cross-posting workflow (improved)
```

Installed as a user-scope plugin. Available in every Claude Code session regardless of working directory.

## mf Skill Content

### Section 1: Site Architecture (new)

Encodes the complete metafunctor site mental model:

- **Content sections:** post/, papers/, projects/, series/, writing/, medical/, research/, probsets/, publications/, media/
- **Taxonomy system:** tags, categories, genres, series, linked_project (and the `linked_project` vs `projects` gotcha)
- **Front matter patterns** per content type (posts, papers, projects, series, writing)
- **URL conventions:** date-prefixed dirs for posts, slug dirs for papers/projects
- **Bundle types:** leaf (index.md) vs branch (_index.md) and when each is used
- **Deployment model:** Hugo builds to docs/, committed to master, served by GitHub Pages
- **relativeURLs=true** — one build works on both metafunctor.com and queelius.github.io/metafunctor

### Section 2: mf CLI Reference (updated)

Existing commands (papers, projects, series, publications, content, analytics, integrity, backup, config) plus new commands:

#### Posts Management (new)

Hugo is the source of truth. mf provides convenience over front matter.

```bash
mf posts create --title "..." [--tags ...] [--categories ...] [--series ...]
mf posts list [--tag X] [--series X] [--since 30d]
mf posts set <slug> <field> <value>       # Edit front matter in-place
mf posts tag <slug> --add <tag>           # Manage tags
mf posts feature <slug>                   # Toggle featured
```

`mf posts create` scaffolds the correct directory structure (`YYYY-MM-DD-slug/index.md`), populates front matter from conventions (including `canonical_url` derived from slug + `site_base_url`), and works from any directory (writes to `{site_root}/content/post/`).

`mf posts list` reads front matter directly from Hugo content files. No database.

#### Taxonomy Hygiene (new)

```bash
mf taxonomy audit                         # Find near-duplicates, case mismatches
mf taxonomy normalize                     # Merge/rename across all content
mf taxonomy orphans                       # Tags used by only 1 post
mf taxonomy stats                         # Frequency, co-occurrence
```

#### Content Health (new)

```bash
mf health links                           # Broken internal links
mf health descriptions                    # Posts missing description
mf health images                          # Missing featured_image, oversized
mf health stale                           # Projects where GitHub desc diverged
mf health drafts                          # List drafts with age
```

#### Cross-repo `mf about` (enhanced)

```bash
mf about                                  # Auto-detect current repo, find related content
mf about --since 30d                      # What changed recently
mf about --gaps                           # "Project page exists but no blog post"
mf about <project-slug>                   # Explicit project lookup
```

#### Analytics with Distribution Data (enhanced)

```bash
mf analytics summary                      # Now includes crier distribution counts
mf analytics gaps                         # Now shows "not cross-posted" alongside "no blog post"
```

Reads `.crier/registry.yaml` (read-only) to correlate content with distribution state.

### Section 3: Cross-Repo Usage (new)

```bash
# mf knows where metafunctor lives
mf config show site_root                  # /home/spinoza/github/repos/metafunctor

# All commands work from any directory
cd ~/github/repos/likelihood.model
mf posts create --title "New release"     # Creates in metafunctor/content/post/
mf about                                  # Detects "likelihood.model", shows related content
```

Configuration in `~/.config/mf/config.yaml`:

```yaml
site_root: /home/spinoza/github/repos/metafunctor
```

### Section 4: mf → Crier Pipeline (new)

Documents the handoff between tools:

```
mf (create/update content) → Hugo (render) → crier (distribute)
```

After content operations, mf suggests crier next steps:

```
$ mf posts create --title "New likelihood.model release"
Created: content/post/2026-02-13-likelihood-model-release/index.md

  Next steps:
  → Edit the post
  → Cross-post: crier publish content/post/2026-02-13-likelihood-model-release/index.md
```

Defers to `/crier` skill for detailed cross-posting workflow.

## Crier Skill Content (improved)

### Preserved

Everything in the current skill: platform reference table, dialogue examples, workflow decision tree, bulk operations, configuration reference, important rules.

### New: Claude as Primary Rewriter

```markdown
## Writing Rewrites

You ARE the rewriter. Don't use --auto-rewrite in interactive sessions.

Rewrite guidelines by platform:

| Platform | Limit | Voice | Format |
|----------|-------|-------|--------|
| Bluesky  | 300   | Conversational, personal | Hook + key insight. No hashtags. |
| Mastodon | 500   | Slightly more detailed   | Can include 2-3 hashtags at end. |
| Twitter  | 280   | Punchy, provocative      | Can thread for longer content. |
| Threads  | 500   | Casual, exploratory      | Like talking to a friend. |
| LinkedIn | ∞     | Professional, structured | 3-4 paragraphs, accomplishment-framed. |

Rewrite process:
1. Read the full post
2. Identify the single most interesting/surprising insight
3. Write the rewrite around that insight, not a generic summary
4. Show the user before posting — they may want to adjust tone
5. Pass to crier via --rewrite

Anti-patterns:
- "New blog post: [title]" — boring, no hook
- "I wrote about X" — self-referential
- "Check out my latest..." — generic call-to-action
- Summarizing the whole post — pick ONE angle

Good rewrites lead with the insight:
- "TIL Bloom filters can give false positives but never false negatives —
   and the math behind why is beautiful."
- "What if the problem isn't the algorithm but the loss function?"
```

### New: Rewrite Decision Matrix

```markdown
| Situation | Use |
|-----------|-----|
| Interactive Claude session | Claude writes rewrite → --rewrite |
| Batch/bulk posting (10+ items) | --auto-rewrite for throughput |
| User says "just post it" | --auto-rewrite as shortcut |
| User cares about quality | Claude writes it, shows for approval |
```

### New: Cross-Repo Awareness

```markdown
## Running from Other Repos

Crier's .crier/ config lives in the metafunctor repo. From other directories:

    crier --project ~/github/repos/metafunctor audit

All crier commands accept --project to specify the site root.
```

### New: Status Checking

```markdown
## Checking Publication Status

Before cross-posting, check what's already out there:

    crier status content/post/2026-02-13-slug/index.md
    crier summary    # Site-wide overview, no API calls
```

## Crier CLI Changes (non-breaking)

Separate PRs to the crier repo. None required for the plugin to work, but improve the experience.

| # | Change | Effort | Value | Details |
|---|--------|--------|-------|---------|
| 1 | Auto-derive `canonical_url` | Small | High | Infer from file path + `site_base_url` when missing from front matter. Log warning. Fallback only — explicit front matter takes priority. |
| 2 | `--project` / `--cwd` flag | Small | High | Override .crier/ discovery. Resolve config/registry relative to specified directory instead of walking up from $PWD. |
| 3 | Section tracking in registry | Small | Medium | Add `section` field derived from file path (e.g., `post`, `papers`, `projects`). New entries only, backward compatible. |
| 4 | `crier summary` command | Small | Medium | Registry-only stats. No API calls. Counts per platform, identifies unposted content. |
| 5 | Rewrite caching | Medium | Low | Cache `--auto-rewrite` output by `(content_hash, char_limit)`. Claude path unaffected. |

## mf CLI Changes

| # | Change | Effort | Priority | Details |
|---|--------|--------|----------|---------|
| 1 | Global config with `site_root` | Small | Critical | `~/.config/mf/config.yaml` with `site_root` path. All relative paths resolve against it. Prerequisite for cross-repo. |
| 2 | `mf posts create/list/set` | Medium | High | Scaffold, query, field ops over Hugo content. No database. |
| 3 | `mf taxonomy audit/normalize/orphans` | Medium | High | Read all front matter, find duplicates, normalize. |
| 4 | `mf health links/descriptions/stale` | Medium | Medium | Content quality checks. |
| 5 | Read crier registry for analytics | Small | Medium | Parse `.crier/registry.yaml`, add distribution data to `mf analytics`. |
| 6 | Enhanced `mf about` | Small | Medium | Auto-detect current repo name, cross-reference with content. |

## What We're NOT Doing

- **No `posts_db.json`** — Hugo content files are the source of truth for posts
- **No SSG abstraction** — mf is coupled to Hugo, that's a feature not a bug
- **No bidirectional platform sync** — comments/engagement don't flow back
- **No crier changes required for plugin** — skill-only improvements work immediately
- **No engagement analytics** — platform dashboards already provide this
- **No content scheduling** — overkill for a personal blog
- **No auto-generation of related posts** — Hugo's taxonomy intersection handles this

## Migration Plan

### Phase 1: Global Plugin (skill-only, no code changes)

1. Create `~/.claude/plugins/repos/mf-plugin/` with plugin.json
2. Write enriched mf skill (site model + CLI reference + workflows)
3. Write improved crier skill (rewrite guidelines + cross-repo + status)
4. Verify plugin loads in Claude Code sessions from other repos
5. Slim down metafunctor CLAUDE.md to defer to the skill

### Phase 2: mf CLI — Global Config + Posts

1. Add `~/.config/mf/config.yaml` with `site_root`
2. Update all mf commands to resolve paths against `site_root`
3. Implement `mf posts create/list/set/tag/feature`
4. Update skill with new commands

### Phase 3: mf CLI — Taxonomy + Health

1. Implement `mf taxonomy audit/normalize/orphans/stats`
2. Implement `mf health links/descriptions/stale/drafts`
3. Update skill with new commands

### Phase 4: Crier CLI Improvements

1. Auto-derive `canonical_url`
2. `--project` flag
3. Section tracking in registry
4. `crier summary` command
5. Update skill with new capabilities

### Phase 5: Integration

1. mf reads crier registry for analytics
2. Enhanced `mf about` with auto-detection
3. End-to-end testing: create post from other repo → cross-post → verify

## Success Criteria

- From `~/github/repos/likelihood.model`, can run `mf posts create --title "..."` and it scaffolds in metafunctor
- From any repo, `/mf` skill is available and Claude knows the full site model
- From any repo, `/crier` skill is available and Claude can orchestrate cross-posting
- `mf analytics summary` shows distribution data from crier
- `mf taxonomy audit` catches near-duplicate tags
- `mf health links` catches broken internal links
- CLAUDE.md in metafunctor is lean (defers to global skill for details)
