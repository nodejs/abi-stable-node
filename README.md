# Node.js API (Node-API)
This repository is the home for ABI Stable Node API project Node-API which was previously known as N-API.

The goal of this project is to provide a stable Node API for native
module developers. Node-API aims to provide ABI compatibility guarantees
across different Node versions and also across different Node
VMs - allowing Node-API enabled native modules to just work
across different versions and flavors of Node.js without recompilations.

It is introduced by this Node enhancement proposal:
[005-ABI-Stable-Module-API.md](https://github.com/nodejs/node-eps/blob/master/005-ABI-Stable-Module-API.md).

Node-API is part of [Node.js core](http://github.com/nodejs/node). Documentation is available here:
[https://nodejs.org/docs/latest/api/n-api.html](https://nodejs.org/docs/latest/api/n-api.html).

Node.js versions 8.12.0 and above provide Node-API as a stable feature.

## Branches

Currently this repo is being used only for meta issue management and
future planning by the [Node-API team](https://github.com/orgs/nodejs/teams/node-api). 

## API Design & Shape

The current shape of the API can be found in header file
[node_api.h](https://github.com/nodejs/node/blob/master/src/node_api.h).
Full documentation is available as part of the standard Node.js API docs here:
[https://nodejs.org/docs/latest/api/n-api.html](https://nodejs.org/docs/latest/api/n-api.html).

There is also a header-only [C++ API](https://github.com/nodejs/node-addon-api), which
simplifies development while still using the same ABI-stable Node API underneath.
It is distributed as a separate npm package: [https://www.npmjs.com/package/node-addon-api](https://www.npmjs.com/package/node-addon-api).

## Node-API enabled modules

|Module|Converted By|Location|Conversion Status|Performance Assessment|
|------|------------|--------|---|-----------|
|leveldown| n-api team | https://github.com/sampsongao/leveldown/tree/napi | Completed | [#55](https://github.com/nodejs/abi-stable-node/issues/55) |
|nanomsg| n-api team | https://github.com/sampsongao/node-nanomsg/tree/napi| Completed | [#57](https://github.com/nodejs/abi-stable-node/issues/57)|
|canvas| n-api team | https://github.com/jasongin/node-canvas/tree/napi | Completed | [#77](https://github.com/nodejs/abi-stable-node/issues/77)|
|node-sass| n-api team | https://github.com/boingoing/node-sass/tree/napi | Completed | [#82](https://github.com/nodejs/abi-stable-node/issues/82)|
|iotivity|[gabrielschulhof](https://github.com/gabrielschulhof) | https://github.com/gabrielschulhof/iotivity-node/tree/abi-stable | Completed |N/A|
|node-sqlite3 |n-api team | https://github.com/mhdawson/node-sqlite3/tree/node-addon-api | Completed | |

## Testing

In addition to running the tests in the converted modules we also have a converted version of the NAN examples
  [node-addon-examples](https://github.com/nodejs/node-addon-examples)

## How to get involved
* Convert a native module to use [Node-API](https://github.com/nodejs/abi-stable-node/blob/api-prototype-8.x/src/node_api.h) and report issues on conversion and performance;
* Port ABI stable APIs to your fork of Node and let us know if there are gaps;
* Review the [roadmap](https://github.com/nodejs/abi-stable-node/milestones) and see how you
can help accelerate this project.

## Badges

The use of badges is recommended to indicate the minimum version of Node-API
required for the module. This helps to determine which Node.js major versions
are supported. Addon maintainers can consult the [Node-API support matrix][] to
determine which Node.js versions provide a given Node-API version. The following
badges are available:

![Node-API v1 Badge](assets/Node-API%20v1%20Badge.svg)
![Node-API v2 Badge](assets/Node-API%20v2%20Badge.svg)
![Node-API v3 Badge](assets/Node-API%20v3%20Badge.svg)
![Node-API v4 Badge](assets/Node-API%20v4%20Badge.svg)
![Node-API v5 Badge](assets/Node-API%20v5%20Badge.svg)
![Node-API v6 Badge](assets/Node-API%20v6%20Badge.svg)
![Node-API v7 Badge](assets/Node-API%20v7%20Badge.svg)
![Node-API v8 Badge](assets/Node-API%20v8%20Badge.svg)
![Node-API v9 Badge](assets/Node-API%20v9%20Badge.svg)
![Node-API Experimental Version Badge](assets/Node-API%20Experimental%20Version%20Badge.svg)

## Meeting

The team meets once a week on zoom. See the **[Node.js Calendar](https://calendar.google.com/calendar/embed?src=nodejs.org_nr77ama8p7d7f9ajrpnu506c98%40group.calendar.google.com)** for the current time/day of the week.
The link to participate to the meeting is:  https://zoom.us/j/363665824 .

## Project Participants

### Active

| Name                | GitHub Link                                           |
| ----                | -----------                                           |
| Chengzhong Wu       | [legendecas](https://github.com/legendecas)           |
| Gabriel Schulhof    | [gabrielschulhof](https://github.com/gabrielschulhof) |
| Jack Xia            | [JckXia](https://github.com/JckXia)                   |
| Kevin Eady          | [KevinEady](https://github.com/KevinEady)             |
| Michael Dawson      | [mhdawson](https://github.com/mhdawson)               |
| Nicola Del Gobbo    | [NickNaso](https://github.com/NickNaso)               |
| Vladimir Morozov    | [vmoroz](https://github.com/vmoroz)                   |


### Emeritus
| Name                | GitHub Link                                           |
| ----                | -----------                                           |
| Anisha Rohra        | [anisha-rohra](https://github.com/anisha-rohra)       |
| Arunesh Chandra     | [aruneshchandra](https://github.com/aruneshchandra)   |
| Cory Mickelson      | [corymickelson](https://github.com/corymickelson)     |
| Hitesh Kanwathirtha | [digitalinfinity](https://github.com/digitalinfinity) |
| Ian Halliday        | [ianwjhalliday](https://github.com/ianwjhalliday)     |
| Jason Ginchereau    | [jasongin](https://github.com/jasongin)               |
| Jim Schlight        | [jschlight](https://github.com/jschlight)             |
| Kyle Farnung        | [kfarnung](https://github.com/kfarnung)               |
| Sampson Gao         | [sampsongao](https://github.com/sampsongao)           |
| Taylor Woll         | [boingoing](https://github.com/boingoing)             |

[Node-API support matrix]: https://nodejs.org/dist/latest/docs/api/n-api.html#n_api_node_api_version_matrix
