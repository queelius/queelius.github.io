/* GA4 outbound-click tracking (metafunctor.com)
 *
 * A single delegated listener fires one `outbound_click` event for any click on
 * a link whose host differs from this site's. The destination domain and URL
 * ride along as parameters, so every external target (YouTube, Amazon, GitHub,
 * and so on) is distinguishable in GA4 without wiring links individually or
 * keeping a per-destination lookup table.
 *
 * To turn these into ad-optimizable goals, mark `outbound_click` as a key event
 * in GA4, optionally split by link_domain (youtube.com for channel subscribes,
 * amazon.com for book-page visits).
 *
 * The event name is deliberately distinct from GA4 Enhanced Measurement's
 * built-in `click`, so this never double-counts if that setting is also on.
 */
(function () {
  function hostOf(url) {
    try { return new URL(url).host; } catch (e) { return ''; }
  }

  document.addEventListener('click', function (event) {
    var a = event.target && event.target.closest ? event.target.closest('a[href]') : null;
    if (!a) return;

    var href = a.href;                                 // resolved absolute URL
    if (!/^https?:/i.test(href)) return;               // skip mailto:, tel:, #frag

    var domain = hostOf(href);
    if (!domain || domain === window.location.host) return;   // internal link

    if (typeof window.gtag !== 'function') return;     // GA not loaded (adblock/consent)

    window.gtag('event', 'outbound_click', {
      link_domain: domain,
      link_url: href,
      link_text: (a.textContent || '').trim().slice(0, 100),
      outbound: true,
      transport_type: 'beacon'                         // survive same-tab navigation
    });
  }, true);                                            // capture: runs even if a handler stops propagation
})();
