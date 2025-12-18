// Anchor Links for Headings
// Adds clickable anchor links to all headings for easy deep linking

(function() {
  'use strict';

  // Add styles for anchor links
  const style = document.createElement('style');
  style.textContent = `
    .heading-anchor {
      position: relative;
    }

    .heading-anchor-link {
      position: absolute;
      left: -1.5em;
      padding-right: 0.5em;
      opacity: 0;
      color: var(--accent, #667eea);
      text-decoration: none;
      font-weight: 400;
      transition: opacity 0.2s ease;
      user-select: none;
    }

    .heading-anchor:hover .heading-anchor-link,
    .heading-anchor-link:focus {
      opacity: 1;
    }

    .heading-anchor-link:hover {
      color: var(--accent-light, #7c8aed);
    }

    .heading-anchor-link svg {
      width: 0.85em;
      height: 0.85em;
      fill: currentColor;
      vertical-align: middle;
    }

    .heading-anchor-link.copied {
      color: #10b981;
    }

    /* Mobile: Show on the right side instead */
    @media (max-width: 768px) {
      .heading-anchor-link {
        position: static;
        margin-left: 0.5em;
        opacity: 0.6;
      }

      .heading-anchor:hover .heading-anchor-link {
        opacity: 1;
      }
    }

    @media (prefers-reduced-motion: reduce) {
      .heading-anchor-link {
        transition: none;
      }
    }
  `;
  document.head.appendChild(style);

  // Link icon SVG
  const linkIcon = `<svg viewBox="0 0 16 16" aria-hidden="true"><path d="M7.775 3.275a.75.75 0 001.06 1.06l1.25-1.25a2 2 0 112.83 2.83l-2.5 2.5a2 2 0 01-2.83 0 .75.75 0 00-1.06 1.06 3.5 3.5 0 004.95 0l2.5-2.5a3.5 3.5 0 00-4.95-4.95l-1.25 1.25zm-4.69 9.64a2 2 0 010-2.83l2.5-2.5a2 2 0 012.83 0 .75.75 0 001.06-1.06 3.5 3.5 0 00-4.95 0l-2.5 2.5a3.5 3.5 0 004.95 4.95l1.25-1.25a.75.75 0 00-1.06-1.06l-1.25 1.25a2 2 0 01-2.83 0z"></path></svg>`;

  // Find all headings in the main content
  function addAnchorLinks() {
    const article = document.querySelector('article') || document.querySelector('main');
    if (!article) return;

    const headings = article.querySelectorAll('h2, h3, h4, h5, h6');

    headings.forEach(function(heading) {
      // Skip if already has anchor link
      if (heading.querySelector('.heading-anchor-link')) return;

      // Get or create ID for heading
      let headingId = heading.id;
      if (!headingId) {
        // Generate ID from heading text
        headingId = heading.textContent
          .trim()
          .toLowerCase()
          .replace(/[^\w\s-]/g, '')
          .replace(/\s+/g, '-');
        heading.id = headingId;
      }

      // Wrap heading content
      heading.classList.add('heading-anchor');

      // Create anchor link
      const anchor = document.createElement('a');
      anchor.className = 'heading-anchor-link';
      anchor.href = '#' + headingId;
      anchor.innerHTML = linkIcon;
      anchor.setAttribute('aria-label', 'Link to this heading');
      anchor.setAttribute('title', 'Copy link to this heading');

      // Add click handler to copy link
      anchor.addEventListener('click', function(e) {
        e.preventDefault();
        const url = window.location.origin + window.location.pathname + '#' + headingId;

        // Copy to clipboard
        navigator.clipboard.writeText(url).then(function() {
          // Show success feedback
          anchor.classList.add('copied');
          const originalTitle = anchor.getAttribute('title');
          anchor.setAttribute('title', 'Link copied!');

          // Reset after 2 seconds
          setTimeout(function() {
            anchor.classList.remove('copied');
            anchor.setAttribute('title', originalTitle);
          }, 2000);

          // Update URL without scrolling
          history.pushState(null, null, '#' + headingId);
        }).catch(function(err) {
          console.error('Failed to copy link:', err);
          // Fallback: just navigate to the anchor
          window.location.hash = headingId;
        });
      });

      // Insert anchor link
      heading.insertBefore(anchor, heading.firstChild);
    });
  }

  // Add anchor links when DOM is ready
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', addAnchorLinks);
  } else {
    addAnchorLinks();
  }

  // Smooth scroll to anchor on page load
  if (window.location.hash) {
    setTimeout(function() {
      const target = document.querySelector(window.location.hash);
      if (target) {
        target.scrollIntoView({ behavior: 'smooth', block: 'start' });
      }
    }, 100);
  }
})();
