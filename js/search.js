// Search Functionality for Hugo Site

(function() {
  let searchIndex = null;
  let fuse = null;

  // Initialize search when DOM is ready
  document.addEventListener('DOMContentLoaded', function() {
    loadSearchIndex();
    setupSearchListeners();
  });

  // Load search index
  async function loadSearchIndex() {
    try {
      const response = await fetch('/index.json');
      searchIndex = await response.json();

      // Initialize Fuse.js for fuzzy search
      const fuseOptions = {
        keys: [
          { name: 'title', weight: 0.8 },
          { name: 'content', weight: 0.5 },
          { name: 'tags', weight: 0.3 },
          { name: 'categories', weight: 0.3 }
        ],
        threshold: 0.3,
        includeScore: true,
        includeMatches: true,
        minMatchCharLength: 2,
        useExtendedSearch: true
      };

      // Check if Fuse.js is available, if not use simple search
      if (typeof Fuse !== 'undefined') {
        fuse = new Fuse(searchIndex, fuseOptions);
      }
    } catch (error) {
      console.error('Failed to load search index:', error);
    }
  }

  // Load search from URL parameters
  function loadFromURL() {
    const params = new URLSearchParams(window.location.search);
    const searchInput = document.getElementById('search-input');

    if (params.has('q') && searchInput) {
      const query = params.get('q');
      searchInput.value = query;
      if (query.length >= 2) {
        performSearch(query);
      }
    }
  }

  // Update URL with search query
  function updateURL(query) {
    const params = new URLSearchParams(window.location.search);

    if (query && query.length >= 2) {
      params.set('q', query);
    } else {
      params.delete('q');
    }

    const newURL = params.toString()
      ? `${window.location.pathname}?${params.toString()}`
      : window.location.pathname;

    window.history.replaceState({}, '', newURL);
  }

  // Setup search event listeners
  function setupSearchListeners() {
    const searchInput = document.getElementById('search-input');
    const searchResults = document.getElementById('search-results');

    if (!searchInput || !searchResults) return;

    let debounceTimer;

    // Search on input with debouncing
    searchInput.addEventListener('input', function(e) {
      clearTimeout(debounceTimer);
      const query = e.target.value.trim();

      if (query.length < 2) {
        searchResults.innerHTML = '';
        updateURL('');
        return;
      }

      // Show loading state
      searchResults.innerHTML = '<div class="search-loading"><div class="loading-spinner"></div> Searching...</div>';

      debounceTimer = setTimeout(() => {
        performSearch(query);
        updateURL(query);
      }, 300);
    });

    // Load from URL on page load
    loadFromURL();

    // Handle keyboard navigation
    searchInput.addEventListener('keydown', function(e) {
      const results = searchResults.querySelectorAll('.search-result');
      const activeResult = searchResults.querySelector('.search-result.active');

      if (e.key === 'ArrowDown') {
        e.preventDefault();
        navigateResults(results, activeResult, 'next');
      } else if (e.key === 'ArrowUp') {
        e.preventDefault();
        navigateResults(results, activeResult, 'prev');
      } else if (e.key === 'Enter' && activeResult) {
        e.preventDefault();
        window.location.href = activeResult.dataset.url;
      }
    });

    // Keyboard shortcut to open search (Ctrl/Cmd + K)
    document.addEventListener('keydown', function(e) {
      if ((e.ctrlKey || e.metaKey) && e.key === 'k') {
        e.preventDefault();
        openSearch();
      }
    });
  }

  // Perform search
  function performSearch(query) {
    const searchResults = document.getElementById('search-results');

    if (!searchIndex) {
      searchResults.innerHTML = '<div class="search-error">Search index not loaded. Please try again.</div>';
      return;
    }

    let results;

    if (fuse) {
      // Use Fuse.js for fuzzy search
      results = fuse.search(query).map(result => ({
        item: result.item,
        score: result.score,
        matches: result.matches
      }));
    } else {
      // Fallback to simple search
      results = simpleSearch(query);
    }

    displayResults(results, query);
  }

  // Simple search fallback
  function simpleSearch(query) {
    const queryLower = query.toLowerCase();
    const results = [];

    searchIndex.forEach(item => {
      let score = 0;
      const titleMatch = item.title.toLowerCase().includes(queryLower);
      const contentMatch = item.content.toLowerCase().includes(queryLower);
      const tagMatch = item.tags && item.tags.some(tag => tag.toLowerCase().includes(queryLower));
      const categoryMatch = item.categories && item.categories.some(cat => cat.toLowerCase().includes(queryLower));

      if (titleMatch) score += 10;
      if (contentMatch) score += 5;
      if (tagMatch) score += 3;
      if (categoryMatch) score += 3;

      if (score > 0) {
        results.push({
          item: item,
          score: 1 - (score / 21), // Normalize score
          matches: []
        });
      }
    });

    return results.sort((a, b) => a.score - b.score).slice(0, 10);
  }

  // Display search results
  function displayResults(results, query) {
    const searchResults = document.getElementById('search-results');

    if (results.length === 0) {
      searchResults.innerHTML = `
        <div class="search-no-results">
          <i class="fas fa-search"></i>
          <p>No results found for "${escapeHtml(query)}"</p>
          <p class="search-suggestions">Try different keywords or check spelling</p>
        </div>
      `;
      return;
    }

    const resultsHtml = results.slice(0, 10).map((result, index) => {
      const item = result.item;
      const highlighted = highlightMatches(item, result.matches, query);

      return `
        <div class="search-result ${index === 0 ? 'active' : ''}" data-url="${item.permalink}">
          <div class="search-result-header">
            <h4 class="search-result-title">${highlighted.title}</h4>
            <span class="search-result-type">${item.section || 'Page'}</span>
          </div>
          <p class="search-result-excerpt">${highlighted.excerpt}</p>
          <div class="search-result-meta">
            ${item.date ? `<span><i class="fas fa-calendar"></i> ${formatDate(item.date)}</span>` : ''}
            ${item.tags && item.tags.length > 0 ? `
              <span class="search-result-tags">
                ${item.tags.slice(0, 3).map(tag => `<span class="mini-tag">${tag}</span>`).join('')}
              </span>
            ` : ''}
          </div>
        </div>
      `;
    }).join('');

    searchResults.innerHTML = resultsHtml;

    // Add click handlers to results
    searchResults.querySelectorAll('.search-result').forEach(result => {
      result.addEventListener('click', function() {
        window.location.href = this.dataset.url;
      });
    });
  }

  // Highlight search matches
  function highlightMatches(item, matches, query) {
    let title = item.title;
    let excerpt = item.content.substring(0, 200) + '...';

    if (matches && matches.length > 0) {
      matches.forEach(match => {
        if (match.key === 'title') {
          title = highlightText(title, match.value);
        } else if (match.key === 'content') {
          // Find relevant excerpt around match
          const matchIndex = item.content.toLowerCase().indexOf(match.value.toLowerCase());
          if (matchIndex > -1) {
            const start = Math.max(0, matchIndex - 50);
            const end = Math.min(item.content.length, matchIndex + 150);
            excerpt = '...' + item.content.substring(start, end) + '...';
            excerpt = highlightText(excerpt, match.value);
          }
        }
      });
    } else {
      // Simple highlight for fallback search
      title = highlightText(title, query);
      excerpt = highlightText(excerpt, query);
    }

    return { title, excerpt };
  }

  // Highlight text with query
  function highlightText(text, query) {
    const regex = new RegExp(`(${escapeRegExp(query)})`, 'gi');
    return text.replace(regex, '<mark class="search-highlight">$1</mark>');
  }

  // Navigate search results with keyboard
  function navigateResults(results, activeResult, direction) {
    if (results.length === 0) return;

    let index = -1;
    if (activeResult) {
      activeResult.classList.remove('active');
      index = Array.from(results).indexOf(activeResult);
    }

    if (direction === 'next') {
      index = (index + 1) % results.length;
    } else {
      index = index <= 0 ? results.length - 1 : index - 1;
    }

    results[index].classList.add('active');
    results[index].scrollIntoView({ behavior: 'smooth', block: 'nearest' });
  }

  // Open search modal
  function openSearch() {
    const searchModal = document.getElementById('search-modal');
    const searchInput = document.getElementById('search-input');

    if (searchModal && searchInput) {
      searchModal.classList.add('active');
      searchInput.focus();
    }
  }

  // Utility functions
  function escapeHtml(text) {
    const map = {
      '&': '&amp;',
      '<': '&lt;',
      '>': '&gt;',
      '"': '&quot;',
      "'": '&#039;'
    };
    return text.replace(/[&<>"']/g, m => map[m]);
  }

  function escapeRegExp(string) {
    return string.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  }

  function formatDate(dateString) {
    const date = new Date(dateString);
    return date.toLocaleDateString('en-US', {
      year: 'numeric',
      month: 'short',
      day: 'numeric'
    });
  }

  // Instant search widget
  class InstantSearch {
    constructor(container) {
      this.container = container;
      this.init();
    }

    init() {
      this.createWidget();
      this.attachListeners();
    }

    createWidget() {
      this.container.innerHTML = `
        <div class="instant-search-widget">
          <div class="instant-search-input-wrapper">
            <i class="fas fa-search"></i>
            <input type="text" class="instant-search-input" placeholder="Quick search...">
            <span class="instant-search-shortcut">⌘K</span>
          </div>
          <div class="instant-search-results"></div>
        </div>
      `;

      this.input = this.container.querySelector('.instant-search-input');
      this.results = this.container.querySelector('.instant-search-results');
    }

    attachListeners() {
      let debounceTimer;

      this.input.addEventListener('input', (e) => {
        clearTimeout(debounceTimer);
        const query = e.target.value.trim();

        if (query.length < 2) {
          this.results.style.display = 'none';
          return;
        }

        debounceTimer = setTimeout(() => {
          this.performQuickSearch(query);
        }, 200);
      });

      this.input.addEventListener('focus', () => {
        if (this.input.value.length >= 2) {
          this.results.style.display = 'block';
        }
      });

      document.addEventListener('click', (e) => {
        if (!this.container.contains(e.target)) {
          this.results.style.display = 'none';
        }
      });
    }

    performQuickSearch(query) {
      if (!searchIndex) return;

      let results;
      if (fuse) {
        results = fuse.search(query).slice(0, 5);
      } else {
        results = simpleSearch(query).slice(0, 5);
      }

      this.displayQuickResults(results, query);
    }

    displayQuickResults(results, query) {
      if (results.length === 0) {
        this.results.innerHTML = '<div class="instant-search-empty">No results found</div>';
      } else {
        const html = results.map(result => {
          const item = result.item;
          return `
            <a href="${item.permalink}" class="instant-search-item">
              <div class="instant-search-title">${highlightText(item.title, query)}</div>
              <div class="instant-search-meta">${item.section || 'Page'}</div>
            </a>
          `;
        }).join('');

        this.results.innerHTML = html;
      }

      this.results.style.display = 'block';
    }
  }

  // Initialize instant search widgets
  document.addEventListener('DOMContentLoaded', () => {
    const instantSearchContainers = document.querySelectorAll('.instant-search-container');
    instantSearchContainers.forEach(container => {
      new InstantSearch(container);
    });
  });

  // Export for use in other scripts
  window.siteSearch = {
    openSearch,
    performSearch,
    InstantSearch
  };
})();

// Add search widget styles
const searchStyles = document.createElement('style');
searchStyles.textContent = `
  .instant-search-widget {
    position: relative;
    width: 100%;
    max-width: 400px;
  }

  .instant-search-input-wrapper {
    position: relative;
    display: flex;
    align-items: center;
  }

  .instant-search-input-wrapper i {
    position: absolute;
    left: 12px;
    color: var(--text-tertiary);
  }

  .instant-search-input {
    width: 100%;
    padding: var(--space-sm) var(--space-md) var(--space-sm) 40px;
    border: 1px solid var(--border-color);
    border-radius: var(--radius-md);
    background: var(--bg-primary);
    color: var(--text-primary);
    font-size: 0.95rem;
    outline: none;
    transition: all var(--transition-base);
  }

  .instant-search-input:focus {
    border-color: var(--accent-purple);
    box-shadow: 0 0 0 3px rgba(99, 102, 241, 0.1);
  }

  .instant-search-shortcut {
    position: absolute;
    right: 12px;
    padding: 2px 6px;
    background: var(--bg-secondary);
    border-radius: var(--radius-sm);
    font-size: 0.75rem;
    color: var(--text-tertiary);
  }

  .instant-search-results {
    position: absolute;
    top: calc(100% + 8px);
    left: 0;
    right: 0;
    background: var(--bg-primary);
    border: 1px solid var(--border-color);
    border-radius: var(--radius-md);
    box-shadow: var(--shadow-lg);
    display: none;
    z-index: var(--z-dropdown);
    max-height: 400px;
    overflow-y: auto;
  }

  .instant-search-item {
    display: block;
    padding: var(--space-sm) var(--space-md);
    text-decoration: none;
    transition: background var(--transition-base);
    border-bottom: 1px solid var(--border-color);
  }

  .instant-search-item:last-child {
    border-bottom: none;
  }

  .instant-search-item:hover {
    background: var(--bg-secondary);
  }

  .instant-search-title {
    color: var(--text-primary);
    font-weight: 600;
    margin-bottom: 2px;
  }

  .instant-search-meta {
    color: var(--text-tertiary);
    font-size: 0.813rem;
  }

  .instant-search-empty {
    padding: var(--space-md);
    text-align: center;
    color: var(--text-tertiary);
  }

  .search-loading {
    display: flex;
    align-items: center;
    justify-content: center;
    padding: var(--space-xl);
    color: var(--text-tertiary);
  }

  .search-no-results {
    text-align: center;
    padding: var(--space-2xl);
    color: var(--text-tertiary);
  }

  .search-no-results i {
    font-size: 3rem;
    margin-bottom: var(--space-lg);
    opacity: 0.5;
  }

  .search-suggestions {
    font-size: 0.875rem;
    margin-top: var(--space-sm);
  }

  .mini-tag {
    display: inline-block;
    padding: 2px 6px;
    background: var(--bg-secondary);
    border-radius: var(--radius-sm);
    font-size: 0.75rem;
    margin-left: 4px;
  }

  mark.search-highlight {
    background: rgba(99, 102, 241, 0.2);
    color: var(--accent-purple);
    padding: 0 2px;
    border-radius: 2px;
  }
`;

document.head.appendChild(searchStyles);