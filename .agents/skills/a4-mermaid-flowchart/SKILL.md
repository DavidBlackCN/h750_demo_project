---
name: a4-mermaid-flowchart
description: Create, improve, or render Mermaid flowcharts (.mmd) and SVGs in this project. Use for software, algorithm, module, and system workflows in NUEDC design reports that must preview in the official Mermaid VS Code extension, avoid Chinese text clipping, use the fixed zinc-light minimal style, and fit A4 landscape pages. Invoke the pretty-mermaid skill during execution.
---

# A4 Mermaid Flowcharts

Read and follow the `pretty-mermaid` Skill first. Use this Skill for the project-specific source, layout, and SVG requirements.

## Source Rules

1. Use official Mermaid flowchart syntax and save UTF-8 `.mmd` files.
2. Quote all Chinese labels and all labels containing parentheses, colons, slashes, commas, or text such as `H(s)` and `H(f)`:

   ```mermaid
   CALC["Calculate input amplitude from known H(s)"]
   MODE -->|"Basic mode"| BASIC
   subgraph LEARN["Unknown model learning"]
   ```

3. Do not use `<br/>`, HTML labels, bare labels with special syntax, or slash-only input/output shapes. Prefer short, single-line labels; one node represents one action or result.
4. Restrict node IDs to ASCII letters, digits, and underscores. Use `(["..."])` for start/end, `{"..."}` for decisions, and `["..."]` for ordinary nodes.
5. Do not write `classDef`, `class`, or `style` in `.mmd` unless the user explicitly requests color semantics. The fixed SVG theme owns the visual style.

## A4 Landscape Layout

1. Prefer `flowchart LR`. Organize 3 to 5 horizontal functional columns, such as system control, input/basic processing, learning/analysis, and output/execution. Use `direction TB` inside each column for 2 to 4 short nodes.
2. Use `subgraph` for stages or responsibility boundaries. Put the main flow across columns; avoid one long row of many nodes.
3. Target an SVG width-height ratio of `1.5:1` through `2.3:1`. If too wide, merge adjacent steps or stack steps inside a column. If too tall, reduce nesting and redundant nodes.
4. Preserve business meaning and data dependencies. Show learning-result-to-reconstruction/execution links explicitly, and label mode branches.

## Render And Check

From project root, run:

```powershell
node .\.agents\skills\a4-mermaid-flowchart\scripts\render_a4_svg.mjs .\diagram.mmd
```

- An optional second parameter sets the SVG path; otherwise it is written beside the source with the same basename.
- The script calls the `beautiful-mermaid` dependency from `pretty-mermaid`, outputs the fixed `zinc-light` style with `Microsoft YaHei`, and handles Windows `file://` imports.
- The script applies Chinese-width compensation only in memory. It does not alter `.mmd` sources and leaves no compensation characters in SVG text.
- Read its canvas size and ratio. Rework the layout and rerender when it reports a ratio outside the A4 landscape target.

Finish with:

```powershell
git diff --check
git status --short
```

Report the modified `.mmd`, generated `.svg`, canvas size/ratio, and whether validation was local rendering only. Do not build, flash, or alter unrelated project files for a flowchart request.
