// Copy Code Button
// Adds a copy button to all code blocks for easy code copying

(function() {
  'use strict';

  // Add styles for copy button
  const style = document.createElement('style');
  style.textContent = `
    .code-block-wrapper {
      position: relative;
    }

    .copy-code-button {
      position: absolute;
      top: 0.5rem;
      right: 0.5rem;
      padding: 0.5rem 0.75rem;
      background: rgba(255, 255, 255, 0.9);
      color: var(--ink, #1a1a1a);
      border: 1px solid rgba(0, 0, 0, 0.1);
      border-radius: 6px;
      font-size: 0.75rem;
      font-weight: 600;
      cursor: pointer;
      opacity: 0;
      transition: all 0.2s ease;
      display: flex;
      align-items: center;
      gap: 0.25rem;
      z-index: 10;
      box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
    }

    [data-theme="dark"] .copy-code-button {
      background: rgba(30, 30, 30, 0.95);
      color: var(--ink, #e8e8e8);
      border-color: rgba(255, 255, 255, 0.2);
    }

    .code-block-wrapper:hover .copy-code-button {
      opacity: 1;
    }

    .copy-code-button:hover {
      background: var(--accent, #667eea);
      color: white;
      border-color: var(--accent, #667eea);
      transform: translateY(-1px);
      box-shadow: 0 4px 8px rgba(102, 126, 234, 0.3);
    }

    .copy-code-button:active {
      transform: translateY(0);
    }

    .copy-code-button.copied {
      background: #10b981;
      color: white;
      border-color: #10b981;
    }

    .copy-code-button svg {
      width: 14px;
      height: 14px;
      fill: currentColor;
    }

    /* Make sure code blocks are positioned relatively */
    pre {
      position: relative;
    }
  `;
  document.head.appendChild(style);

  // Icons
  const copyIcon = `<svg viewBox="0 0 24 24"><path d="M16 1H4c-1.1 0-2 .9-2 2v14h2V3h12V1zm3 4H8c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h11c1.1 0 2-.9 2-2V7c0-1.1-.9-2-2-2zm0 16H8V7h11v14z"/></svg>`;
  const checkIcon = `<svg viewBox="0 0 24 24"><path d="M9 16.17L4.83 12l-1.42 1.41L9 19 21 7l-1.41-1.41L9 16.17z"/></svg>`;

  // Find all code blocks
  function addCopyButtons() {
    const codeBlocks = document.querySelectorAll('pre code');

    codeBlocks.forEach(function(codeBlock) {
      const pre = codeBlock.parentElement;

      // Skip if button already added
      if (pre.querySelector('.copy-code-button')) return;

      // Create wrapper if needed
      if (!pre.classList.contains('code-block-wrapper')) {
        const wrapper = document.createElement('div');
        wrapper.className = 'code-block-wrapper';
        pre.parentNode.insertBefore(wrapper, pre);
        wrapper.appendChild(pre);
      }

      // Create copy button
      const button = document.createElement('button');
      button.className = 'copy-code-button';
      button.type = 'button';
      button.innerHTML = copyIcon + '<span>Copy</span>';
      button.setAttribute('aria-label', 'Copy code to clipboard');

      // Add click handler
      button.addEventListener('click', async function() {
        const code = codeBlock.textContent;

        try {
          await navigator.clipboard.writeText(code);

          // Show success state
          button.innerHTML = checkIcon + '<span>Copied!</span>';
          button.classList.add('copied');

          // Reset after 2 seconds
          setTimeout(function() {
            button.innerHTML = copyIcon + '<span>Copy</span>';
            button.classList.remove('copied');
          }, 2000);
        } catch (err) {
          console.error('Failed to copy code:', err);

          // Fallback for older browsers
          const textArea = document.createElement('textarea');
          textArea.value = code;
          textArea.style.position = 'fixed';
          textArea.style.left = '-999999px';
          document.body.appendChild(textArea);
          textArea.select();

          try {
            document.execCommand('copy');
            button.innerHTML = checkIcon + '<span>Copied!</span>';
            button.classList.add('copied');
            setTimeout(function() {
              button.innerHTML = copyIcon + '<span>Copy</span>';
              button.classList.remove('copied');
            }, 2000);
          } catch (err) {
            button.innerHTML = copyIcon + '<span>Failed</span>';
          }

          document.body.removeChild(textArea);
        }
      });

      pre.appendChild(button);
    });
  }

  // Add buttons when DOM is ready
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', addCopyButtons);
  } else {
    addCopyButtons();
  }
})();
