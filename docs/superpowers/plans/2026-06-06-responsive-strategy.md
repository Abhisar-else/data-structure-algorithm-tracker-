# Responsive Strategy Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.  

**Goal:** Transform the Algorithm Flight Recorder into a fully responsive web application with a touch-friendly mobile navigation and a tablet slide-in drawer.

**Architecture:** Use CSS Grid and Flexbox for fluid layouts across three main breakpoints (Desktop, Tablet, Mobile). Navigation will be managed via a slide-in overlap drawer on tablets and a full-screen overlay on mobile, both controlled by a central hamburger menu toggle.

**Tech Stack:** Vanilla HTML5, CSS3 (Media Queries, Flexbox, Grid), Vanilla JavaScript.

---

### Task 1: Mobile Header & Hamburger Menu

**Files:**
- Modify: `public/index.html`

- [ ] **Step 1: Add Hamburger Menu Icon to Header**

Add the button markup to the `.header-right` or as a new `.nav-toggle` in the header.

```html
<button class="menu-toggle" id="menu-toggle" aria-label="Open navigation menu" aria-expanded="false" aria-controls="sidebar">
  <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
    <line x1="3" y1="12" x2="21" y2="12"></line>
    <line x1="3" y1="6" x2="21" y2="6"></line>
    <line x1="3" y1="18" x2="21" y2="18"></line>
  </svg>
</button>
```

- [ ] **Step 2: Add CSS for Menu Toggle Visibility**

Ensure it's only visible on tablet and mobile screens.

```css
.menu-toggle {
  display: none;
  background: none;
  border: none;
  color: var(--text);
  cursor: pointer;
  padding: var(--sp-2);
}

@media (max-width: 1024px) {
  .menu-toggle { display: block; }
}
```

- [ ] **Step 3: Commit**

```bash
git add public/index.html
git commit -m "feat: add hamburger menu toggle for responsive navigation"
```

---

### Task 2: Responsive Breakpoints & Layout Shifts

**Files:**
- Modify: `public/index.html`

- [ ] **Step 1: Implement Tablet Breakpoint (1024px)**

Hide the sidebar by default and setup the drawer transition.

```css
@media (max-width: 1024px) {
  #sidebar {
    position: fixed;
    top: 0;
    left: 0;
    bottom: 0;
    width: 300px;
    z-index: 2000;
    transform: translateX(-100%);
    transition: transform 0.25s ease-in-out;
    background: var(--surface-0);
    box-shadow: 10px 0 30px rgba(0,0,0,0.3);
  }
  
  #sidebar.open {
    transform: translateX(0);
  }
  
  .main-layout {
    grid-template-columns: 1fr; /* Sidebar removed from grid */
  }
}
```

- [ ] **Step 2: Implement Mobile Breakpoint (768px)**

Stack the visualization and controls vertically.

```css
@media (max-width: 768px) {
  .viz-container {
    flex-direction: column;
    height: auto;
  }
  
  .viz-panel, .control-panel {
    width: 100%;
    height: auto;
  }
  
  #sidebar {
    width: 100%; /* Full screen overlay */
  }
}
```

- [ ] **Step 3: Commit**

```bash
git add public/index.html
git commit -m "style: implement responsive breakpoints and layout shifts"
```

---

### Task 3: Drawer Interaction & Scrim

**Files:**
- Modify: `public/index.html`

- [ ] **Step 1: Add Scrim Backdrop**

Add the backdrop element to the HTML.

```html
<div class="sidebar-scrim" id="sidebar-scrim"></div>
```

- [ ] **Step 2: Implement Toggle Logic in JS**

```javascript
const $menuToggle = document.getElementById('menu-toggle');
const $sidebar = document.getElementById('sidebar');
const $sidebarScrim = document.getElementById('sidebar-scrim');

function toggleSidebar() {
  const isOpen = $sidebar.classList.toggle('open');
  $sidebarScrim.classList.toggle('visible', isOpen);
  $menuToggle.setAttribute('aria-expanded', isOpen);
}

$menuToggle.addEventListener('click', toggleSidebar);
$sidebarScrim.addEventListener('click', toggleSidebar);
```

- [ ] **Step 3: Commit**

```bash
git add public/index.html
git commit -m "feat: implement sidebar drawer toggle and backdrop interaction"
```

---

### Task 4: Touch Optimizations & Swipe Gestures

**Files:**
- Modify: `public/index.html`

- [ ] **Step 1: Increase Tap Targets**

```css
.algo-btn, .speed-preset, .play-btn {
  min-height: 44px;
  min-width: 44px;
}
```

- [ ] **Step 2: Add Basic Swipe-to-Close**

```javascript
let touchStartX = 0;

$sidebar.addEventListener('touchstart', e => {
  touchStartX = e.changedTouches[0].screenX;
});

$sidebar.addEventListener('touchend', e => {
  const touchEndX = e.changedTouches[0].screenX;
  if (touchStartX - touchEndX > 50) { // Swipe left
    if ($sidebar.classList.contains('open')) toggleSidebar();
  }
});
```

- [ ] **Step 3: Commit**

```bash
git add public/index.html
git commit -m "feat: optimize touch targets and add swipe-to-close gesture"
```
