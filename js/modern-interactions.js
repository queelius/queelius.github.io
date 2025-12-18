// Modern Interactions and Animations for Metafunctor

// Smooth scroll for anchor links
document.querySelectorAll('a[href^="#"]').forEach(anchor => {
  anchor.addEventListener('click', function (e) {
    e.preventDefault();
    const target = document.querySelector(this.getAttribute('href'));
    if (target) {
      target.scrollIntoView({
        behavior: 'smooth',
        block: 'start'
      });
    }
  });
});

// Intersection Observer for fade-in animations
const observerOptions = {
  root: null,
  rootMargin: '0px',
  threshold: 0.1
};

const observer = new IntersectionObserver((entries) => {
  entries.forEach(entry => {
    if (entry.isIntersecting) {
      entry.target.classList.add('visible');

      // Add staggered animation for children
      const children = entry.target.querySelectorAll('.stagger-item');
      children.forEach((child, index) => {
        setTimeout(() => {
          child.classList.add('visible');
        }, index * 100);
      });
    }
  });
}, observerOptions);

// Observe elements with animation classes
document.addEventListener('DOMContentLoaded', () => {
  const animatedElements = document.querySelectorAll('.fade-in, .slide-in-left, .slide-in-right, .bounce-in');
  animatedElements.forEach(el => observer.observe(el));
});

// Parallax effect for hero sections
let ticking = false;
function updateParallax() {
  const scrolled = window.pageYOffset;
  const parallaxElements = document.querySelectorAll('.parallax');

  parallaxElements.forEach(element => {
    const speed = element.dataset.speed || 0.5;
    const yPos = -(scrolled * speed);
    element.style.transform = `translateY(${yPos}px)`;
  });

  ticking = false;
}

function requestTick() {
  if (!ticking) {
    window.requestAnimationFrame(updateParallax);
    ticking = true;
  }
}

window.addEventListener('scroll', requestTick);

// Animated counter for statistics
function animateCounter(element) {
  // Skip if no data-count attribute (static numbers)
  if (!element.dataset.count) {
    return;
  }

  const target = parseInt(element.dataset.count);
  if (isNaN(target)) {
    return;
  }

  const duration = 2000;
  const start = 0;
  const increment = target / (duration / 16);
  let current = start;

  const timer = setInterval(() => {
    current += increment;
    if (current >= target) {
      current = target;
      clearInterval(timer);
    }

    // Format large numbers
    let display = Math.floor(current);
    if (display >= 1000000) {
      display = (display / 1000000).toFixed(1) + 'M';
    } else if (display >= 1000) {
      display = (display / 1000).toFixed(1) + 'K';
    }

    element.textContent = display + '+';
  }, 16);
}

// Start counter animation when visible
const counterObserver = new IntersectionObserver((entries) => {
  entries.forEach(entry => {
    if (entry.isIntersecting && !entry.target.classList.contains('counted')) {
      animateCounter(entry.target);
      entry.target.classList.add('counted');
    }
  });
}, { threshold: 0.5 });

// Only observe stat-numbers that have data-count attribute for animation
document.querySelectorAll('.stat-number[data-count]').forEach(counter => {
  counterObserver.observe(counter);
});

// Typing effect for hero text
function typeWriter(element, text, speed = 50) {
  let i = 0;
  element.textContent = '';

  function type() {
    if (i < text.length) {
      element.textContent += text.charAt(i);
      i++;
      setTimeout(type, speed);
    }
  }

  type();
}

// Magnetic button effect
document.querySelectorAll('.magnetic').forEach(button => {
  button.addEventListener('mousemove', (e) => {
    const rect = button.getBoundingClientRect();
    const x = e.clientX - rect.left - rect.width / 2;
    const y = e.clientY - rect.top - rect.height / 2;

    button.style.transform = `translate(${x * 0.3}px, ${y * 0.3}px)`;
  });

  button.addEventListener('mouseleave', () => {
    button.style.transform = 'translate(0, 0)';
  });
});

// Smooth hover effect for cards
document.querySelectorAll('.card, .article-card').forEach(card => {
  card.addEventListener('mouseenter', (e) => {
    const rect = card.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;

    card.style.setProperty('--mouse-x', `${x}px`);
    card.style.setProperty('--mouse-y', `${y}px`);
  });
});

// Copy code button functionality
function addCopyButtons() {
  const codeBlocks = document.querySelectorAll('pre code');

  codeBlocks.forEach(block => {
    const button = document.createElement('button');
    button.className = 'copy-code-btn';
    button.innerHTML = '<i class="fas fa-copy"></i>';
    button.title = 'Copy code';

    button.addEventListener('click', () => {
      const text = block.textContent;
      navigator.clipboard.writeText(text).then(() => {
        button.innerHTML = '<i class="fas fa-check"></i>';
        button.classList.add('copied');

        setTimeout(() => {
          button.innerHTML = '<i class="fas fa-copy"></i>';
          button.classList.remove('copied');
        }, 2000);
      });
    });

    const pre = block.parentElement;
    pre.style.position = 'relative';
    pre.appendChild(button);
  });
}

