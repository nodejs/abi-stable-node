'use strict';
require('../../common');
var assert = require('assert');
var test_number = require('./build/Release/test_number');


// testing api calls for number
assert.equal(0, test_number.Test(0));
assert.equal(1, test_number.Test(1));
assert.equal(-1, test_number.Test(-1));
assert.equal(100, test_number.Test(100));
assert.equal(2121, test_number.Test(2121));
assert.equal(-1233, test_number.Test(-1233));
assert.equal(986583, test_number.Test(986583));
assert.equal(-976675, test_number.Test(-976675));

assert.equal(98765432213456789876546896323445679887645323232436587988766545658, 
  test_number.Test(98765432213456789876546896323445679887645323232436587988766545658));
assert.equal(-4350987086545760976737453646576078997096876957864353245245769809, 
  test_number.Test(-4350987086545760976737453646576078997096876957864353245245769809));

assert.equal(Number.MAX_SAFE_INTEGER, test_number.Test(Number.MAX_SAFE_INTEGER));
assert.equal(Number.MAX_SAFE_INTEGER + 10, test_number.Test(Number.MAX_SAFE_INTEGER + 10));
assert.equal(Number.MAX_VALUE, test_number.Test(Number.MAX_VALUE));
assert.equal(Number.MAX_VALUE + 10, test_number.Test(Number.MAX_VALUE + 10));

assert.equal(Number.POSITIVE_INFINITY, test_number.Test(Number.POSITIVE_INFINITY));
assert.equal(Number.NEGATIVE_INFINITY, test_number.Test(Number.NEGATIVE_INFINITY));
