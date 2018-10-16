# N-API Working Group meeting

**Location**: Node+JS Interactive 2018, Collaborator Summit
**Date**: October 13, 2018

**Attendees**:
@addaleax
@codebytere
@digitalinfinity
@gabrielschulhof
@jschlight
@mafintosh
@mhdawson
@NickNaso
@ofrobots

**Agenda**:
- Talk about N-API API docs
- Discuss state of embedding APIs + electron APIs
- Discuss libuv story
- Discuss WASM modules

## N-API docs presentation by @NickNaso

- Want to generate N-API static doc website from the N-API documentation
- Why:
    - Evangelization
    - Different presentation for the docs
- Other than docs, want to add links to resources like slides, workshops etc

- Question: Where would this live? Options:
    - Drop github pages
    - Continue to do doxygen model
    - Do something like @NickNaso's model
- Question- why not have docs live with headers
    - Value in IDEs
    - Orthogonal to website conversation
- Feedback: Website needs api search
- Folks want md too
- Question: What should be source of truth?
- Question: What tool can we use to autogenerate docs? Doxygen doesn’t generate markdown
    - Needs research - next step
    - @addaleax- quick look indicates that doxygen supports makrdown

- **Outcome**: @NickNaso to investigate more and report back to the group

## Embedding APIs + N-API (led by @codebytere)

- Wishes:
    - Better docs on how to embed Node
    - N-API APIs to give embedders better control on how to embed

- State of the world:
	- Node::start - big block kind of method
    - Call `CreateEnvironment`/`LoadEnvironment` - but this has other implementation requirements like embedder now are responsible for other things

Feedback:
- @mhdawson: Want to be careful it's not v8 specific
- @gabrielschulhof: Environment is a fine abstraction for vm agnosticism
- Someone pointed out that @rubys has an MVP for embedders with create, execute and teardown APIs
- @addaleax:
    - Difficult to do this with Node
    - Current API is not generate
		- No separation between setting up Node and actually starting the user script - should be addressed
		- More generally: there are different levels at which data is stored in node
			- Process
			- Multiple threads per process
			- Multiple isolates per thread
			- Node environments - set of all node apis, per isolate but multiple ones
			- Context: Global object + JS built-ins (managed by v8)
			- Multiple contexts/environment
			- Normal case: only 1 of each - embedder scenario, this is not true - and Node has a ton of code backing it
			- Unclear what napi_env maps to
				- @gabrielschulhof: Whoever gives context to native addon loader, this is 1:1 mapping
				- @addaleax: Maybe fine for N-API but not sure about addons. Example, multiple contexts per environment
				- @gabrielschulhof: Writing examples to show addons how to be loaded multiple times in a node process
				- @addaleax: worried that we're repeating mistakes with the embedder API in the past - for example, we mix per process state setup/teardown with per environment/isolate setup and teardown. We need to tease this all apart before we provide such an API
				- @gabrielschulhof: another way of stating this is does `napi_env` represent a Node or do we need another datatype
				- @mhdawson: there might be more than just that datatype
				- @addaleax: exactly, multiple levels could each represent valid embedder use cases
				- @mhdawson: levels could be VM specific?
				- @gabrielschulhof: Not vm specific - `napi_env` is not a problem- however, this API is completely node specific and vm agnostic - the new data types that we add are completely nodejs specific
				- @addaleax: not necessarily- do other runtimes offer same level of separation?
				- @gabrielschulhof: opaque, so anyone could implement them however they chose
