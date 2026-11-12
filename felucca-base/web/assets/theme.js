// SPDX-License-Identifier: GPL-3.0-only
// The theme toggle of the TIDE site: system (default) -> light -> dark. The choice is kept in this browser only (localStorage, if allowed).
// Loaded in the head as a plain script so the theme is set before the page paints. No network, no tracking.
(function () {
  var root = document.documentElement, KEY = "duell-tide-theme";
  function get() { try { return localStorage.getItem(KEY) || "system"; } catch (e) { return "system"; } }
  function set(v) { try { if (v === "system") localStorage.removeItem(KEY); else localStorage.setItem(KEY, v); } catch (e) { /* not allowed: the choice lasts this visit */ } }
  function apply(v) { if (v === "light" || v === "dark") root.setAttribute("data-theme", v); else root.removeAttribute("data-theme"); }
  var mode = get();
  apply(mode);
  var LABEL = { system: "Theme: system", light: "Theme: light", dark: "Theme: dark" };
  document.addEventListener("DOMContentLoaded", function () {
    var b = document.getElementById("theme");
    if (!b) return;
    function show() { b.textContent = LABEL[mode]; b.setAttribute("aria-label", LABEL[mode] + ". Press to change."); }
    show();
    b.addEventListener("click", function () {
      mode = mode === "system" ? "light" : mode === "light" ? "dark" : "system";
      set(mode); apply(mode); show();
    });
  });
})();
