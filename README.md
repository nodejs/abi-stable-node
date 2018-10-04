# Node.js API (N-API)
This repository is the home for ABI Stable Node API project (N-API).
The goal of this project is to provide a stable Node API for native
module developers. N-API aims to provide ABI compatibility guarantees
across different Node versions and also across different Node
VMs - allowing N-API enabled native modules to just work
across different versions and flavors of Node.js without recompilations.

It is introduced by this Node enhancement proposal:
[005-ABI-Stable-Module-API.md](https://github.com/nodejs/node-eps/blob/master/005-ABI-Stable-Module-API.md).

N-API is now part of core as an experimental feature. Documentation is available here:
[https://nodejs.org/docs/latest/api/n-api.html](https://nodejs.org/docs/latest/api/n-api.html).

As of Node.js version 8.6.0 N-API is enabled by default. If you use an N-API enabled module you
will be warned that it is experimental as follows:

```
(node:16761) Warning: N-API is an experimental feature and could change at any time.
```

**Branches**

Currently this repo is being used only for meta issue management and
future planning by the [N-API team](https://github.com/orgs/nodejs/teams/n-api). All branches can be considered `stale` as they are no longer
being maintained. Updates and changes to N-API are being done
in the core [repo](http://github.com/nodejs/node).

**API Design & Shape**

The current shape of the API can be found in header file
[node_api.h](https://github.com/nodejs/node/blob/master/src/node_api.h).
Full documentation is available as part of the standard Node.js API docs here:
[https://nodejs.org/docs/latest/api/n-api.html](https://nodejs.org/docs/latest/api/n-api.html).

There is also a header-only [C++ API](https://github.com/nodejs/node-addon-api), which
simplifies development while still using the same ABI-stable Node API underneath.
It is distributed as a separate npm package: [https://www.npmjs.com/package/node-addon-api](https://www.npmjs.com/package/node-addon-api).

**N-API enabled modules**

|Module|Converted By|Location|Conversion Status|Performance Assessment|
|------|------------|--------|---|-----------|
|leveldown| n-api team | https://github.com/sampsongao/leveldown/tree/napi | Completed | [#55](https://github.com/nodejs/abi-stable-node/issues/55) |
|nanomsg| n-api team | https://github.com/sampsongao/node-nanomsg/tree/napi| Completed | [#57](https://github.com/nodejs/abi-stable-node/issues/57)|
|canvas| n-api team | https://github.com/jasongin/node-canvas/tree/napi | Completed | [#77](https://github.com/nodejs/abi-stable-node/issues/77)|
|node-sass| n-api team | https://github.com/boingoing/node-sass/tree/napi | Completed | [#82](https://github.com/nodejs/abi-stable-node/issues/82)|
|iotivity|[gabrielschulhof](https://github.com/gabrielschulhof) | https://github.com/gabrielschulhof/iotivity-node/tree/abi-stable | Completed |N/A|
|node-sqlite3 |n-api team | https://github.com/mhdawson/node-sqlite3/tree/node-addon-api | Completed | |

**Testing**

In addition to running the tests in the converted modules.  We have also

* Converted version of the NAN examples
  [abi-stable-node-addon-examples](https://github.com/nodejs/abi-stable-node-addon-examples)

* Converted version of the [core addons tests](https://github.com/nodejs/abi-stable-node/tree/api-prototype-8.x/test/addons-napi) which can be run with `make test addons-napi`

**How to get involved**
* Convert a native module to use [N-API](https://github.com/nodejs/abi-stable-node/blob/api-prototype-8.x/src/node_api.h) and report issues on conversion and performance;
* Port ABI stable APIs to your fork of Node and let us know if there are gaps;
* Review the [roadmap](https://github.com/nodejs/abi-stable-node/issues/18) and see how can you
can help accelerate this project.

Hangout link for weekly standup
https://zoom.us/j/363665824   (PRIMARY)
https://plus.google.com/u/0/events/c0eevtrlajniu7h8cjrdk0f56c8?authkey=COH04YCalJS8Ug (backup)

# Project Participants
* [aruneshchandra](https://github.com/aruneshchandra) - Arunesh Chandra
* [boingoing](https://github.com/boingoing) - Taylor Woll
* [corymickelson](https://github.com/corymickelson) - Cory Mickelson
* [digitalinfinity](https://github.com/digitalinfinity) - Hitesh Kanwathirtha
* [gabrielschulhof](https://github.com/gabrielschulhof) - Gabriel Schulhof
* [ianwjhalliday](https://github.com/ianwjhalliday) - Ian Halliday
* [jasongin](https://github.com/jasongin) - Jason Ginchereau
* [jschlight](https://github.com/jschlight) - Jim Schlight
* [mhdawson](https://github.com/mhdawson) - Michael Dawson
* [NickNaso](https://github.com/NickNaso) - Nicola Del Gobbo
* [sampsongao](https://github.com/sampsongao) - Sampson Gao
* [anisha-rohra](https://github.com/anisha-rohra) - Anisha Rohra
* [kfarnung](https://github.com/kfarnung) - Kyle Farnung
