// Accessible dropdown navigation for the primary header.
// Replaces the inline script formerly in layouts/partials/page-header.html.
// Behavior: click toggles a dropdown (closing others), outside click and
// Escape close, ArrowDown/ArrowUp/Home/End move focus within an open menu,
// Escape returns focus to the toggle.
document.addEventListener('DOMContentLoaded', function () {
  const dropdowns = Array.from(document.querySelectorAll('.nav-dropdown'));
  if (dropdowns.length === 0) return;

  function setOpen(dropdown, open) {
    dropdown.classList.toggle('open', open);
    const toggle = dropdown.querySelector('.dropdown-toggle');
    if (toggle) toggle.setAttribute('aria-expanded', String(open));
  }

  function closeAll(except) {
    dropdowns.forEach(function (d) {
      if (d !== except) setOpen(d, false);
    });
  }

  function menuLinks(dropdown) {
    return Array.from(dropdown.querySelectorAll('.dropdown-menu a'));
  }

  dropdowns.forEach(function (dropdown) {
    const toggle = dropdown.querySelector('.dropdown-toggle');
    if (!toggle) return;

    toggle.addEventListener('click', function () {
      const isOpen = dropdown.classList.contains('open');
      closeAll(dropdown);
      setOpen(dropdown, !isOpen);
    });

    // ArrowDown on the toggle opens the menu and focuses its first item.
    toggle.addEventListener('keydown', function (e) {
      if (e.key === 'ArrowDown') {
        e.preventDefault();
        e.stopPropagation();
        closeAll(dropdown);
        setOpen(dropdown, true);
        const links = menuLinks(dropdown);
        if (links.length > 0) links[0].focus();
      }
    });

    // Arrow-key navigation within an open menu.
    dropdown.addEventListener('keydown', function (e) {
      if (!dropdown.classList.contains('open')) return;
      const links = menuLinks(dropdown);
      const idx = links.indexOf(document.activeElement);
      if (e.key === 'ArrowDown' && idx > -1) {
        e.preventDefault();
        if (idx < links.length - 1) links[idx + 1].focus();
      } else if (e.key === 'ArrowUp' && idx > -1) {
        e.preventDefault();
        if (idx > 0) {
          links[idx - 1].focus();
        } else {
          toggle.focus();
        }
      } else if (e.key === 'Home' && idx > -1) {
        e.preventDefault();
        links[0].focus();
      } else if (e.key === 'End' && idx > -1) {
        e.preventDefault();
        links[links.length - 1].focus();
      } else if (e.key === 'Escape') {
        setOpen(dropdown, false);
        toggle.focus();
      }
    });
  });

  document.addEventListener('click', function (e) {
    if (!e.target.closest('.nav-dropdown')) closeAll(null);
  });

  document.addEventListener('keydown', function (e) {
    if (e.key === 'Escape') closeAll(null);
  });
});
