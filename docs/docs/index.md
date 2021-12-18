---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Welcome

Welcome to Fragcolor documentation. Here, you can find the documentation of the technologies used with Fragcolor.

- [Chainblocks](./blocks/)
- [Contribution guide](./contribute/)
- [Built-in functions](./functions/)

--8<-- "includes/license.md"

Built on {{ (git.date or now()).strftime("%b %d, %Y at %H:%M:%S") }}{% if git.status %} from commit [{{ git.short_commit }}](https://github.com/fragcolor-xyz/chainblocks/commit/{{ git.commit }}){% endif %}.
