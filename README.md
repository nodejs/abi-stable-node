# Node.js API (N-API)
This repository is the home for ABI Stable Node API project (N-API). The goal of this
project is to provide a stable Node API for native module developers. N-API aims
to provide ABI compatibility guarantees across different Node versions and also
across different Node VMs – allowing N-API enabled native modules to just work
across different versions and flavors of Node.js without recompilations.

It is introduced by this Node enhancemnet proposal:
[005-ABI-Stable-Module-API.md](https://github.com/nodejs/node-eps/blob/master/005-ABI-Stable-Module-API.md).

This is project is in experimental stage at the moment. During the latest [VM Summit](https://github.com/nodejs/vm/issues/4), we [discussed the progress](https://github.com/nodejs/abi-stable-node/blob/doc/VM%20Summit.pdf) of this project. The summit participants agreed to consider a PR for this feature as experimental in the upcoming Node.js version 8.0. You can checkout the [Notes from Node.js VM Summit](https://blogs.windows.com/msedgedev/2017/03/13/notes-from-nodejs-vm-summit/) for more details.

**Branches**

This repository contains node sources from Node versions 0.10, 0.12, 6.2, 8.x and
Node-ChakraCore version 7.x and 8.x with addition of ABI stable Node APIs. The branches
are named according to the node versions that have been enabled with N-API support. 
Recently updated branches are the following:
* [api-prototype-6.2.0](https://github.com/nodejs/abi-stable-node/tree/api-prototype-6.2.0)
* [api-prototype-8.x](https://github.com/nodejs/abi-stable-node/tree/api-prototype-8.x)
* [api-prototype-chakracore-8.x](https://github.com/nodejs/abi-stable-node/tree/api-prototype-chakracore-8.x)

Branches for older Node versions were to test the resiliency of N-APIs across different Node versions. The PR will be submitted using the [api-prototype-8.x](https://github.com/nodejs/abi-stable-node/tree/api-prototype-8.x) branch which is tracking the latest Node.js master.

**API Design & Shape**

The current shape of the API can be found in header file
[node_api.h](https://github.com/nodejs/abi-stable-node/blob/api-prototype-8.x/src/node_api.h)

There is also a header-only [C++ API](https://github.com/nodejs/node-api/blob/master/napi.h), which simplifies development while still using the same ABI-stable Node API underneath. It will be distributed as a separate npm package.

**N-API enabled modules**

|Module|Converted By|Location|Performance Assesment|
|------|------------|--------|-----------|
|leveldown|[boingoing](https://github.com/boingoing) / [ianwjhalliday](https://github.com/ianwjhalliday) / [jasongin](https://github.com/jasongin) | https://github.com/jasongin/leveldown/tree/napi | [#55](https://github.com/nodejs/abi-stable-node/issues/55)|
|nanomsg|[sampsongao](https://github.com/sampsongao) | https://github.com/sampsongao/node-nanomsg/tree/api-opaque-prototype | [#57](https://github.com/nodejs/abi-stable-node/issues/57)|
|canvas|[jasongin](https://github.com/jasongin) | https://github.com/jasongin/node-canvas/tree/napi | [#77](https://github.com/nodejs/abi-stable-node/issues/77)|
|node-sass|[boingoing](https://github.com/boingoing) / [jasongin](https://github.com/jasongin) | https://github.com/boingoing/node-sass/tree/napi | [#82](https://github.com/nodejs/abi-stable-node/issues/82)|
|iotivity|[gabrielschulhof](https://github.com/gabrielschulhof) | https://github.com/gabrielschulhof/iotivity-node/tree/abi-stable | N/A|


**Testing**

In addition to running the tests in the converted modules.  We have also

* Converted version of the NAN examples
  [abi-stable-node-addon-examples](https://github.com/nodejs/abi-stable-node-addon-examples)

* Converted version of the [core addons tests](https://github.com/nodejs/abi-stable-node/tree/api-prototype-8.x/test/addons-napi) which can be run with ```make test addons-napi```

**How to get involved**
* Convert a native module to use [N-API](https://github.com/nodejs/abi-stable-node/blob/api-prototype-8.x/src/node_api.h) and report issues on conversion and performance;
* Port ABI stable APIs to your fork of Node and let us know if there are gaps;
* Review the [roadmap](https://github.com/nodejs/abi-stable-node/issues/18) and see how can you
can help accelerate this project.

Hangout link for weekly standup
https://plus.google.com/u/0/events/c0eevtrlajniu7h8cjrdk0f56c8?authkey=COH04YCalJS8Ug

# Project Participants
* [aruneshchandra](https://github.com/aruneshchandra) - Arunesh Chandra
* [boingoing](https://github.com/boingoing) - Taylor Woll
* [digitalinfinity](https://github.com/digitalinfinity) - Hitesh Kanwathirtha
* [gabrielschulhof](https://github.com/gabrielschulhof) - Gabriel Schulhof
* [ianwjhalliday](https://github.com/ianwjhalliday) - Ian Halliday
* [jasongin](https://github.com/jasongin) - Jason Ginchereau
* [mhdawson](https://github.com/mhdawson) - Michael Dawson
* [sampsongao](https://github.com/sampsongao) - Sampson Gao
