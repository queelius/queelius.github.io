/**
 * Performance Optimizations for Metafunctor
 *
 * Merged from the former performance-optimizations.js and
 * performance-enhancements.js (2026-07). Those two files overlapped heavily
 * (both lazy-loaded the same img[loading="lazy"] elements, both animated
 * [data-animate] elements, both shipped a debounce() utility, and both ran
 * duplicate Web Vitals PerformanceObservers). This file keeps the union of
 * distinct behavior and removes the true duplicates. See git history for the
 * two originals.
 *
 * One thing that looked like a duplicate but was not: performance-
 * enhancements.js's initReadingProgress() built the .reading-progress
 * element used sitewide by static/js/modern-interactions.js's scroll
 * handler (loaded on every page via baseof.html). That is a different bar
 * from the post-only #reading-progress element created by the dedicated
 * static/js/reading-progress.js. Element creation is restored below
 * (initReadingProgressBar), without its own scroll handler, since
 * modern-interactions.js already owns one.
 */

(function() {
  'use strict';

  // ============================================
  // Resource Hints
  // ============================================

  function addResourceHints() {
    const hints = [
      { rel: 'dns-prefetch', href: 'https://www.google-analytics.com' },
      { rel: 'dns-prefetch', href: 'https://cdnjs.cloudflare.com' }
    ];

    hints.forEach(hint => {
      const link = document.createElement('link');
      link.rel = hint.rel;
      link.href = hint.href;
      document.head.appendChild(link);
    });
  }

  // ============================================
  // Critical Resource Preloading
  // ============================================

  /**
   * Preload hero images explicitly marked as critical via [data-critical-image].
   */
  function preloadCriticalImages() {
    document.querySelectorAll('[data-critical-image]').forEach(img => {
      const link = document.createElement('link');
      link.rel = 'preload';
      link.as = 'image';
      link.href = img.dataset.criticalImage || img.src;
      document.head.appendChild(link);
    });
  }

  // ============================================
  // Image Lazy Loading
  // ============================================

  /**
   * Swaps the placeholder src/srcset for the real data-src/data-srcset once
   * the browser is ready to fetch the image (native loading="lazy" support,
   * or an IntersectionObserver fallback for older browsers). Adds a "loaded"
   * class once the real image has actually finished loading, so CSS can hook
   * a blur-up or fade-in transition without racing the swap itself.
   */
  function lazyLoadImages() {
    const images = document.querySelectorAll('img[loading="lazy"]');

    function swap(img) {
      if (img.dataset.src) img.src = img.dataset.src;
      if (img.dataset.srcset) img.srcset = img.dataset.srcset;
      img.addEventListener('load', () => img.classList.add('loaded'), { once: true });
    }

    if ('loading' in HTMLImageElement.prototype) {
      images.forEach(swap);
    } else if ('IntersectionObserver' in window) {
      const imageObserver = new IntersectionObserver((entries, observer) => {
        entries.forEach(entry => {
          if (entry.isIntersecting) {
            swap(entry.target);
            observer.unobserve(entry.target);
          }
        });
      }, { rootMargin: '50px 0px', threshold: 0.01 });

      images.forEach(img => imageObserver.observe(img));
    } else {
      images.forEach(swap);
    }
  }

  // ============================================
  // Layout Shift Prevention
  // ============================================

  function preventLayoutShift() {
    document.querySelectorAll('img:not([width]):not([height])').forEach(img => {
      if (img.naturalWidth && img.naturalHeight) {
        img.style.aspectRatio = `${img.naturalWidth} / ${img.naturalHeight}`;
      }
    });
  }

  // ============================================
  // Scroll-Reveal Animations
  // ============================================

  /**
   * Reveals [data-animate] elements as they enter the viewport, staggering
   * any [data-stagger] children.
   */
  function initIntersectionAnimations() {
    if (!('IntersectionObserver' in window)) return;

    const animationObserver = new IntersectionObserver((entries) => {
      entries.forEach(entry => {
        if (entry.isIntersecting) {
          entry.target.classList.add('animate-in');

          const children = entry.target.querySelectorAll('[data-stagger]');
          children.forEach((child, index) => {
            setTimeout(() => child.classList.add('animate-in'), index * 100);
          });
        }
      });
    }, { rootMargin: '0px 0px -10% 0px', threshold: 0.1 });

    document.querySelectorAll('[data-animate]').forEach(el => animationObserver.observe(el));
  }

  // ============================================
  // Link Prefetching
  // ============================================

  /**
   * Prefetches same-origin links on hover/touch intent, and predictively
   * for links that scroll into view (primed 2s after DOM ready so it does
   * not compete with initial page load). A single Set dedupes across both
   * trigger paths.
   */
  function initLinkPrefetching() {
    const prefetchedUrls = new Set();

    function prefetchLink(url) {
      if (!url || prefetchedUrls.has(url)) return;
      const link = document.createElement('link');
      link.rel = 'prefetch';
      link.href = url;
      document.head.appendChild(link);
      prefetchedUrls.add(url);
    }

    document.addEventListener('mouseover', (e) => {
      const link = e.target.closest('a[href^="/"]');
      if (link) prefetchLink(link.href);
    }, { passive: true });

    document.addEventListener('touchstart', (e) => {
      const link = e.target.closest('a[href^="/"]');
      if (link) prefetchLink(link.href);
    }, { passive: true });

    if ('IntersectionObserver' in window) {
      const linkObserver = new IntersectionObserver((entries) => {
        entries.forEach(entry => {
          if (entry.isIntersecting) {
            prefetchLink(entry.target.getAttribute('href'));
          }
        });
      }, { rootMargin: '200px' });

      setTimeout(() => {
        document.querySelectorAll('a[href^="/"]').forEach(link => linkObserver.observe(link));
      }, 2000);
    }

    return prefetchLink;
  }

  // ============================================
  // Sitewide Reading Progress Bar (element creation only)
  // ============================================

  /**
   * Creates the .reading-progress element that modern-interactions.js's
   * updateReadingProgress() looks up on every scroll and resizes via
   * style.width. Styled by the .reading-progress rule in
   * assets/css/micro-interactions.css. This function only creates the
   * element, once, if it is not already present; it deliberately does not
   * attach a scroll listener of its own, since modern-interactions.js
   * already updates this element on scroll and a second handler here would
   * just do duplicate work on every scroll event.
   */
  function initReadingProgressBar() {
    if (document.querySelector('.reading-progress')) return;

    const progressBar = document.createElement('div');
    progressBar.className = 'reading-progress';
    progressBar.setAttribute('role', 'progressbar');
    progressBar.setAttribute('aria-label', 'Reading progress');
    progressBar.setAttribute('aria-valuemin', '0');
    progressBar.setAttribute('aria-valuemax', '100');
    document.body.appendChild(progressBar);
  }

  // ============================================
  // Scroll Progress Hook (opt-in via [data-scroll-progress])
  // ============================================

  function optimizeScrollPerformance() {
    let ticking = false;

    function updateScrollProgress() {
      const progressBar = document.querySelector('[data-scroll-progress]');
      if (!progressBar) return;

      const windowHeight = window.innerHeight;
      const documentHeight = document.documentElement.scrollHeight - windowHeight;
      const progress = (window.scrollY / documentHeight) * 100;
      progressBar.style.width = `${Math.min(progress, 100)}%`;
    }

    window.addEventListener('scroll', () => {
      if (!ticking) {
        window.requestAnimationFrame(() => {
          updateScrollProgress();
          ticking = false;
        });
        ticking = true;
      }
    }, { passive: true });
  }

  // ============================================
  // Deferred Third-Party Scripts (opt-in via [data-defer])
  // ============================================

  function deferThirdPartyScripts() {
    document.querySelectorAll('script[data-defer]').forEach(script => {
      const newScript = document.createElement('script');
      newScript.src = script.dataset.src;
      newScript.defer = true;
      if (script.dataset.async) newScript.async = true;
      document.body.appendChild(newScript);
    });
  }

  // ============================================
  // Non-Critical CSS (print-media swap trick)
  // ============================================

  function loadNonCriticalCSS() {
    document.querySelectorAll('link[rel="stylesheet"][media="print"]').forEach(link => {
      link.addEventListener('load', function() {
        this.media = 'all';
      });
    });
  }

  // ============================================
  // Font Loading
  // ============================================
  //
  // @font-face rules live in assets/css/fonts.css (self-hosted, font-display:
  // swap). This uses the Font Loading API to mark the document once the core
  // weights are ready, so CSS can key transitions off html.fonts-loaded.

  function optimizeFontLoading() {
    if (!('fonts' in document)) return;

    Promise.all([
      document.fonts.load('400 1em Inter'),
      document.fonts.load('600 1em Inter'),
      document.fonts.load('700 1em Inter')
    ]).then(() => {
      document.documentElement.classList.add('fonts-loaded');
    });
  }

  // ============================================
  // Service Worker Registration
  // ============================================
  //
  // Disabled until a static/sw.js exists; kept ready to enable.

  function registerServiceWorker() {
    if (!('serviceWorker' in navigator)) return;

    window.addEventListener('load', () => {
      navigator.serviceWorker
        .register('/sw.js')
        .then(() => {
          console.log('ServiceWorker registration successful');
        })
        .catch(err => {
          console.log('ServiceWorker registration failed: ', err);
        });
    });
  }

  // ============================================
  // Web Vitals Monitoring
  // ============================================
  //
  // One set of observers reports each metric to gtag (a no-op today; no
  // analytics script is loaded, this is a ready hook for when one is) and,
  // on localhost, also logs to the console for local debugging.

  function monitorWebVitals() {
    const isDev = window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1';

    function report(name, value) {
      if (typeof gtag !== 'undefined') {
        gtag('event', name, {
          event_category: 'Web Vitals',
          value: Math.round(name === 'CLS' ? value * 1000 : value),
          non_interaction: true
        });
      }
      if (isDev) console.log(name + ':', value);
    }

    if (!('PerformanceObserver' in window)) return;

    try {
      new PerformanceObserver((list) => {
        for (const entry of list.getEntries()) {
          report('FID', entry.processingStart - entry.startTime);
        }
      }).observe({ type: 'first-input', buffered: true });
    } catch (e) {
      // not supported
    }

    try {
      new PerformanceObserver((list) => {
        const entries = list.getEntries();
        const lastEntry = entries[entries.length - 1];
        report('LCP', lastEntry.renderTime || lastEntry.loadTime);
      }).observe({ type: 'largest-contentful-paint', buffered: true });
    } catch (e) {
      // not supported
    }

    try {
      let clsValue = 0;
      new PerformanceObserver((list) => {
        for (const entry of list.getEntries()) {
          if (!entry.hadRecentInput) clsValue += entry.value;
        }
      }).observe({ type: 'layout-shift', buffered: true });

      document.addEventListener('visibilitychange', () => {
        if (document.visibilityState === 'hidden') report('CLS', clsValue);
      });
    } catch (e) {
      // not supported
    }
  }

  // ============================================
  // Debounce / Throttle / Batched DOM Reads
  // ============================================

  function debounce(func, wait) {
    let timeout;
    return function executedFunction(...args) {
      clearTimeout(timeout);
      timeout = setTimeout(() => func(...args), wait);
    };
  }

  function throttle(func, limit) {
    let inThrottle;
    return function(...args) {
      if (!inThrottle) {
        func.apply(this, args);
        inThrottle = true;
        setTimeout(() => { inThrottle = false; }, limit);
      }
    };
  }

  function batchDOMReads() {
    let frameRequested = false;
    const domReads = [];

    return (callback) => {
      domReads.push(callback);

      if (!frameRequested) {
        frameRequested = true;
        requestAnimationFrame(() => {
          domReads.forEach(cb => cb());
          domReads.length = 0;
          frameRequested = false;
        });
      }
    };
  }

  // ============================================
  // Initialize
  // ============================================

  function init() {
    // Remove preload class to enable CSS transitions (see .preload rule in
    // assets/css/micro-interactions.css).
    document.documentElement.classList.remove('preload');

    // Run immediately.
    addResourceHints();
    preloadCriticalImages();

    const runDomReady = () => {
      initReadingProgressBar();
      lazyLoadImages();
      preventLayoutShift();
      initIntersectionAnimations();
      const prefetchLink = initLinkPrefetching();
      optimizeScrollPerformance();
      loadNonCriticalCSS();
      deferThirdPartyScripts();
      optimizeFontLoading();
      monitorWebVitals();

      window.performanceUtils = {
        debounce,
        throttle,
        batchDOMReads: batchDOMReads(),
        prefetchLink
      };
    };

    if (document.readyState === 'loading') {
      document.addEventListener('DOMContentLoaded', runDomReady);
    } else {
      runDomReady();
    }

    // Service worker registration: uncomment once static/sw.js exists.
    // window.addEventListener('load', registerServiceWorker);
  }

  init();
})();
