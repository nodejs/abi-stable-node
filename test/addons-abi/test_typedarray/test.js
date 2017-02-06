'use strict';
require('../../common');
var assert = require('assert');

// Testing api calls for arrays
var test_typedarray = require('./build/Release/test_typedarray');

var byteArray = new Uint8Array(3);
byteArray[0] = 0;
byteArray[1] = 1;
byteArray[2] = 2;
assert.equal(byteArray.length, 3);

var doubleArray = new Float64Array(3);
doubleArray[0] = 0.0;
doubleArray[1] = 1.1;
doubleArray[2] = 2.2;
assert.equal(doubleArray.length, 3);

var byteResult = test_typedarray.Multiply(byteArray, 3);
assert.equal(byteResult.length, 3);
assert.equal(byteResult[0], 0);
assert.equal(byteResult[1], 3);
assert.equal(byteResult[2], 6);

var doubleResult = test_typedarray.Multiply(doubleArray, -3);
assert.equal(doubleResult.length, 3);
assert.equal(doubleResult[0], 0);
assert.equal(Math.round(10 * doubleResult[1]) / 10, -3.3);
assert.equal(Math.round(10 * doubleResult[2]) / 10, -6.6);
