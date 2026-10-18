/* HCPBridge documentation - navigation, table of contents, copy buttons.
 * To add a page: create the HTML file and add one entry to NAV below. */
(function () {
  var NAV = [
    { group: "Introduction" },
    { href: "index.html", title: "Overview" },
    { href: "getting-started.html", title: "Getting started" },
    { group: "Using HCPBridge" },
    { href: "web-interface.html", title: "Web interface" },
    { href: "configuration.html", title: "Configuration reference" },
    { href: "home-assistant.html", title: "MQTT & Home Assistant" },
    { href: "sensors.html", title: "Sensors" },
    { href: "inputs-outputs.html", title: "Digital inputs & outputs" },
    { href: "http-api.html", title: "HTTP API" },
    { group: "Reference" },
    { href: "architecture.html", title: "Architecture & development" },
    { href: "troubleshooting.html", title: "Troubleshooting & FAQ" }
  ];

  var here = location.pathname.split("/").pop() || "index.html";
  var pages = NAV.filter(function (n) { return n.href; });

  // ---- sidebar
  var sidebar = document.getElementById("sidebar");
  if (sidebar) {
    var html = "";
    NAV.forEach(function (n) {
      if (n.group) html += '<div class="group">' + n.group + "</div>";
      else html += '<a href="' + n.href + '"' + (n.href === here ? ' class="active" aria-current="page"' : "") + ">" + n.title + "</a>";
    });
    sidebar.innerHTML = html;
  }

  // ---- mobile menu
  var btn = document.querySelector(".menu-btn");
  if (btn) {
    btn.addEventListener("click", function () {
      var open = document.body.classList.toggle("nav-open");
      btn.setAttribute("aria-expanded", open ? "true" : "false");
    });
    document.addEventListener("click", function (e) {
      if (document.body.classList.contains("nav-open") && !sidebar.contains(e.target) && !btn.contains(e.target)) {
        document.body.classList.remove("nav-open");
      }
    });
  }

  var main = document.querySelector("main");
  if (!main) return;

  // ---- heading ids + anchors
  function slug(t) {
    return t.toLowerCase().replace(/[^a-z0-9\s-]/g, "").trim().replace(/\s+/g, "-");
  }
  var heads = Array.prototype.filter.call(main.querySelectorAll("h2, h3"), function (h) {
    return !h.closest(".card");
  });
  heads.forEach(function (h) {
    h.dataset.title = h.textContent;
    if (!h.id) h.id = slug(h.textContent);
    var a = document.createElement("a");
    a.className = "anchor"; a.href = "#" + h.id; a.textContent = "#"; a.setAttribute("aria-hidden", "true");
    h.appendChild(a);
  });

  // ---- table of contents (h2 + h3)
  var toc = document.getElementById("toc");
  if (toc && heads.length > 1) {
    var t = '<div class="title">On this page</div>';
    heads.forEach(function (h) {
      t += '<a href="#' + h.id + '"' + (h.tagName === "H3" ? ' class="sub"' : "") + ">" +
        h.dataset.title.replace(/</g, "&lt;") + "</a>";
    });
    toc.innerHTML = t;
    var links = toc.querySelectorAll("a");
    var onScroll = function () {
      var current = 0;
      heads.forEach(function (h, i) { if (h.getBoundingClientRect().top < 120) current = i; });
      links.forEach(function (l, i) { l.classList.toggle("active", i === current); });
    };
    window.addEventListener("scroll", onScroll, { passive: true });
    onScroll();
  }

  // ---- scrollable tables
  main.querySelectorAll("table").forEach(function (tbl) {
    if (tbl.parentNode.classList.contains("table-wrap")) return;
    var w = document.createElement("div");
    w.className = "table-wrap";
    tbl.parentNode.insertBefore(w, tbl);
    w.appendChild(tbl);
  });

  // ---- copy buttons
  main.querySelectorAll("pre").forEach(function (pre) {
    var b = document.createElement("button");
    b.className = "copy"; b.type = "button"; b.textContent = "Copy";
    b.addEventListener("click", function () {
      var text = pre.querySelector("code") ? pre.querySelector("code").innerText : pre.innerText;
      var done = function () { b.textContent = "Copied"; setTimeout(function () { b.textContent = "Copy"; }, 1500); };
      if (navigator.clipboard) navigator.clipboard.writeText(text).then(done);
    });
    pre.appendChild(b);
  });

  // ---- previous / next
  var idx = pages.findIndex(function (p) { return p.href === here; });
  if (idx >= 0) {
    var pager = document.createElement("nav");
    pager.className = "pager";
    var h = "";
    if (idx > 0) h += '<a class="prev" href="' + pages[idx - 1].href + '"><small>Previous</small>' + pages[idx - 1].title + "</a>";
    if (idx < pages.length - 1) h += '<a class="next" href="' + pages[idx + 1].href + '"><small>Next</small>' + pages[idx + 1].title + "</a>";
    pager.innerHTML = h;
    main.appendChild(pager);
  }
  var foot = document.createElement("footer");
  foot.className = "footer";
  foot.innerHTML = 'HCPBridge is licensed under the <a href="https://github.com/Gifford47/HCPBridgeMqtt/blob/master/LICENSE">PolyForm Noncommercial License 1.0.0</a> – commercial use is not permitted. Found an error in these docs? ' +
    '<a href="https://github.com/Gifford47/HCPBridgeMqtt/issues">Open an issue</a> or edit the page in ' +
    '<a href="https://github.com/Gifford47/HCPBridgeMqtt/tree/master/docs">/docs</a>.';
  main.appendChild(foot);
})();
