# Node-API Build Tools

Unlike modules written in JavaScript, developing and deploying Node.js
native addons using Node-API requires an additional set of tools. Besides the
basic tools required to develop for Node.js, the native addon developer
requires a toolchain that can compile C and C++ code into a binary. In
addition, depending upon how the native addon is deployed, the _user_ of
the native addon will also need to have a C/C++ toolchain installed.

See the `node-gyp` [installation instructions][] for platform-specific
toolchain requirements.

If the addon is written in programming languages other than C/C++, please
refer to [language bindings](./node-api-engine-bindings.md) for their
documentation and toolchain requirements.

## Build tools

Both the tools listed here require that _users_ of the native
addon have a C/C++ toolchain installed in order to successfully install
the native addon.

### node-gyp

[node-gyp][] is a build tool based on the [gyp-next][] tool and comes
bundled with npm.

#### CMake.js

[CMake.js][] is an alternative build system based on [CMake][].

CMake.js is a good choice for projects that already use CMake.
[`build_with_cmake`][] is an example of a CMake-based native addon project.

### Uploading precompiled binaries

The three tools listed here permit native addon developers and maintainers
to create and upload binaries to public or private servers. These tools are
typically integrated with CI/CD build systems to build and upload binaries
for a variety of platforms and architectures. These binaries are then
available for download by users who do not need to have a C/C++ toolchain
installed.

#### node-pre-gyp

[node-pre-gyp][] is a tool based on node-gyp that adds the ability to
upload binaries to a server of the developer's choice. node-pre-gyp has
particularly good support for uploading binaries to Amazon S3.

#### prebuild

[prebuild][] is a tool that supports builds using either node-gyp or
CMake.js. Unlike node-pre-gyp which supports a variety of servers, prebuild
uploads binaries only to GitHub releases. prebuild is a good choice for
GitHub projects using CMake.js.

#### prebuildify

[prebuildify][] is a tool based on node-gyp. The advantage of prebuildify is
that the built binaries are bundled with the native addon when it's
uploaded to npm. The binaries are downloaded from npm and are immediately
available to the module user when the native addon is installed.

[`build_with_cmake`]: https://github.com/nodejs/node-addon-examples/tree/main/src/8-tooling/build_with_cmake
[CMake]: https://cmake.org
[CMake.js]: https://github.com/cmake-js/cmake-js
[gyp-next]: https://github.com/nodejs/gyp-next
[installation instructions]: https://github.com/nodejs/node-gyp#installation
[node-gyp]: https://github.com/nodejs/node-gyp
[node-pre-gyp]: https://github.com/mapbox/node-pre-gyp
[prebuild]: https://github.com/prebuild/prebuild
[prebuildify]: https://github.com/prebuild/prebuildify
