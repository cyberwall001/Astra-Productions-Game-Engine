# Astra Studio UI guide

The brief: premium, minimal, "enhance the player experience" — a studio that looks like an
instrument, not a dashboard. Dark, quiet surfaces; one warm accent; generous spacing; no
decorative gradients, no drop shadows except where elevation is meaningful (modals,
hover lift on cards).

## Design tokens (`ui::Theme`)

| Token | Value | Use |
| --- | --- | --- |
| `bg` | `#09090B` | app background |
| `bgDeep` | `#060608` | vignettes, boot screen base |
| `surface` | `#121216` | cards |
| `surfaceRaised` | `#17171C` | inputs, chips, popups |
| `surfaceSunken` | `#0D0D11` | inset fields, log rows |
| `surfaceHover` | `#1C1C23` | hover state |
| `stroke` | `#26262E` | 1 px borders |
| `brand` | `#FF5F5A` | primary accent, active state, focus |
| `brandBright` | `#FF7A6C` | hover on brand surfaces |
| `brandDeep` | `#D63E3A` | pressed |
| `text` | `#F2F2F5` | primary text |
| `textDim` | `#A0A0AC` | secondary text, labels |
| `textFaint` | `#6A6A78` | meta text, timestamps |
| `success` | `#5FCF9E` | synced, saved |
| `warn` | `#F0B45E` | renderer offline, destructive hints |
| `danger` | `#F2585B` | errors, invalid states |
| `info` | `#6FA8DC` | demo workspace, neutral notice |
| `violet` | `#A98BE0` | ledger/owner accents |

Radii: frames `8`, cards `12–14`, popups `10`, pills fully rounded.
Heights: buttons `42–44`, fields `44`, toolbar chips `28–30`, panel headers `30`, rows `26`.
All metrics go through `U(px)` so a 125%/150% display scale stays proportional.

## Typography

Two families, no more:

* **Roboto Medium** — UI text. Sizes: 11.5 `tiny`, 13 `small`, 15 `body`, 16.5 `medium`,
  19 `h3`, 24 `h2`, 32 `h1`, 54 `display`.
* **Cousine Regular** — anything a user might copy or compare: access codes, file paths,
  console lines, ids, hex.

Letter-spacing is applied manually for the wordmark and section overlines
(`screens::SpacedText`). Overlines are uppercase, faint, tracked ~1.8 px.

## Branding

* Logo: `assets/branding/astra_logo.png` (user-supplied mark), decoded at runtime, drawn
  through `ui::branding::Logo()`; never redrawn as a vector approximation.
* Lockup: logo + `ASTRA` (tracked) with a second line `PRODUCTIONS` / `PROJECT HUB`.
* The logo is the only full-colour element on the launcher; everything else is monochrome
  plus the coral accent.
* Watermark: a large, low-opacity brand mark behind the launcher column. Decorative
  only; never behind form fields where it would hurt legibility.

## Component vocabulary (`ui::Widgets`)

| Component | Notes |
| --- | --- |
| `PrimaryButton` / `SecondaryButton` / `GhostButton` | 42–44 px tall, icon + label, disabled state is a muted coral, never grey-on-grey |
| `IconButton` | 28–30 px square, 1.5 px vector icon, tooltip required |
| `LinkButton` | text + icon, faint until hover |
| `TextField` / `PasswordField` / `SearchField` | 44 px, icon gutter on the left, error ring in `danger`, placeholder matches caret position |
| `Segmented` | 2–4 options, pill row, brand fill on the active item |
| `Select` | 40 px, chevron right, popup with 8 px padding |
| `Stepper` | `[-] value [+]`, used for small integer ranges (1–10 code batches) |
| `Toggle` | pill switch, brand when on |
| `Checkbox` | 18 px square, brand check |
| `Pill` / `Badge` | 22 px capsule with optional 12 px icon |
| `BeginCard` / `EndCard` | padded surface + 1 px border; padding is owned by the card (14 × 12) |
| `Overline` / `FieldLabel` / `KeyValue` | section and field typography |
| `InfoBanner` / `ErrorBanner` / `SuccessBanner` | 1 px tinted border, tinted icon, wrapped text |
| `Sparkline` | 46 px, padded range so flat data still reads as a line |
| `EmptyState` | icon + title + body + optional action, centred |
| `SkeletonBar` | loading placeholder with a moving highlight |
| `Avatar` | initials on a deterministic accent from the palette |
| `Tooltip` | 10 px radius, one line, 400 ms delay |
| `VSpace` / `HSpace` | explicit rhythm; no bare `Dummy` in screens |

Icons are hand-written 1.5 px stroke vectors in `ui/Icons.cpp` (~60 glyphs). They scale
with `U()` and recolour from a single `ImU32`, so nothing depends on an icon font.

## Layout rules

* **Launcher**: brand panel only above 900 px width (`panelWidth = clamp(0.42w, 360, 560)`).
  Forms are capped at 420 px and vertically centred.
* **Project hub**: sidebar only above 1040 px (`min(292, 0.30w)`). Repo grid columns =
  `width / 320`; cards are 168 px tall and uniform per row.
* **Editor**: menu 30 px + toolbar 46 px + status 26 px, 8 px gutters. Hierarchy
  `clamp(0.19w, 210, 280)`, inspector `clamp(0.23w, 268, 340)`, statistics
  `clamp(0.32h, 160, 230)`, console `clamp(0.30h, 140, 240)`.
* Panels are rectangles computed once per frame in `screens::Editor`; panels never move
  themselves. Every panel has the same 30 px header with icon, title and optional badge.
* **Property rows** are 26 px tall: label at 42% width (clipped, never ellipsised mid-word),
  value right-aligned in the remainder, unit in a reserved gutter. Vectors own two rows —
  label on the first, three wide fields on the second — because three 44 px fields clip
  values like `900.02`.

## Motion

* Screen transitions: 280 ms opaque curtain (`ui::Fade(bg, 1 - t)`), never alpha-multiplying
  the draw list (that double-blends overlapping geometry).
* Hover lift on cards: 2 px, no scale.
* Toasts: slide in from the top-right, 5 s, stacked to a maximum of four.
* No infinite decorative animation. The boot sequence and skeletons are the only loops, and
  both communicate progress.

## Rules of thumb

1. If a number can overflow, reserve its gutter (units, badges, right-aligned meta).
2. Never put a control inside a fixed-height card without counting its padding — cards own
   their padding, so content that fit at `size - 28` will overflow.
3. Prefer one accent per screen. The coral means "this is the action" or "this is live".
4. Every icon-only control needs a tooltip; every disabled control needs a reason nearby.
5. Copy is sentence case, present tense, no exclamation marks.
