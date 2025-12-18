/**
 * Performance Optimizations for Metafunctor
 * Handles critical CSS inlining, resource hints, and Web Vitals optimization
 */

(function() {
  'use strict';

  // ============================================
  // Critical Resource Loading
  // ============================================

  /**
   * Preload critical assets for better LCP (Largest Contentful Paint)
   */
  function preloadCriticalAssets() {
    // Preload hero images if they exist
    const heroImages = document.querySelectorAll('[data-critical-image]');
    heroImages.forEach(img => {
      const link = document.createElement('link');
      link.rel = 'preload';
      link.as = 'image';
      link.href = img.dataset.criticalImage || img.src;
      document.head.appendChild(link);
    });
  }

  // ============================================
  // Image Lazy Loading with Blur-up Effect
  // ============================================

  /**
   * Implements progressive image loading with blur effect
   */
  function initProgressiveImageLoading() {
    if ('IntersectionObserver' in window) {
      const imageObserver = new IntersectionObserver(
        (entries, observer) => {
          entries.forEach(entry => {
            if (entry.isIntersecting) {
              const img = entry.target;
              loadImage(img);
              observer.unobserve(img);
            }
          });
        },
        {
          rootMargin: '50px 0px',
          threshold: 0.01
        }
      );

      // Observe all lazy images
      document.querySelectorAll('img[loading="lazy"]').forEach(img => {
        imageObserver.observe(img);
      });
    }
  }

  function loadImage(img) {
    const src = img.dataset.src || img.src;

    // Create temporary image to load in background
    const tempImg = new Image();
    tempImg.onload = function() {
      img.src = src;
      img.classList.add('loaded');
      img.style.filter = 'none';
    };
    tempImg.src = src;
  }

  // ============================================
  // Font Loading Optimization
  // ============================================

  /**
   * Use Font Loading API for better control over font display
   */
  function optimizeFontLoading() {
    if ('fonts' in document) {
      // Define font families to load
      const fonts = [
        new FontFace('Inter', 'url(https://fonts.gstatic.com/s/inter/v12/UcC73FwrK3iLTeHuS_fvQtMwCp50KnMa1ZL7.woff2)', {
          weight: '400',
          style: 'normal',
          display: 'swap'
        }),
        new FontFace('Inter', 'url(https://fonts.gstatic.com/s/inter/v12/UcC73FwrK3iLTeHuS_fvQtMwCp50KnMa25L7.woff2)', {
          weight: '700',
          style: 'normal',
          display: 'swap'
        })
      ];

      // Load fonts
      fonts.forEach(font => {
        document.fonts.add(font);
        font.load();
      });
    }
  }

  // ============================================
  // Intersection Observer for Animations
  // ============================================

  /**
   * Trigger animations only when elements are in viewport
   */
  function initIntersectionAnimations() {
    if ('IntersectionObserver' in window) {
      const animationObserver = new IntersectionObserver(
        (entries) => {
          entries.forEach(entry => {
            if (entry.isIntersecting) {
              entry.target.classList.add('animate-in');

              // Stagger child animations
              const children = entry.target.querySelectorAll('[data-stagger]');
              children.forEach((child, index) => {
                setTimeout(() => {
                  child.classList.add('animate-in');
                }, index * 100);
              });
            }
          });
        },
        {
          rootMargin: '0px 0px -10% 0px',
          threshold: 0.1
        }
      );

      // Observe elements with animation classes
      document.querySelectorAll('[data-animate]').forEach(el => {
        animationObserver.observe(el);
      });
    }
  }

  // ============================================
  // Prefetch Links on Hover
  // ============================================

  /**
   * Prefetch pages when user hovers over links
   */
  function initLinkPrefetching() {
    const prefetchedLinks = new Set();

    function prefetchLink(url) {
      if (prefetchedLinks.has(url)) return;

      const link = document.createElement('link');
      link.rel = 'prefetch';
      link.href = url;
      document.head.appendChild(link);
      prefetchedLinks.add(url);
    }

    // Prefetch on hover
    document.addEventListener('mouseover', (e) => {
      const link = e.target.closest('a[href^="/"]');
      if (link) {
        prefetchLink(link.href);
      }
    }, { passive: true });

    // Prefetch on touch start (for mobile)
    document.addEventListener('touchstart', (e) => {
      const link = e.target.closest('a[href^="/"]');
      if (link) {
        prefetchLink(link.href);
      }
    }, { passive: true });
  }

  // ============================================
  // Reduce Layout Shift (CLS Optimization)
  // ============================================

  /**
   * Add aspect ratio boxes to prevent layout shift
   */
  function preventLayoutShift() {
    // Add aspect ratio to images without dimensions
    document.querySelectorAll('img:not([width]):not([height])').forEach(img => {
      if (img.naturalWidth && img.naturalHeight) {
        const aspectRatio = img.naturalHeight / img.naturalWidth;
        img.style.aspectRatio = `${img.naturalWidth} / ${img.naturalHeight}`;
      }
    });
  }

  // ============================================
  // Service Worker Registration
  // ============================================

  /**
   * Register service worker for offline support and caching
   */
  function registerServiceWorker() {
    if ('serviceWorker' in navigator) {
      window.addEventListener('load', () => {
        navigator.serviceWorker
          .register('/sw.js')
          .then(registration => {
            console.log('ServiceWorker registration successful');
          })
          .catch(err => {
            console.log('ServiceWorker registration failed: ', err);
          });
      });
    }
  }

  // ============================================
  // Web Vitals Monitoring
  // ============================================

  /**
   * Monitor Core Web Vitals and report to analytics
   */
  function monitorWebVitals() {
    // Helper to send metrics to analytics
    function sendToAnalytics(metric) {
      if (typeof gtag !== 'undefined') {
        gtag('event', metric.name, {
          event_category: 'Web Vitals',
          value: Math.round(metric.name === 'CLS' ? metric.value * 1000 : metric.value),
          event_label: metric.id,
          non_interaction: true,
        });
      }
    }

    // Measure FID (First Input Delay)
    if ('PerformanceObserver' in window) {
      try {
        const fidObserver = new PerformanceObserver((list) => {
          for (const entry of list.getEntries()) {
            const fid = entry.processingStart - entry.startTime;
            sendToAnalytics({
              name: 'FID',
              value: fid,
              id: 'fid-' + Date.now()
            });
          }
        });

        fidObserver.observe({ type: 'first-input', buffered: true });
      } catch (e) {
        // PerformanceObserver not supported
      }
    }

    // Measure LCP (Largest Contentful Paint)
    if ('PerformanceObserver' in window) {
      try {
        const lcpObserver = new PerformanceObserver((list) => {
          const entries = list.getEntries();
          const lastEntry = entries[entries.length - 1];

          sendToAnalytics({
            name: 'LCP',
            value: lastEntry.renderTime || lastEntry.loadTime,
            id: 'lcp-' + Date.now()
          });
        });

        lcpObserver.observe({ type: 'largest-contentful-paint', buffered: true });
      } catch (e) {
        // PerformanceObserver not supported
      }
    }

    // Measure CLS (Cumulative Layout Shift)
    if ('PerformanceObserver' in window) {
      try {
        let clsValue = 0;
        let clsEntries = [];

        const clsObserver = new PerformanceObserver((list) => {
          for (const entry of list.getEntries()) {
            if (!entry.hadRecentInput) {
              clsValue += entry.value;
              clsEntries.push(entry);
            }
          }
        });

        clsObserver.observe({ type: 'layout-shift', buffered: true });

        // Report CLS when page is hidden
        document.addEventListener('visibilitychange', () => {
          if (document.visibilityState === 'hidden') {
            sendToAnalytics({
              name: 'CLS',
              value: clsValue,
              id: 'cls-' + Date.now()
            });
          }
        });
      } catch (e) {
        // PerformanceObserver not supported
      }
    }
  }

  // ============================================
  // Debounce and Throttle Utilities
  // ============================================

  function debounce(func, wait) {
    let timeout;
    return function executedFunction(...args) {
      const later = () => {
        clearTimeout(timeout);
        func(...args);
      };
      clearTimeout(timeout);
      timeout = setTimeout(later, wait);
    };
  }

  function throttle(func, limit) {
    let inThrottle;
    return function(...args) {
      if (!inThrottle) {
        func.apply(this, args);
        inThrottle = true;
        setTimeout(() => inThrottle = false, limit);
      }
    };
  }

  // ============================================
  // Optimize Scroll Performance
  // ============================================

  /**
   * Use passive event listeners for scroll events
   */
  function optimizeScrollPerformance() {
    let ticking = false;

    const scrollHandler = () => {
      if (!ticking) {
        window.requestAnimationFrame(() => {
          // Perform scroll-based updates here
          updateScrollProgress();
          ticking = false;
        });
        ticking = true;
      }
    };

    window.addEventListener('scroll', scrollHandler, { passive: true });
  }

  function updateScrollProgress() {
    const progressBar = document.querySelector('[data-scroll-progress]');
    if (!progressBar) return;

    const windowHeight = window.innerHeight;
    const documentHeight = document.documentElement.scrollHeight - windowHeight;
    const scrolled = window.scrollY;
    const progress = (scrolled / documentHeight) * 100;

    progressBar.style.width = `${Math.min(progress, 100)}%`;
  }

  // ============================================
  // Resource Hints
  // ============================================

  /**
   * Add DNS prefetch and preconnect hints
   */
  function addResourceHints() {
    const hints = [
      { rel: 'preconnect', href: 'https://fonts.googleapis.com' },
      { rel: 'preconnect', href: 'https://fonts.gstatic.com', crossorigin: 'anonymous' },
      { rel: 'dns-prefetch', href: 'https://www.google-analytics.com' },
      { rel: 'dns-prefetch', href: 'https://cdnjs.cloudflare.com' }
    ];

    hints.forEach(hint => {
      const link = document.createElement('link');
      link.rel = hint.rel;
      link.href = hint.href;
      if (hint.crossorigin) {
        link.crossOrigin = hint.crossorigin;
      }
      document.head.appendChild(link);
    });
  }

  // ============================================
  // Initialize All Optimizations
  // ============================================

  function init() {
    // Run immediately
    addResourceHints();
    preloadCriticalAssets();

    // Run when DOM is ready
    if (document.readyState === 'loading') {
      document.addEventListener('DOMContentLoaded', () => {
        initProgressiveImageLoading();
        initIntersectionAnimations();
        initLinkPrefetching();
        preventLayoutShift();
        optimizeScrollPerformance();
        monitorWebVitals();
      });
    } else {
      initProgressiveImageLoading();
      initIntersectionAnimations();
      initLinkPrefetching();
      preventLayoutShift();
      optimizeScrollPerformance();
      monitorWebVitals();
    }

    // Run after page load
    window.addEventListener('load', () => {
      optimizeFontLoading();
      // registerServiceWorker(); // Uncomment when service worker is ready
    });
  }

  // Start optimization
  init();

  // Export utilities for use in other scripts
  window.performanceUtils = {
    debounce,
    throttle,
    prefetchLink: (url) => prefetchLink(url)
  };

})();