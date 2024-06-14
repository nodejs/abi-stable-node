# Node-API

Node-API (formerly N-API) is an API for building native addons. It is
independent from the underlying JavaScript runtime (for example, V8) and is
maintained as part of Node.js itself. This API will be Application Binary
Interface (ABI) stable across versions of Node.js. It is intended to insulate
addons from changes in the underlying JavaScript engine and allow modules
compiled for one major version to run on later major versions of Node.js
without recompilation. The [ABI stability guide][] provides a more in-depth
explanation.

APIs exposed by Node-API are generally used to create and manipulate JavaScript
values. Concepts and operations generally map to ideas specified in the
**[ECMA262 Language Specification][]**.

Node-API is an ABI stable C interface and it allows the usage of the [FFI][]
(Foreign Function Interface) to implement bindings written in a programming
langunage different from C or C++.

There are many projects that implement Node-API bindings and allow to develop
a native addon for Node.js, but at the same time  Node-API has been implemented
in other JavaScript/TypeScript runtimes. In the following section there are
some of them.

- [Node-API bindings for other languages](#languages)
- [Node-API bindings for other runtimes](#runtimes)

**NOTE** The Node-API team has not tried out or validated the projects listed
below and does not endorse them in any way. Similarly, being listed below does
not indicate that those projects endorse Node.js or Node-API.
The lists are simply provided as a starting point for you to do your own evaluation.

<a name="#languages"></a>

## Node-API bindings for other languages

|Project | Programming language|
|--------|---------------------|
|[napi-rs](https://github.com/napi-rs/napi-rs)| Rust|
|[napi-sys](https://github.com/napi-rs/napi-sys)| Rust|
|[nodejs-sys](https://github.com/elmarx/nodejs-sys)| Rust|
|[neon](https://github.com/neon-bindings/neon)| Rust|
|[NodeSwift](https://github.com/kabiroberai/node-swift)| Swift|

There are other projects that are in early stage, in development or that
it's possible to use as example to understand how to use a programming
language to implement native addons for Node.js through Node-API:

|Project | Programming language|
|--------|---------------------|
|[napi-cs](https://github.com/EYHN/napi-cs)| C#|
|[node-api-dotnet](https://github.com/microsoft/node-api-dotnet)| C#|
|[swift-napi-bindings](https://github.com/LinusU/swift-napi-bindings)| Swift|
|[swift-node-addon-examples](https://github.com/LinusU/swift-node-addon-examples)| Swift|
|[napi-nim](https://github.com/andi23rosca/napi-nim)| Nim|
|[zig-napigen](https://github.com/cztomsik/zig-napigen)|Zig|
|[zig-nodejs-example](zig-nodejs-example)| Zig|
|[go-node-api](https://github.com/napi-bindings/go-node-api)| Go|

<a name="#runtimes"></a>

## Node-API bindings for other runtimes

|Project | Programming language|
|--------|---------------------|
|[bun](https://github.com/oven-sh/bun)| Zig|
|[deno](https://github.com/denoland/deno)| Rust|
|[electron](https://github.com/electron/electron)| C++|
|[emnapi](https://github.com/toyobayashi/emnapi)| C / JavaScript|
|[iotjs](https://github.com/jerryscript-project/iotjs)| C|
|[veil](https://github.com/lightsourceengine/veil)| C|

[ABI stability guide]: https://nodejs.org/en/docs/guides/abi-stability/
[ECMA262 Language Specification]: https://tc39.es/ecma262/
[FFI]: https://en.wikipedia.org/wiki/Foreign_function_interface
