# Maintaining @nodejs/node-api-implementer

The [@nodejs/node-api-implementer][] team is used a method for the Node-api
team to community and share information with implementations of Node-api
outside of Node.js itself. It will be used to gather feedback on major changes
that may affect those implementations.

This document is a checklist of things to add a new person to the
[@nodejs/node-api-implementer][] team.

While adding new Node-API implementer to the team is encouraged
and welcomed, the following are a set of principles of the team:

1. The person must be a maintainer of a Node-API runtime implementation.
1. The Node-API runtime implementation must be added to the
   [Node-API bindings for other runtimes][] document.
1. The Node-API runtime should be publicly available.
1. The Node-API runtime should be actively maintained in 6 months.

To request joining the team, a person should open a PR at
https://github.com/nodejs/abi-stable-node/pulls to add their Node-API
runtime implementation to the [Node-API bindings for other runtimes][] document.

Onboarding a new person to the team doesn't suggest their work is sponsored
by the [Node-API team][]. The Node-API team is not responsible for maintaining
the runtime implementation.

## Offboarding

In order to keep the team effective, the [@nodejs/node-api-implementer][] team
maintainers are responsible to regularly review the team members.

This following is a checklist of things whether a person should be
offboard from the [@nodejs/node-api-implementer][] team. The criteria
should be determined by the discretion of the
[@nodejs/node-api-implementer][] team maintainers.

1. The person is no longer a maintainer of a Node-API runtime implementation.
1. The Node-API runtime implementation is not actively maintained in 6 months.

Offboarding a person from the team doesn't suggest their work is disregarded
by the Node-API team. If circumstances change, the person can be onboarded
again.

[Node-API team]: https://github.com/orgs/nodejs/teams/node-api
[@nodejs/node-api-implementer]: https://github.com/orgs/nodejs/teams/node-api-implementer
[Node-API bindings for other runtimes]: ./node-api-engine-bindings.md#node-api-bindings-for-other-runtimes
