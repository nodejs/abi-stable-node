# Node.js API (N-API)
This repository is the home for ABI Stable Node API project (N-API).
The goal of this project is to provide a stable Node API for native
module developers. N-API aims to provide ABI compatibility guarantees
across different Node versions and also across different Node
VMs - allowing N-API enabled native modules to just work
across different versions and flavors of Node.js without recompilations.

It is introduced by this Node enhancement proposal:
[005-ABI-Stable-Module-API.md](https://github.com/nodejs/node-eps/blob/master/005-ABI-Stable-Module-API.md).

N-API is now part of core. Documentation is available here:
[https://nodejs.org/docs/latest/api/n-api.html](https://nodejs.org/docs/latest/api/n-api.html).

As of Node.js version 8.6.0 N-API is enabled by default. If you use an N-API enabled module you
will be warned that it is experimental as follows:

```
(node:16761) Warning: N-API is an experimental feature and could change at any time.
```

Node.js versions 8.12.0 and above provide N-API as a stable feature.

## Branches

Currently this repo is being used only for meta issue management and
future planning by the [N-API team](https://github.com/orgs/nodejs/teams/n-api). All branches can be considered `stale` as they are no longer
being maintained. Updates and changes to N-API are being done
in the core [repo](http://github.com/nodejs/node).

## API Design & Shape

The current shape of the API can be found in header file
[node_api.h](https://github.com/nodejs/node/blob/master/src/node_api.h).
Full documentation is available as part of the standard Node.js API docs here:
[https://nodejs.org/docs/latest/api/n-api.html](https://nodejs.org/docs/latest/api/n-api.html).

There is also a header-only [C++ API](https://github.com/nodejs/node-addon-api), which
simplifies development while still using the same ABI-stable Node API underneath.
It is distributed as a separate npm package: [https://www.npmjs.com/package/node-addon-api](https://www.npmjs.com/package/node-addon-api).

## N-API enabled modules

|Module|Converted By|Location|Conversion Status|Performance Assessment|
|------|------------|--------|---|-----------|
|leveldown| n-api team | https://github.com/sampsongao/leveldown/tree/napi | Completed | [#55](https://github.com/nodejs/abi-stable-node/issues/55) |
|nanomsg| n-api team | https://github.com/sampsongao/node-nanomsg/tree/napi| Completed | [#57](https://github.com/nodejs/abi-stable-node/issues/57)|
|canvas| n-api team | https://github.com/jasongin/node-canvas/tree/napi | Completed | [#77](https://github.com/nodejs/abi-stable-node/issues/77)|
|node-sass| n-api team | https://github.com/boingoing/node-sass/tree/napi | Completed | [#82](https://github.com/nodejs/abi-stable-node/issues/82)|
|iotivity|[gabrielschulhof](https://github.com/gabrielschulhof) | https://github.com/gabrielschulhof/iotivity-node/tree/abi-stable | Completed |N/A|
|node-sqlite3 |n-api team | https://github.com/mhdawson/node-sqlite3/tree/node-addon-api | Completed | |

## Testing

In addition to running the tests in the converted modules we also have

* a converted version of the NAN examples
  [node-addon-examples](https://github.com/nodejs/node-addon-examples)

* a converted version of the [core addons tests](https://github.com/nodejs/node/tree/master/test/addons-napi) which can be run with `make test addons-napi`

## How to get involved
* Convert a native module to use [N-API](https://github.com/nodejs/abi-stable-node/blob/api-prototype-8.x/src/node_api.h) and report issues on conversion and performance;
* Port ABI stable APIs to your fork of Node and let us know if there are gaps;
* Review the [roadmap](https://github.com/nodejs/abi-stable-node/milestones) and see how you
can help accelerate this project.

## Badges

The use of badges is recommended to indicate the minimum version of N-API
required for the module. This helps to determine which Node.js major versions
are supported. Addon maintainers can consult the [N-API support matrix][] to
determine which Node.js versions provide a given N-API version. The following
badges are available:

![N-API v1 Badge](assets/N-API%20v1%20Badge.svg)
![N-API v2 Badge](assets/N-API%20v2%20Badge.svg)
![N-API v3 Badge](assets/N-API%20v3%20Badge.svg)
![N-API v4 Badge](assets/N-API%20v4%20Badge.svg)
![N-API v5 Badge](assets/N-API%20v5%20Badge.svg)
![N-API Experimental Version Badge](assets/N-API%20Experimental%20Version%20Badge.svg)

## Meeting

Every week the team has a scheduled meeting that usually happen on Monday at 
08:00 AM of the Pacific Time (PT). For more informations about it see at **[Node.js Calendar](https://calendar.google.com/calendar/embed?src=nodejs.org_nr77ama8p7d7f9ajrpnu506c98%40group.calendar.google.com)**. 
The link to participate to the meeting is:  https://zoom.us/j/363665824 .

## Project Participants

### Active

| Name                | GitHub Link                                           |
| ----                | -----------                                           |
| Arunesh Chandra     | [aruneshchandra](https://github.com/aruneshchandra)   |
| Taylor Woll         | [boingoing](https://github.com/boingoing)             |
| Hitesh Kanwathirtha | [digitalinfinity](https://github.com/digitalinfinity) |
| Gabriel Schulhof    | [gabrielschulhof](https://github.com/gabrielschulhof) |
| Jim Schlight        | [jschlight](https://github.com/jschlight)             |
| Michael Dawson      | [mhdawson](https://github.com/mhdawson)               |
| Nicola Del Gobbo    | [NickNaso](https://github.com/NickNaso)               |
| Anisha Rohra        | [anisha-rohra](https://github.com/anisha-rohra)       |
| Kyle Farnung        | [kfarnung](https://github.com/kfarnung)               |
| Kevin Eady          | [KevinEady](https://github.com/KevinEady)             |

### Emeritus
| Name                | GitHub Link                                           |
| ----                | -----------                                           |
| Cory Mickelson      | [corymickelson](https://github.com/corymickelson)     |
| Ian Halliday        | [ianwjhalliday](https://github.com/ianwjhalliday)     |
| Jason Ginchereau    | [jasongin](https://github.com/jasongin)               |
| Sampson Gao         | [sampsongao](https://github.com/sampsongao)           |

[N-API support matrix]: https://nodejs.org/dist/latest/docs/api/n-api.html#n_api_n_api_version_matrix
