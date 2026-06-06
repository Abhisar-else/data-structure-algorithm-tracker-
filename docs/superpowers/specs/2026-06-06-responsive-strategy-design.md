# Design Spec: Responsive Strategy (Algorithm Flight Recorder)

**Date:** 2026-06-06
**Status:** Draft
**Topic:** UX / Mobile Responsiveness

## Overview
Transform the current desktop-only 3-column layout into a fluid, responsive experience that supports Desktop, Tablet, and Mobile users without sacrificing functionality.

## Goals
- Maintain access to all 24+ algorithms on small screens.
- Ensure tap targets are touch-friendly (min 44x44px).
- Provide a focused visualization experience on mobile.

## Design

### 1. Breakpoints
- **Desktop (1024px+):** Standard 3-column layout.
- **Tablet (768px - 1023px):** Slide-in Overlap Drawer.
- **Mobile (< 768px):** Full-screen Overlay + Vertical Stack.

### 2. Navigation (The Drawer/Overlay)
- **Tablet:** The sidebar remains hidden. A "hamburger" menu icon in the header triggers a slide-in drawer from the left.
- **Mobile:** The hamburger menu triggers a full-screen modal overlay.
- **Scrim:** Both modes use a dimmed backdrop (`rgba(0,0,0,0.5)`) that, when tapped, closes the navigation.
- **Collapsible Categories:** Category headers (Sorting, Searching, etc.) in the menu will be collapsible on mobile to save space.

### 3. Content Layout (Mobile)
- **Visualizer First:** The visualization canvas remains at the top of the stack (below the header).
- **Controls Second:** Playback controls, speed presets, and metrics follow below the visualization.
- **Code Trace:** On very small screens (< 480px), the "Source Trace" tab will be moved to a secondary view or collapsible section.

### 4. Technical Implementation
- **CSS:** Use CSS Grid for the main container and `@media` queries for layout shifts.
- **Transitions:** `transform: translateX()` and `opacity` transitions (250ms, ease-in-out) for smooth drawer movement.
- **Touch Targets:** Increase padding on `.algo-btn`, `.speed-preset`, and playback controls.

## Success Criteria
- [ ] App is usable on a 375px viewport (iPhone SE).
- [ ] All 24 algorithms can be selected and run on tablet/mobile.
- [ ] No layout breakage (overlaps or overflow) at 768px.
