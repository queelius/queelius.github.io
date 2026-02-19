# sigmark — GPG Signing for Static Site Content

**Date:** 2026-02-17
**Status:** Approved
**Repo:** ~/github/repos/sigmark (new)

## Summary

`sigmark` is a standalone CLI tool that GPG-signs YAML-frontmatter markdown files.
It works with any static site generator that uses `---` YAML front matter (Hugo, Jekyll, Zola, Eleventy, etc.).

## Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Sign scope | Body only | Metadata changes (tags, featured) don't invalidate signature |
| Storage | Front matter field | Self-contained, travels with the file |
| GPG key | Default key | Override with `--key-id` if needed |
| Content scope | All .md files | Recursive scan, filterable with `--glob` |
| Implementation | GPG subprocess | No new Python deps, standard tooling, verifiable |
| Packaging | Standalone tool | Generic enough for any static site, not mf-specific |

## Commands

```
sigmark sign [PATH] [--key-id KEY] [--force] [--glob PATTERN]
sigmark verify [PATH] [--glob PATTERN]
sigmark status [PATH] [--json]
sigmark strip [PATH] [--glob PATTERN]
```

`PATH` defaults to current directory. Recursively finds all `.md` files with YAML front matter.

## Signing Flow

1. Walk directory tree, find `.md` files with `---` YAML front matter
2. For each file, extract the body (everything below the closing `---`)
3. Normalize: strip trailing whitespace per line, ensure single trailing newline
4. Pipe normalized body to `gpg --detach-sign --armor` via subprocess
5. Store in front matter via YAML round-trip:
   - `gpg_sig`: ASCII-armored detached signature
   - `gpg_sig_date`: ISO 8601 timestamp
   - `gpg_body_hash`: `sha256:<hex>` of the normalized body

## Verification Flow

1. Extract body, normalize identically to signing
2. Write normalized body to temp file
3. Write `gpg_sig` value to temp `.sig` file
4. Run `gpg --verify temp.sig temp_body`
5. Report pass/fail with signer identity from GPG output

## Staleness Detection

A signature is **stale** when the body has changed since signing:
- `sigmark status` computes SHA-256 of current normalized body
- Compares against stored `gpg_body_hash`
- Instant check without invoking GPG
- `sigmark verify` does full cryptographic verification

## Front Matter Example

```yaml
title: "My Post"
date: 2026-02-17
tags: ["cryptography"]
gpg_sig: |
  -----BEGIN PGP SIGNATURE-----
  iQIzBAABCAAdFiEE...
  ...
  -----END PGP SIGNATURE-----
gpg_sig_date: "2026-02-17T14:30:00Z"
gpg_body_hash: "sha256:a1b2c3d4e5f6..."
```

## Hugo Template Support

A Hugo partial for rendering a verification badge:

```html
<!-- layouts/partials/gpg-badge.html -->
{{ with .Params.gpg_sig }}
<div class="gpg-verified">
  <span>GPG Signed</span>
  {{ with $.Params.gpg_sig_date }}
    <time datetime="{{ . }}">{{ dateFormat "Jan 2, 2006" (time .) }}</time>
  {{ end }}
  <details>
    <summary>View Signature</summary>
    <pre><code>{{ . }}</code></pre>
  </details>
</div>
{{ end }}
```

## Project Structure

```
sigmark/
├── src/sigmark/
│   ├── __init__.py        # __version__
│   ├── cli.py             # Click CLI entry point
│   ├── signer.py          # GPG subprocess wrapper, sign/verify
│   ├── scanner.py         # Walk dirs, find .md files, parse front matter
│   └── frontmatter.py     # Read/write YAML front matter (lightweight)
├── hugo/
│   └── layouts/partials/
│       └── gpg-badge.html
├── tests/
│   ├── conftest.py
│   ├── test_signer.py
│   ├── test_scanner.py
│   ├── test_frontmatter.py
│   └── test_cli.py
├── pyproject.toml
├── LICENSE
└── README.md
```

## Dependencies

- `click>=8.0` — CLI framework
- `pyyaml>=6.0` — front matter parsing
- `rich>=13.0` — terminal output (tables, colors)

Runtime requirement: `gpg` binary on PATH.

## Future: mf Integration

Once sigmark is stable, mf can optionally wrap it:

```python
# src/mf/gpg/commands.py
import sigmark
# Thin wrapper delegating to sigmark with mf's site root
```

This keeps mf focused on metafunctor-specific concerns while reusing sigmark's generic logic.