document.addEventListener('DOMContentLoaded', addCopyButtons);

// Image lazy loading with blur effect
const imageObserver = new IntersectionObserver((entries) => {
  entries.forEach(entry => {
    if (entry.isIntersecting) {
      const img = entry.target;
      const src = img.dataset.src;

      if (src) {
        const tempImg = new Image();
        tempImg.onload = () => {
          img.src = src;
          img.classList.add('loaded');
        };
        tempImg.src = src;
        imageObserver.unobserve(img);
      }
    }
  });
}, { rootMargin: '50px' });

document.querySelectorAll('img[data-src]').forEach(img => {
  imageObserver.observe(img);
});

// Smooth page transitions
function smoothTransition(url) {
  document.body.classList.add('fade-out');

  setTimeout(() => {
    window.location.href = url;
  }, 300);
}

// Add transition to internal links
document.querySelectorAll('a[href^="/"]').forEach(link => {
  link.addEventListener('click', (e) => {
    if (!e.ctrlKey && !e.metaKey && !e.shiftKey) {
      e.preventDefault();
      smoothTransition(link.href);
    }
  });
});

// Reading progress indicator
function updateReadingProgress() {
  const article = document.querySelector('article');
  if (!article) return;

  const articleTop = article.offsetTop;
  const articleHeight = article.offsetHeight;
  const windowHeight = window.innerHeight;
  const scrolled = window.scrollY;

  const progress = Math.max(0, Math.min(1,
    (scrolled - articleTop + windowHeight) / articleHeight
  ));

  const progressBar = document.querySelector('.reading-progress');
  if (progressBar) {
    progressBar.style.width = `${progress * 100}%`;
  }
}

window.addEventListener('scroll', updateReadingProgress);

// Tooltip functionality
function initTooltips() {
  const tooltipElements = document.querySelectorAll('[data-tooltip]');

  tooltipElements.forEach(element => {
    const tooltip = document.createElement('div');
    tooltip.className = 'tooltip';
    tooltip.textContent = element.dataset.tooltip;
    document.body.appendChild(tooltip);

    element.addEventListener('mouseenter', (e) => {
      const rect = element.getBoundingClientRect();
      tooltip.style.left = `${rect.left + rect.width / 2}px`;
      tooltip.style.top = `${rect.top - 10}px`;
      tooltip.classList.add('visible');
    });

    element.addEventListener('mouseleave', () => {
      tooltip.classList.remove('visible');
    });
  });
}

document.addEventListener('DOMContentLoaded', initTooltips);

// Filter functionality for content
function initFilters() {
  const filterButtons = document.querySelectorAll('.filter-btn');
  const filterableItems = document.querySelectorAll('[data-type], [data-status]');

  filterButtons.forEach(button => {
    button.addEventListener('click', () => {
      const filterGroup = button.parentElement;
      const filterType = filterGroup.dataset.filter;
      const filterValue = button.dataset.value;

      // Update active state
      filterGroup.querySelectorAll('.filter-btn').forEach(btn => {
        btn.classList.remove('active');
      });
      button.classList.add('active');

      // Apply filter
      filterableItems.forEach(item => {
        if (filterValue === 'all') {
          item.style.display = '';
        } else {
          const itemValue = item.dataset[filterType];
          if (itemValue === filterValue) {
            item.style.display = '';
            item.classList.add('fade-in');
          } else {
            item.style.display = 'none';
          }
        }
      });
    });
  });
}

document.addEventListener('DOMContentLoaded', initFilters);

// Add ripple effect to buttons
function createRipple(event) {
  const button = event.currentTarget;
  const ripple = document.createElement('span');
  const rect = button.getBoundingClientRect();
  const size = Math.max(rect.width, rect.height);
  const x = event.clientX - rect.left - size / 2;
  const y = event.clientY - rect.top - size / 2;

  ripple.style.width = ripple.style.height = `${size}px`;
  ripple.style.left = `${x}px`;
  ripple.style.top = `${y}px`;
  ripple.classList.add('ripple');

  button.appendChild(ripple);

  setTimeout(() => {
    ripple.remove();
  }, 600);
}

document.querySelectorAll('.btn').forEach(button => {
  button.addEventListener('click', createRipple);
});

// Performance optimization - Debounce scroll events
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

// Apply debouncing to scroll-based functions
const debouncedUpdateParallax = debounce(updateParallax, 10);
const debouncedUpdateReadingProgress = debounce(updateReadingProgress, 10);

window.addEventListener('scroll', () => {
  debouncedUpdateParallax();
  debouncedUpdateReadingProgress();
});

// Preload critical images
function preloadImages() {
  const criticalImages = document.querySelectorAll('[data-preload]');
  criticalImages.forEach(img => {
    const tempImg = new Image();
    tempImg.src = img.dataset.src || img.src;
  });
}

document.addEventListener('DOMContentLoaded', preloadImages);

// Export functions for use in other scripts
window.metafunctor = {
  smoothTransition,
  animateCounter,
  typeWriter,
  initTooltips,
  initFilters,
  createRipple
};