- @digitalinfinity - also scope for separating out Node and JS Engine use cases. @gabrielschulhof had the idea of moving N-API to a separate project and standardizing the API with TC39
- @mhdawson - so what is the way forward?
- @addaleax - move forward with the embedded API, tackle option parsing and then tease apart other APIs. Once we go through the exercise, we'll have enough data to design the N-API versions of these APIs. If we need embedder utility APIs, they can live in node-addon-api.
- @mhdawson: So how official should we make the existing one?
- @gabrielschulhof: Is there one level with the least amount of granularity that we might want to expose via N-API?
- @addaleax: Sounds like you still are asking about a big-bang one step utility function, not sure if I'm a fan of that, because we don’t know whether we would want to change it or not
- @gabrielschulhof: don’t need abi stability at front - for embedders having a stable C++ api is good enough compared to C API that's ABI stable
- Customers: electron, IIB, nw.js,
- @mhdawson: Defining use cases and driving testing makes sense
- @mhdawson: to summarize, we don’t have to do anything in N-API, we wait till we experiment with the current embedder API
- @codebytere: compared to what we have today, anything would be better
- @gabrielschulhof: Tangential question- if we clean up some of the startup code, would we help simplify the snapshot generation
- @addaleax: Yeah, maybe
- @mhdawson: Embedder WG going to be restarted, issue is under user feedback today (https://github.com/nodejs/user-feedback/issues/51)

**Outcome**: @addaleax to continue working on reasoning about existing APIs, @rubys to restart embedder WG

## N-API libuv discussion (led by @mafintosh)

- @mhdawson: We've updated docs to make it clear there are no guarantees with libuv - no stability if you use libuv directly. Anna has shown that we could make some of it stable, but no one working on this. Michael's perspective is that to get proper stability, we would need to wrap all of libuv but that doesn’t make sense. Incremental wrapping makes sense but we don’t have a sense of where that balance is. If there are key libuv things that we could add, we'd like that feedback
- @mafintosh: feedback - libuv has no stability guarantee but it's been for a while
- @gabrielschulhof: Good times might be over
- @mafintosh: N-API makes making writing modules easier but writing any interesting module needs libuv in practice so all fun goes away. When I used libuv, used timers + udp - less of everything else. Some fs stuff.
- @addaleax: have thoughts - still think we should push to make some subset of libuv stable. Addons use timers and check_idle_async pretty commonly
- @mafintosh- reason why I mentioned UDP is that fs is fine crossing into node, but high-traffic networking, crossing the JS boundary is expensive
- @addaleax: not the common use case
- @mafintosh- hitting similar issues in other cases like http2
- @gabrielschulhof: what precisely is slated to change in libuv? Just because major version changes doesn’t mean it'll be broken
- @mhdawson: No guarantee of stability so cant make assumptions
- @addaleax: no struct level abi guarantees but we should mark subset of APIs as abi stable. Pushback was that they want to reserve freedom to cleanup apis once in a while
- @mhdawson: Should we specifically consider wrapping timers
- @addaleax: Hesitant to add stuff that's not available in other runtimes
- @mhdawson: `setTimeout` is in other runtimes?
- @mafintosh: consensus is that its just how it is
- @gabrielschulhof: Not going to rebuild your module as long as libuv
- @mhdawson: Add your native module to CITGM

**Outcome** - No consensus on adding more libuv APIs to N-API.

## Electron/N-API issues (led by @mafintosh)

- @mafintosh: just doesn’t work on Windows today
- @codebytere: multiple things in electron don’t work in windows
- @mafintosh: Is that intentional or just a bug
- @codebytere: Bunch of issues
- @mafintosh: If that’s the case we should update the docs to reflect the reality
- @mhdawson Since today electron doesn’t use a version of node that actually ships, no N-API guarantees
- @codebytere: only true in 4, before we used whatever v8 was in node
- @mafintosh: many pains with native modules, install with npm want it to work in electron
- @codebytere: Priority for us so we'll fix that
- @gabrielschulhof: also issue with linking on windows because of node module name - might need a different loader interface for electron?
- @codebytere: haven't really heard about it so would love more info. N-API modules are expected to work but no one has bought to our attention that there are issues with them
- @mafintosh: things generally work on Unix, Windows you don’t have abi stability because you have to compile against electron headers
- @mhdawson Open issues (or any issues re electron), we'll cc Shelley on them
- @gabrielschulhof: This problem also goes away when we use unpatched Node

**Outcome**: @codebytere will come to N-API standup

## WASM/Native modules (led by @ofrobots)
@ofrobots:
- Thinking of kick-starting a team or effort to make WASM native modules a reality.
- Few use cases where N-API is not needed
- Not all use cases for N-API covered by WASM but there are some
- Canonical example: node-sass- every time new node version comes out, need to ship new binaries. It's pure computation that can have it ship as a native module
- Would be good to have a team to work through the challenges
    - Don’t have a good toolchain - emscripten is from a web perspective. If you want to use malloc, where would it come from? Emscripten- ship something like musl-c - this is not great, especially if you have multiple WASM modules
    - Would be cool if node provided an LLVM wrapper out of the box, variant of Emscripten for Node
    - Would be good to have a wasm version of musl-c that we can ship once

Questions:
- @gabrielschulhof: when you run a wasm binary, theres no concept of shared libraries- how could we avoid having two copies of libc
- @ofrobots: There is a concept of a table- wasm functions take a table of functions - host environment sets up this table- this is unexplored territory, no one has done much around this. If folks are interested, can start a team. You can reach js functionality from those tables but not sure of the full details. Right now there is quite a bit of bouncing involved but VMs are actively working on wasm to js not being a trampoline
- @mhdawson Out of time- one last question- is this awareness or do you want to fit this to N-API?
- @ofrobots: Gut feel is not aligned with N-API but aligned with native modules
- @gabrielschulhof: @digitalinfinity and I identified a use case where it is- if you have a N-API module, compiled to wasm, we just need to provide N-API use cases

**Outcome**: @ofrobots to open an issue
