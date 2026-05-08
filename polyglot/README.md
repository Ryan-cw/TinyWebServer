# Polyglot TinyWebServer

This directory contains four small, dependency-light implementations that serve the existing `root/` static assets.  They share the same command-line shape so you can compare language/runtime trade-offs without changing the demo pages.

| Language | Source | Build/Run |
| --- | --- | --- |
| Go | `go/server.go` | `go run ./polyglot/go/server.go --port 9006 --root root` |
| C++17 | `cpp/server.cpp` | `make polyglot-cpp && ./polyglot/cpp/server --port 9006 --root root` |
| Rust | `rust/server.rs` | `make polyglot-rust && ./polyglot/rust/server --port 9006 --root root` |
| Python 3 | `python/server.py` | `python3 polyglot/python/server.py --port 9006 --root root` |

All implementations:

- default to port `9006`;
- default to the repository's `root/` directory;
- serve `/` as `/welcome.html`;
- support `GET` and `HEAD` for static files;
- reject path traversal outside the configured static root.

The original C++ epoll/MySQL server remains available through the existing `make server` target.
