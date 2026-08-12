# Licensing

This file is a pointer, not a license grant. It conveys no rights on its own.

There is no repository-wide license. Each bean is licensed independently, and
adopting one carries only that bean's terms:

| Bean | License | Text |
|---|---|---|
| `esp_webserver` | GPL-3.0-or-later | [esp_webserver/LICENSE](esp_webserver/LICENSE) |
| `roost_logging` | MIT | [roost_logging/LICENSE](roost_logging/LICENSE) |

Each bean also declares its license in its `library.json` and repeats it in its
source file headers. Where anything disagrees, the bean's own `LICENSE` file is
authoritative.

Beans are independently adoptable, so taking one does not pull in the terms of
any other. A single umbrella license would force every bean to the most
restrictive terms in the collection, which would deny an MIT-only build to beans
meant to be adopted freely.

Adding a bean means adding its `LICENSE` and a row here.
