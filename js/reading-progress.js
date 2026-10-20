// Reading Progress Bar
// Shows a thin progress indicator at the top of the page as user scrolls

(function() {
  'use strict';

  // Only show on post pages
  const isPostPage = document.body.classList.contains('type-post') ||
                     document.querySelector('article.single-post');

  if (!isPostPage) return;

  // Create progress bar element
  const progressBar = document.createElement('div');
  progressBar.id = 'reading-progress';
  progressBar.setAttribute('role', 'progressbar');
  progressBar.setAttribute('aria-label', 'Reading progress');
  progressBar.setAttribute('aria-valuemin', '0');
  progressBar.setAttribute('aria-valuemax', '100');

  // Style the progress bar
  const style = document.createElement('style');
  style.textContent = `
    #reading-progress {
      position: fixed;
      top: 0;
      left: 0;
      height: 3px;
      background: linear-gradient(90deg, #667eea 0%, #764ba2 100%);
      width: 0%;
      z-index: 9999;
      transition: width 0.1s ease-out;
      box-shadow: 0 1px 3px rgba(102, 126, 234, 0.3);
    }

    #reading-progress.complete {
      background: linear-gradient(90deg, #10b981 0%, #059669 100%);
    }

    @media (prefers-reduced-motion: reduce) {
      #reading-progress {
        transition: none;
      }
    }
  `;

  document.head.appendChild(style);
  document.body.appendChild(progressBar);

  // Update progress on scroll
  function updateProgress() {
    const scrollTop = window.scrollY || document.documentElement.scrollTop;
    const docHeight = document.documentElement.scrollHeight - document.documentElement.clientHeight;
    const scrollPercent = (scrollTop / docHeight) * 100;

    progressBar.style.width = scrollPercent + '%';
    progressBar.setAttribute('aria-valuenow', Math.round(scrollPercent));

    // Add 'complete' class when at bottom
    if (scrollPercent >= 99) {
      progressBar.classList.add('complete');
    } else {
      progressBar.classList.remove('complete');
    }
  }

  // Throttle scroll events for performance
  let ticking = false;
  window.addEventListener('scroll', function() {
    if (!ticking) {
      window.requestAnimationFrame(function() {
        updateProgress();
        ticking = false;
      });
      ticking = true;
    }
  }, { passive: true });

  // Initial update
  updateProgress();
})();
