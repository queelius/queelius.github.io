// Add navigation bar to standalone HTML documents
(function() {
  // Wait for DOM to be ready
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }

  function init() {
    // Detect if we're in a standalone HTML document (LaTeX-generated)
    // Check for LaTeXML or similar indicators
    const isStandaloneDoc = document.querySelector('link[href*="LaTeXML"]') ||
                           document.querySelector('link[href*="ltx-article"]') ||
                           window.location.pathname.includes('/html/index.html');

    if (!isStandaloneDoc) {
      console.log('Not a standalone document, navigation not added');
      return;
    }

    console.log('Standalone document detected, adding navigation');
    createNavigation();
  }

  function createNavigation() {

  // Create navigation bar
  const nav = document.createElement('div');
  nav.id = 'site-nav-overlay';
  nav.innerHTML = `
    <div class="nav-content">
      <a href="javascript:history.back()" class="back-btn">
        <svg width="16" height="16" viewBox="0 0 16 16" fill="currentColor">
          <path d="M11.5 1.5l-7 6.5 7 6.5" stroke="currentColor" stroke-width="2" fill="none"/>
        </svg>
        Back
      </a>
      <span class="nav-title">Viewing Document</span>
      <a href="${getBaseURL()}" class="home-link">Return to Site</a>
    </div>
  `;

  // Add styles
  const style = document.createElement('style');
  style.textContent = `
    #site-nav-overlay {
      position: fixed;
      top: 0;
      left: 0;
      right: 0;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      padding: 0.75rem 1.5rem;
      box-shadow: 0 2px 8px rgba(0,0,0,0.15);
      z-index: 10000;
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
    }

    #site-nav-overlay .nav-content {
      max-width: 1400px;
      margin: 0 auto;
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 1rem;
    }

    #site-nav-overlay .back-btn,
    #site-nav-overlay .home-link {
      display: inline-flex;
      align-items: center;
      gap: 0.5rem;
      padding: 0.4rem 0.9rem;
      background: rgba(255,255,255,0.2);
      color: white;
      text-decoration: none;
      border-radius: 6px;
      font-size: 0.9rem;
      font-weight: 500;
      transition: all 0.2s ease;
      border: 1px solid rgba(255,255,255,0.3);
    }

    #site-nav-overlay .back-btn:hover,
    #site-nav-overlay .home-link:hover {
      background: rgba(255,255,255,0.3);
      transform: translateY(-1px);
      box-shadow: 0 2px 8px rgba(0,0,0,0.2);
    }

    #site-nav-overlay .nav-title {
      font-size: 0.95rem;
      opacity: 0.9;
      flex: 1;
      text-align: center;
    }

    /* Adjust body to account for nav bar */
    body {
      margin-top: 60px !important;
    }

    @media (max-width: 768px) {
      #site-nav-overlay {
        padding: 0.6rem 1rem;
      }

      #site-nav-overlay .nav-content {
        flex-wrap: wrap;
      }

      #site-nav-overlay .nav-title {
        order: -1;
        flex-basis: 100%;
        text-align: left;
        font-size: 0.85rem;
        margin-bottom: 0.3rem;
      }

      #site-nav-overlay .back-btn,
      #site-nav-overlay .home-link {
        font-size: 0.85rem;
        padding: 0.35rem 0.7rem;
      }

      body {
        margin-top: 90px !important;
      }
    }
  `;

    // Helper function to get base URL
    function getBaseURL() {
      // Try to extract from current path
      const path = window.location.pathname;

      // Check if we're in a subdirectory like /metafunctor/
      if (path.includes('/metafunctor/')) {
        return '/metafunctor/';
      }

      // For local development (localhost)
      if (window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1') {
        // Extract base path from current location
        // If path is /metafunctor/publications/..., use /metafunctor/
        // If path is /publications/..., use /
        const match = path.match(/^(\/[^\/]+)?/);
        return match ? match[0] + '/' : '/';
      }

      // Default to root
      return '/';
    }

    // Insert nav and styles
    document.head.appendChild(style);
    document.body.insertBefore(nav, document.body.firstChild);

    console.log('Navigation bar added successfully');
  }
})();