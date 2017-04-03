'use strict';

const common = require('../../common');
const test_napi_status = require(`./build/${common.buildType}/test_napi_status`);
const assert = require('assert');

test_napi_status.createNapiError();
assert(test_napi_status.testNapiErrorCleanup(), "napi_status cleaned up for second call");
