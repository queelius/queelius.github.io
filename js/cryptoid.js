/**
 * Cryptoid: Client-side decryption for Hugo (v2 multi-user)
 *
 * Uses WebCrypto API for PBKDF2-SHA256 key derivation and AES-256-GCM decryption.
 * Compatible with Python cryptoid CLI v2 encryption format.
 *
 * v2 format uses per-user key wrapping: a random CEK encrypts content,
 * then each authorized user gets a key blob (CEK wrapped with their
 * PBKDF2-derived key from "username:password").
 */

(function () {
  "use strict";

  // Cryptographic parameters (must match Python implementation)
  const ITERATIONS = 310000;
  const KEY_LENGTH = 256;

  /**
   * Base64 decode to Uint8Array.
   */
  function b64decode(str) {
    return Uint8Array.from(atob(str), (c) => c.charCodeAt(0));
  }

  /**
   * Derive AES-256 key from secret using PBKDF2-SHA256.
   */
  async function deriveKey(secret, salt, usage) {
    const encoder = new TextEncoder();
    const keyMaterial = await crypto.subtle.importKey(
      "raw",
      encoder.encode(secret),
      "PBKDF2",
      false,
      ["deriveKey"]
    );

    return crypto.subtle.deriveKey(
      {
        name: "PBKDF2",
        salt: salt,
        iterations: ITERATIONS,
        hash: "SHA-256",
      },
      keyMaterial,
      { name: "AES-GCM", length: KEY_LENGTH },
      false,
      usage
    );
  }

  /**
   * Compute truncated SHA-256 hash of content for integrity verification.
   * Must match Python's content_hash() implementation.
   */
  async function computeHash(content) {
    const encoder = new TextEncoder();
    const data = encoder.encode(content);
    const hashBuffer = await crypto.subtle.digest("SHA-256", data);
    // Truncate to first 16 bytes (128 bits) to match Python implementation
    const hashArray = new Uint8Array(hashBuffer).slice(0, 16);
    return Array.from(hashArray, (b) => b.toString(16).padStart(2, "0")).join(
      ""
    );
  }

  /**
   * Build the PBKDF2 secret from username and password.
   * Format: "username:password" for user mode.
   */
  function buildSecret(username, password) {
    if (username) {
      return username + ":" + password;
    }
    return password;
  }

  /**
   * Try to unwrap a CEK from a key blob using a wrapping key.
   * Returns the raw CEK bytes or null if unwrapping fails.
   */
  async function tryUnwrapKey(keyBlob, wrappingKey) {
    try {
      const iv = b64decode(keyBlob.iv);
      const ct = b64decode(keyBlob.ct);
      const decrypted = await crypto.subtle.decrypt(
        { name: "AES-GCM", iv: iv },
        wrappingKey,
        ct
      );
      return new Uint8Array(decrypted);
    } catch (e) {
      return null;
    }
  }

  /**
   * Decrypt v2 ciphertext using username + password.
   *
   * 1. Build secret "username:password"
   * 2. Derive wrapping key via PBKDF2
   * 3. Try each key blob until one unwraps successfully → recovered CEK
   * 4. Import CEK, decrypt content, parse inner JSON, return plaintext
   */
  async function decrypt(ciphertext, password, username) {
    // Decode base64 outer layer
    const jsonStr = atob(ciphertext);
    const data = JSON.parse(jsonStr);

    // Validate format version
    if (data.v !== 2) {
      throw new Error(`Unsupported format version: ${data.v}`);
    }

    // Decode shared salt
    const salt = b64decode(data.salt);

    // Build secret and derive wrapping key
    const secret = buildSecret(username, password);
    const wrappingKey = await deriveKey(secret, salt, ["decrypt"]);

    // Try each key blob to find ours
    let cekBytes = null;
    for (const keyBlob of data.keys) {
      cekBytes = await tryUnwrapKey(keyBlob, wrappingKey);
      if (cekBytes) break;
    }

    if (!cekBytes) {
      throw new Error("No valid key blob found — wrong username or password");
    }

    // Import the recovered CEK
    const cek = await crypto.subtle.importKey(
      "raw",
      cekBytes,
      { name: "AES-GCM", length: KEY_LENGTH },
      false,
      ["decrypt"]
    );

    // Decrypt the content
    const contentIv = b64decode(data.iv);
    const contentCt = b64decode(data.ct);

    let plainBytes;
    try {
      plainBytes = await crypto.subtle.decrypt(
        { name: "AES-GCM", iv: contentIv },
        cek,
        contentCt
      );
    } catch (e) {
      throw new Error("Content decryption failed — data may be corrupted");
    }

    // Parse inner JSON wrapper {"c": plaintext, "m": null}
    const innerJson = new TextDecoder().decode(plainBytes);
    const inner = JSON.parse(innerJson);
    return inner.c;
  }

  /**
   * Get storage key for a container.
   */
  function getStorageKey(containerId) {
    return `cryptoid-${containerId}`;
  }

  /**
   * Save credentials based on remember setting.
   * Stores JSON {"u": username, "p": password} for user mode.
   */
  function saveCredentials(containerId, username, password, rememberMode, userChecked) {
    const key = getStorageKey(containerId);

    // Determine actual storage based on mode
    let storage = null;
    if (rememberMode === "local" || (rememberMode === "ask" && userChecked)) {
      storage = localStorage;
    } else if (rememberMode === "session") {
      storage = sessionStorage;
    }

    if (storage) {
      try {
        storage.setItem(key, JSON.stringify({ u: username || "", p: password }));
      } catch (e) {
        // Storage may be unavailable (incognito mode, quota exceeded, disabled)
      }
    }
  }

  /**
   * Try to retrieve saved credentials.
   * Returns {u: username, p: password} or null.
   */
  function getSavedCredentials(containerId) {
    const key = getStorageKey(containerId);

    let raw;
    try {
      raw = localStorage.getItem(key) || sessionStorage.getItem(key);
    } catch (e) {
      // Storage may be unavailable (incognito mode, disabled)
      return null;
    }
    if (!raw) return null;

    try {
      const parsed = JSON.parse(raw);
      if (parsed && typeof parsed.p === "string") {
        return parsed;
      }
    } catch (e) {
      // Ignore malformed data
    }

    // Legacy v1 format: raw password string
    // Clear it since we can't use it without a username
    clearSavedCredentials(containerId);
    return null;
  }

  /**
   * Clear saved credentials.
   */
  function clearSavedCredentials(containerId) {
    const key = getStorageKey(containerId);
    try {
      localStorage.removeItem(key);
      sessionStorage.removeItem(key);
    } catch (e) {
      // Storage may be unavailable (incognito mode, disabled)
    }
  }

  /**
   * Check if a URL is safe for use in href attributes.
   * Allows http:, https:, mailto:, and relative URLs.
   */
  function isSafeUrl(url) {
    const trimmed = url.trim().toLowerCase();
    if (
      trimmed.startsWith("http:") ||
      trimmed.startsWith("https:") ||
      trimmed.startsWith("mailto:")
    ) {
      return true;
    }
    // Block URLs with explicit schemes (e.g., javascript:, data:, vbscript:)
    if (/^[a-z][a-z0-9+.-]*:/i.test(trimmed)) {
      return false;
    }
    // Allow relative URLs (no scheme)
    return true;
  }

  /**
   * Show the unlock state: render content, hide form, add lock button if credentials are saved.
   */
  function showUnlocked(container, plaintext) {
    const form = container.querySelector(".cryptoid-form");
    const contentDiv = container.querySelector(".cryptoid-content");
    const containerId = container.id;

    contentDiv.innerHTML = renderContent(plaintext);
    contentDiv.style.display = "block";
    form.style.display = "none";
    container.classList.add("cryptoid-unlocked");

    // Add lock button if credentials are saved
    if (getSavedCredentials(containerId)) {
      const lockBtn = document.createElement("button");
      lockBtn.className = "cryptoid-lock-btn";
      lockBtn.textContent = "Lock";
      lockBtn.addEventListener("click", function () {
        clearSavedCredentials(containerId);
        contentDiv.style.display = "none";
        contentDiv.innerHTML = "";
        lockBtn.remove();
        form.style.display = "";
        container.classList.remove("cryptoid-unlocked");
      });
      contentDiv.parentNode.insertBefore(lockBtn, contentDiv);
    }
  }

  /**
   * Render decrypted markdown as HTML using marked.js.
   * Falls back to basic escaping if marked is not loaded.
   */
  function renderContent(markdown) {
    if (typeof marked !== "undefined" && marked.parse) {
      // Configure marked with link sanitization
      const renderer = new marked.Renderer();
      const originalLink = renderer.link.bind(renderer);
      renderer.link = function (token) {
        if (!isSafeUrl(token.href)) {
          return token.text;
        }
        return originalLink(token);
      };
      return marked.parse(markdown, { renderer: renderer, breaks: false });
    }
    // Fallback: escape HTML and wrap in paragraph
    return "<p>" + markdown
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/\n\n+/g, "</p><p>")
      .replace(/\n/g, "<br>") + "</p>";
  }

  /**
   * Main decryption handler for form submission.
   */
  window.cryptoidDecrypt = async function (containerId, event) {
    event.preventDefault();

    const container = document.getElementById(containerId);
    const form = container.querySelector(".cryptoid-form");
    const mode = container.dataset.mode || "user";
    const usernameInput = container.querySelector(".cryptoid-username");
    const passwordInput = container.querySelector(".cryptoid-password");
    const errorDiv = container.querySelector(".cryptoid-error");
    const contentDiv = container.querySelector(".cryptoid-content");
    const template = container.querySelector(".cryptoid-ciphertext");
    const rememberCheckbox = container.querySelector(
      ".cryptoid-remember-checkbox"
    );

    const username = mode === "user" && usernameInput ? usernameInput.value : "";
    const password = passwordInput.value;
    const ciphertext = template.innerHTML.trim();
    const rememberMode = container.dataset.remember;
    const expectedHash = container.dataset.contentHash;

    // Hide any previous error
    errorDiv.style.display = "none";

    try {
      // Decrypt content
      const plaintext = await decrypt(ciphertext, password, username);

      // Verify content hash if present
      if (expectedHash) {
        const actualHash = await computeHash(plaintext);
        if (actualHash !== expectedHash) {
          throw new Error("Content hash mismatch - decryption may be corrupted");
        }
      }

      // Save credentials if requested
      saveCredentials(
        containerId,
        username,
        password,
        rememberMode,
        rememberCheckbox?.checked
      );

      // Render and display unlocked content with optional lock button
      showUnlocked(container, plaintext);
    } catch (e) {
      // Show error — distinguish format errors from wrong credentials
      let message;
      if (
        e.message &&
        (e.message.includes("Unsupported format") ||
          e.message.includes("JSON") ||
          e.message.includes("atob"))
      ) {
        message =
          "Unable to decrypt: the encrypted data appears to be corrupted.";
      } else if (e.message && e.message.includes("hash mismatch")) {
        message =
          "Decryption succeeded but content integrity check failed. The data may be corrupted.";
      } else {
        message = "Incorrect username or password. Please try again.";
      }
      errorDiv.textContent = message;
      errorDiv.style.display = "block";
      passwordInput.value = "";
      passwordInput.focus();
    }

    return false;
  };

  /**
   * Auto-decrypt containers with saved credentials on page load.
   */
  async function autoDecrypt() {
    const containers = document.querySelectorAll(".cryptoid-container");

    for (const container of containers) {
      const containerId = container.id;
      const saved = getSavedCredentials(containerId);

      if (saved) {
        const template = container.querySelector(".cryptoid-ciphertext");
        const ciphertext = template.innerHTML.trim();
        const expectedHash = container.dataset.contentHash;

        try {
          const plaintext = await decrypt(ciphertext, saved.p, saved.u);

          // Verify content hash if present
          if (expectedHash) {
            const actualHash = await computeHash(plaintext);
            if (actualHash !== expectedHash) {
              throw new Error("Content hash mismatch");
            }
          }

          showUnlocked(container, plaintext);
        } catch (e) {
          // Saved credentials are wrong or content corrupted, clear them
          clearSavedCredentials(containerId);
        }
      }
    }
  }

  // Auto-decrypt on page load
  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", autoDecrypt);
  } else {
    autoDecrypt();
  }
})();
