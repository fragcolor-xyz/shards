---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Welcome

Welcome to Fragcolor documentation!

These documents will help you understand, use, and contribute to Fragcolor technologies.

You can start from a high-level topic below or use the navigation links at the top.

- [Shards](./shards/)
- [Built-in functions](./functions/)
- [Contributions guides](./contribute/)

*Use the search box if you're looking for something specific.*


--8<-- "includes/license.md"

Built on {{ (git.date or now()).strftime("%b %d, %Y at %H:%M:%S") }}{% if git.status %} from commit [{{ git.short_commit }}](https://github.com/fragcolor-xyz/shards/commit/{{ git.commit }}){% endif %}.
