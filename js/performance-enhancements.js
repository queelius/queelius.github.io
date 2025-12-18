/**
 * Performance Enhancements for Metafunctor
 * Implements lazy loading, critical resource hints, and optimizations
 */

(function() {
  'use strict';

  // ===================================
  // LAZY LOADING IMAGES
  // ===================================

  const lazyLoadImages = () => {
    const images = document.querySelectorAll('img[loading="lazy"]');

    if ('loading' in HTMLImageElement.prototype) {
      // Browser supports native lazy loading
      images.forEach(img => {
        if (img.dataset.src) {
          img.src = img.dataset.src;
        }
        if (img.dataset.srcset) {
          img.srcset = img.dataset.srcset;
        }
      });
    } else {
      // Fallback to Intersection Observer
      const imageObserver = new IntersectionObserver((entries, observer) => {
        entries.forEach(entry => {
          if (entry.isIntersecting) {
            const img = entry.target;
            if (img.dataset.src) {
              img.src = img.dataset.src;
            }
            if (img.dataset.srcset) {
              img.srcset = img.dataset.srcset;
            }
            img.classList.add('loaded');
            observer.unobserve(img);
          }
        });
      }, {
        rootMargin: '50px 0px',
        threshold: 0.01
      });

      images.forEach(img => imageObserver.observe(img));
    }
  };

  // ===================================
  // READING PROGRESS INDICATOR
  // ===================================

  const initReadingProgress = () => {
    const progressBar = document.createElement('div');
    progressBar.className = 'reading-progress';
    progressBar.setAttribute('role', 'progressbar');
    progressBar.setAttribute('aria-label', 'Reading progress');
    progressBar.setAttribute('aria-valuemin', '0');
    progressBar.setAttribute('aria-valuemax', '100');
    document.body.appendChild(progressBar);

    let ticking = false;

    const updateProgress = () => {
      const winScroll = document.documentElement.scrollTop || document.body.scrollTop;
      const height = document.documentElement.scrollHeight - document.documentElement.clientHeight;
      const scrolled = (winScroll / height) * 100;

      progressBar.style.width = scrolled + '%';
      progressBar.setAttribute('aria-valuenow', Math.round(scrolled));
      ticking = false;
    };

    window.addEventListener('scroll', () => {
      if (!ticking) {
        window.requestAnimationFrame(updateProgress);
        ticking = true;
      }
    }, { passive: true });
  };

  // ===================================
  // PRELOAD CRITICAL RESOURCES
  // ===================================

  const preloadCriticalResources = () => {
    // Preload critical fonts
    const fonts = [
      '/fonts/inter-var.woff2',
      '/fonts/lora-var.woff2',
      '/fonts/jetbrains-mono.woff2'
    ];

    fonts.forEach(font => {
      const link = document.createElement('link');
      link.rel = 'preload';
      link.as = 'font';
      link.type = 'font/woff2';
      link.crossOrigin = 'anonymous';
      link.href = font;
      document.head.appendChild(link);
    });
  };

  // ===================================
  // OPTIMIZE THIRD-PARTY SCRIPTS
  // ===================================

  const deferThirdPartyScripts = () => {
    // Defer non-critical third-party scripts
    const scripts = document.querySelectorAll('script[data-defer]');

    scripts.forEach(script => {
      const newScript = document.createElement('script');
      newScript.src = script.dataset.src;
      newScript.defer = true;

      if (script.dataset.async) {
        newScript.async = true;
      }

      document.body.appendChild(newScript);
    });
  };

  // ===================================
  // INTERSECTION OBSERVER FOR ANIMATIONS
  // ===================================

  const initScrollAnimations = () => {
    const animatedElements = document.querySelectorAll('[data-animate]');

    if (!animatedElements.length) return;

    const observer = new IntersectionObserver((entries) => {
      entries.forEach((entry, index) => {
        if (entry.isIntersecting) {
          // Stagger animation delays
          setTimeout(() => {
            entry.target.classList.add('animate-in');
          }, index * 50);
          observer.unobserve(entry.target);
        }
      });
    }, {
      threshold: 0.1,
      rootMargin: '0px 0px -50px 0px'
    });

    animatedElements.forEach(el => observer.observe(el));
  };

  // ===================================
  // PREFETCH VISIBLE LINKS
  // ===================================

  const prefetchVisibleLinks = () => {
    const links = document.querySelectorAll('a[href^="/"]');
    const prefetchedUrls = new Set();

    const linkObserver = new IntersectionObserver((entries) => {
      entries.forEach(entry => {
        if (entry.isIntersecting) {
          const href = entry.target.getAttribute('href');

          if (href && !prefetchedUrls.has(href)) {
            const link = document.createElement('link');
            link.rel = 'prefetch';
            link.href = href;
            document.head.appendChild(link);
            prefetchedUrls.add(href);
          }
        }
      });
    }, {
      rootMargin: '200px'
    });

    links.forEach(link => linkObserver.observe(link));
  };

  // ===================================
  // WEB VITALS MONITORING
  // ===================================

  const monitorWebVitals = () => {
    // Monitor Largest Contentful Paint (LCP)
    const lcpObserver = new PerformanceObserver((entryList) => {
      const entries = entryList.getEntries();
      const lastEntry = entries[entries.length - 1];
      console.log('LCP:', lastEntry.renderTime || lastEntry.loadTime);
    });

    try {
      lcpObserver.observe({ entryTypes: ['largest-contentful-paint'] });
    } catch (e) {
      // LCP not supported
    }

    // Monitor First Input Delay (FID)
    const fidObserver = new PerformanceObserver((entryList) => {
      entryList.getEntries().forEach(entry => {
        console.log('FID:', entry.processingStart - entry.startTime);
      });
    });

    try {
      fidObserver.observe({ entryTypes: ['first-input'] });
    } catch (e) {
      // FID not supported
    }

    // Monitor Cumulative Layout Shift (CLS)
    let clsScore = 0;
    const clsObserver = new PerformanceObserver((entryList) => {
      entryList.getEntries().forEach(entry => {
        if (!entry.hadRecentInput) {
          clsScore += entry.value;
          console.log('CLS:', clsScore);
        }
      });
    });

    try {
      clsObserver.observe({ entryTypes: ['layout-shift'] });
    } catch (e) {
      // CLS not supported
    }
  };

  // ===================================
  // SERVICE WORKER REGISTRATION
  // ===================================

  const registerServiceWorker = () => {
    if ('serviceWorker' in navigator) {
      window.addEventListener('load', () => {
        navigator.serviceWorker.register('/sw.js')
          .then(registration => {
            console.log('SW registered:', registration);
          })
          .catch(err => {
            console.log('SW registration failed:', err);
          });
      });
    }
  };

  // ===================================
  // CRITICAL CSS INLINING
  // ===================================

  const loadNonCriticalCSS = () => {
    const cssLinks = document.querySelectorAll('link[rel="stylesheet"][media="print"]');

    cssLinks.forEach(link => {
      link.addEventListener('load', function() {
        this.media = 'all';
      });
    });
  };

  // ===================================
  // DEBOUNCE SCROLL/RESIZE EVENTS
  // ===================================

  const debounce = (func, wait = 10) => {
    let timeout;
    return function executedFunction(...args) {
      const later = () => {
        clearTimeout(timeout);
        func(...args);
      };
      clearTimeout(timeout);
      timeout = setTimeout(later, wait);
    };
  };

  // ===================================
  // REDUCE LAYOUT THRASHING
  // ===================================

  const batchDOMReads = () => {
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
  };

  // ===================================
  // OPTIMIZE FONT LOADING
  // ===================================

  const optimizeFontLoading = () => {
    if ('fonts' in document) {
      Promise.all([
        document.fonts.load('400 1em Inter'),
        document.fonts.load('600 1em Inter'),
        document.fonts.load('700 1em Inter')
      ]).then(() => {
        document.documentElement.classList.add('fonts-loaded');
      });
    }
  };

  // ===================================
  // INITIALIZE ALL OPTIMIZATIONS
  // ===================================

  const init = () => {
    // Remove preload class to enable transitions
    document.documentElement.classList.remove('preload');

    // Initialize features
    lazyLoadImages();
    initReadingProgress();
    initScrollAnimations();
    loadNonCriticalCSS();
    optimizeFontLoading();

    // Prefetch links after initial load
    setTimeout(() => {
      prefetchVisibleLinks();
    }, 2000);

    // Monitor performance in development
    if (window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1') {
      monitorWebVitals();
    }

    // Service worker for production only
    // if (window.location.protocol === 'https:') {
    //   registerServiceWorker();
    // }
  };

  // ===================================
  // RUN ON DOM READY
  // ===================================

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }

  // ===================================
  // EXPORT UTILITIES
  // ===================================

  window.performanceUtils = {
    debounce,
    batchDOMReads: batchDOMReads()
  };

})();
