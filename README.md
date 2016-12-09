# Node.js API (NAPI)
This repository is the home for ABI Stable Node API project (NAPI). The goal of this
project is to provide a stable Node API for native module developers. NAPI aims
to provide ABI compatibility guarantees across different Node versions and also
across different Node VMs – allowing NAPI enabled native modules to just work
across different versions and flavors of Node.js without recompilations.

This project is in an early proof of concept stage. Check out our [roadmap](https://github.com/nodejs/abi-stable-node/issues/18)
for details on the remaining work.

**API Design & Shape**

For more details on design of the API, please check out the 
[node-eps](https://github.com/nodejs/node-eps/pull/20) covering this effort. 

The current shape of the API can be found in header file 
[node_jsvmapi.h](https://github.com/nodejs/abi-stable-node/blob/api-prototype-6.2.0/src/node_jsvmapi.h)

**Branches**

This repository contains node sources from Node versions 0.10, 0.12, 6.2 and
Node-ChakraCore version 7.0, with addition of ABI stable Node APIs. The branches
are named according to the node versions that have been enabled with NAPI support. 

**NAPI enabled modules**
* [leveldown](https://github.com/boingoing/leveldown/)
* [nanomsg](https://github.com/sampsongao/node-nanomsg)
* [sqllite3](https://github.com/mhdawson/node-sqlite3) - in progress
* [iotivity](https://github.com/gabrielschulhof/iotivity-node) - in progress

**Testing**

In addition to running the tests in the converted modules.  We have also 

* Converted version of the NAN examples that we always keep working
  [abi-stable-node-addon-examples](https://github.com/nodejs/abi-stable-node-addon-examples)

* Converted version of the [core addons tests](https://github.com/nodejs/abi-stable-node/tree/api-prototype-6.2.0/test/addons-abi) which can be run with ```make test addons-abi```

**How to get involved**
* Convert a native module to use [NAPI](https://github.com/nodejs/abi-stable-node/blob/api-prototype-6.2.0/src/node_jsvmapi.h) and report issues on conversion and performance;
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
* [mykmelez](https://github.com/mykmelez) - Myk Melez
* [sampsongao](https://github.com/sampsongao) - Sampson Gao
