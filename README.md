
# SandScript Language

**SandScript** is a modern Interpreted widget programming language.

## 🚀 Quick Start

### Hello World Example

Create a file named `hello.sand`:

```
outputLine("Hello SandScript");
```

Run the Interpreter:

```
# Run your source code
QSandi hello.sand
```

## 🌐 HTTP requests

The built-in `httpGet(url)` and `httpPost(url, body)` functions make synchronous
HTTP/HTTPS requests and return the response body as a string. `httpPost` sends
its body as UTF-8 text. Requests time out after 30 seconds; invalid URLs and
network or HTTP errors are reported as runtime errors.

Qt6 Network support is enabled by default. To build without it, configure CMake
with `-DSAND_ENABLE_QT_NETWORK=OFF`.

## 📂 Project Structure

```
Sand/
├── sand-lang/       # Simple VS Code extention
├── src/             # Compiler and runtime source code
├── test/            # Test sourse
├── LICENSE          # MIT License
├── LICENSE-APACHE   # Apache License 2.0
├── SECURITY.md      # Security indrodution
├── writer.txt       # Writer infomation
├── .gitignore       # Git ignore
└── README.md        # Project introduction
```

## 🤝 Contributing

Issues and pull requests are welcome. You may help with bug fixes, feature implementation and language design.

All contributions must follow the **DCO (Developer Certificate of Origin)**.
Add the following line at the end of your commit message:

```
Signed-off-by: Syai-adivis 271877199@qq.com
```

## 📜 License (Dual License)

This project is available under either the MIT License or Apache License 2.0.
You may choose either license for your use, modification and distribution.

- [MIT License](./LICENSE) — Permissive simple license
- [Apache License 2.0](./LICENSE-APACHE) — Includes patent protection and modification notice requirements

Copyright (c) 2026 Syai
