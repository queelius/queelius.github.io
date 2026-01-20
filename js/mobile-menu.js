// Mobile Menu Toggle
document.addEventListener('DOMContentLoaded', function() {
  const mobileMenuToggle = document.querySelector('.mobile-menu-toggle');
  const navMenu = document.querySelector('.nav-primary ul');

  if (mobileMenuToggle && navMenu) {
    mobileMenuToggle.addEventListener('click', function() {
      const isExpanded = this.getAttribute('aria-expanded') === 'true';
      this.setAttribute('aria-expanded', !isExpanded);
      navMenu.classList.toggle('active');
    });

    // Close menu when clicking outside
    document.addEventListener('click', function(event) {
      if (!mobileMenuToggle.contains(event.target) && !navMenu.contains(event.target)) {
        mobileMenuToggle.setAttribute('aria-expanded', 'false');
        navMenu.classList.remove('active');
      }
    });

    // Close menu when pressing Escape
    document.addEventListener('keydown', function(event) {
      if (event.key === 'Escape') {
        mobileMenuToggle.setAttribute('aria-expanded', 'false');
        navMenu.classList.remove('active');
      }
    });

    // Close menu when navigating (but not for dropdown toggles)
    navMenu.querySelectorAll('a:not(.dropdown-toggle)').forEach(link => {
      link.addEventListener('click', function() {
        mobileMenuToggle.setAttribute('aria-expanded', 'false');
        navMenu.classList.remove('active');
      });
    });
  }
});